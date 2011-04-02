/***************************************************************************
 *   Copyright (C) 1998-2009 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 *   Lux Renderer is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Lux Renderer is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   Lux Renderer website : http://www.luxrender.org                       *
 ***************************************************************************/

/*
 * FlexImage Film class
 *
 */

// Those includes must come first (before lux.h)
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>

#include "fleximage.h"
#include "cameraresponse.h"
#include "error.h"
#include "scene.h"		// for Scene
#include "filter.h"
#include "exrio.h"
#include "tgaio.h"
#include "pngio.h"
#include "osfunc.h"
#include "dynload.h"

#include <boost/thread/xtime.hpp>
#include <boost/filesystem/path.hpp>

using namespace lux;

// FlexImageFilm Method Definitions
FlexImageFilm::FlexImageFilm(u_int xres, u_int yres, Filter *filt, u_int filtRes, const float crop[4],
	const string &filename1, bool premult, int wI, int fwI, int dI, int cM,
	bool cw_EXR, OutputChannels cw_EXR_channels, bool cw_EXR_halftype, int cw_EXR_compressiontype, bool cw_EXR_applyimaging,
	bool cw_EXR_gamutclamp, bool cw_EXR_ZBuf, ZBufNormalization cw_EXR_ZBuf_normalizationtype,
	bool cw_PNG, OutputChannels cw_PNG_channels, bool cw_PNG_16bit, bool cw_PNG_gamutclamp, bool cw_PNG_ZBuf, ZBufNormalization cw_PNG_ZBuf_normalizationtype,
	bool cw_TGA, OutputChannels cw_TGA_channels, bool cw_TGA_gamutclamp, bool cw_TGA_ZBuf, ZBufNormalization cw_TGA_ZBuf_normalizationtype, 
	bool w_resume_FLM, bool restart_resume_FLM, int haltspp, int halttime,
	int p_TonemapKernel, float p_ReinhardPreScale, float p_ReinhardPostScale,
	float p_ReinhardBurn, float p_LinearSensitivity, float p_LinearExposure, float p_LinearFStop, float p_LinearGamma,
	float p_ContrastYwa, const string &p_response, float p_Gamma,
	const float cs_red[2], const float cs_green[2], const float cs_blue[2], const float whitepoint[2],
	int reject_warmup, bool debugmode, int outlierk) :
	Film(xres, yres, filt, filtRes, crop, filename1, premult, cw_EXR_ZBuf || cw_PNG_ZBuf || cw_TGA_ZBuf, w_resume_FLM, 
		restart_resume_FLM, haltspp, halttime, reject_warmup, debugmode, outlierk), 
	framebuffer(NULL), float_framebuffer(NULL), alpha_buffer(NULL), z_buffer(NULL),
	writeInterval(wI), flmWriteInterval(fwI), displayInterval(dI)
{
	colorSpace = ColorSystem(cs_red[0], cs_red[1], cs_green[0], cs_green[1], cs_blue[0], cs_blue[1], whitepoint[0], whitepoint[1], 1.f);

	// Set Image Output parameters
	clampMethod = cM;

	write_EXR = cw_EXR;
	AddBoolAttribute(*this, "write_EXR", "Write EXR image", write_EXR, &FlexImageFilm::write_EXR, Queryable::ReadWriteAccess);
	write_EXR_halftype = cw_EXR_halftype;
	AddBoolAttribute(*this, "write_EXR_halftype", "Write EXR half-float (16bit per channel)", write_EXR_halftype, &FlexImageFilm::write_EXR_halftype, Queryable::ReadWriteAccess);
	write_EXR_applyimaging = cw_EXR_applyimaging;
	AddBoolAttribute(*this, "write_EXR_applyimaging", "Write EXR with tonemapping", write_EXR_applyimaging, &FlexImageFilm::write_EXR_applyimaging, Queryable::ReadWriteAccess);
	write_EXR_gamutclamp = cw_EXR_gamutclamp;
	AddBoolAttribute(*this, "write_EXR_gamutclamp", "Clamp out of gamut colors in EXR", write_EXR_gamutclamp, &FlexImageFilm::write_EXR_gamutclamp, Queryable::ReadWriteAccess);
	write_EXR_ZBuf = cw_EXR_ZBuf;
	AddBoolAttribute(*this, "write_EXR_ZBuf", "Write EXR with Z-buffer", write_EXR_ZBuf, &FlexImageFilm::write_EXR_ZBuf, Queryable::ReadWriteAccess);
	write_EXR_channels = cw_EXR_channels;
	AddIntAttribute(*this, "write_EXR_channels", "Channels to add to EXR image { 0: Y, 1: YA, 2: RGB, 3: RGBA }", 0, &FlexImageFilm::write_EXR_channels, Queryable::ReadWriteAccess);
	write_EXR_compressiontype = cw_EXR_compressiontype;
	// AddIntAttribute(*this, "write_EXR_compressiontype", "EXR compression type { 0: RLE, 1: PIZ, 2: ZIP, 3: Pxr24 (lossy), 4: None }", 0, &FlexImageFilm::write_EXR_compressiontype, Queryable::ReadWriteAccess);
	write_EXR_ZBuf_normalizationtype = cw_EXR_ZBuf_normalizationtype;
	// AddIntAttribute(*this, "write_EXR_ZBuf_normalizationtype", "EXR Z-buffer normalization type { 0: None, 1: Camera Start/End, 2: Min/Max }", 0, &FlexImageFilm::write_EXR_ZBuf_normalizationtype, Queryable::ReadWriteAccess);

	write_PNG = cw_PNG;
	write_PNG_16bit = cw_PNG_16bit;
	write_PNG_gamutclamp = cw_PNG_gamutclamp;
	write_PNG_ZBuf = cw_PNG_ZBuf;
	write_PNG_ZBuf_normalizationtype = cw_PNG_ZBuf_normalizationtype;
	write_PNG_channels = cw_PNG_channels;

	write_TGA = cw_TGA;
	write_TGA_gamutclamp = cw_TGA_gamutclamp;
	write_TGA_ZBuf = cw_TGA_ZBuf;
	write_TGA_channels = cw_TGA_channels;
	write_TGA_ZBuf_normalizationtype = cw_TGA_ZBuf_normalizationtype;

	AddIntAttribute(*this, "displayInterval", "Display interval (seconds)", displayInterval, &FlexImageFilm::displayInterval, Queryable::ReadWriteAccess);
	AddIntAttribute(*this, "writeInterval", "Output file write interval (seconds)", writeInterval, &FlexImageFilm::writeInterval, Queryable::ReadWriteAccess);
	AddIntAttribute(*this, "flmWriteInterval", "Output FLM file write interval (seconds)", flmWriteInterval, &FlexImageFilm::flmWriteInterval, Queryable::ReadWriteAccess);

	// Set use and default runtime changeable parameters
	m_TonemapKernel = d_TonemapKernel = p_TonemapKernel;
	AddIntAttribute(*this, "TonemapKernel", "Tonemap kernel type: {0: Reinhard, 1: Linear, 2: Contrast, 3: MaxWhite, 4: AutoLinear}", 0, &FlexImageFilm::m_TonemapKernel, Queryable::ReadWriteAccess);

	m_ReinhardPreScale = d_ReinhardPreScale = p_ReinhardPreScale;
	AddFloatAttribute(*this, "ReinhardPreScale", "Reinhard pre-scale", &FlexImageFilm::m_ReinhardPreScale, Queryable::ReadWriteAccess);
	m_ReinhardPostScale = d_ReinhardPostScale = p_ReinhardPostScale;
	AddFloatAttribute(*this, "ReinhardPostScale", "Reinhard post-scale", &FlexImageFilm::m_ReinhardPostScale, Queryable::ReadWriteAccess);
	m_ReinhardBurn = d_ReinhardBurn = p_ReinhardBurn;
	AddFloatAttribute(*this, "ReinhardBurn", "Reinhard burn", &FlexImageFilm::m_ReinhardBurn, Queryable::ReadWriteAccess);

	m_LinearSensitivity = d_LinearSensitivity = p_LinearSensitivity;
	AddFloatAttribute(*this, "LinearSensitivity", "Linear ISO sensitivity", &FlexImageFilm::m_LinearSensitivity, Queryable::ReadWriteAccess);
	m_LinearExposure = d_LinearExposure = p_LinearExposure;
	AddFloatAttribute(*this, "LinearExposure", "Linear exposure (sec)", &FlexImageFilm::m_LinearExposure, Queryable::ReadWriteAccess);
	m_LinearFStop = d_LinearFStop = p_LinearFStop;
	AddFloatAttribute(*this, "LinearFStop", "Linear f/Stop", &FlexImageFilm::m_LinearFStop, Queryable::ReadWriteAccess);
	m_LinearGamma = d_LinearGamma = p_LinearGamma;
	AddFloatAttribute(*this, "LinearGamma", "Linear gamma", &FlexImageFilm::m_LinearGamma, Queryable::ReadWriteAccess);

	m_ContrastYwa = d_ContrastYwa = p_ContrastYwa;
	AddFloatAttribute(*this, "ContrastYwa", "Contrast world-adaptation luminance", &FlexImageFilm::m_ContrastYwa, Queryable::ReadWriteAccess);

	m_RGB_X_White = d_RGB_X_White = whitepoint[0];
	AddFloatAttribute(*this, "RGB_X_White", "Colourspace: white point X", &FlexImageFilm::m_RGB_X_White, Queryable::ReadWriteAccess);
	m_RGB_Y_White = d_RGB_Y_White = whitepoint[1];
	AddFloatAttribute(*this, "RGB_Y_White", "Colourspace: white point Y", &FlexImageFilm::m_RGB_Y_White, Queryable::ReadWriteAccess);
	m_RGB_X_Red = d_RGB_X_Red = cs_red[0];
	AddFloatAttribute(*this, "RGB_X_Red", "Colourspace: red X", &FlexImageFilm::m_RGB_X_Red, Queryable::ReadWriteAccess);
	m_RGB_Y_Red = d_RGB_Y_Red = cs_red[1];
	AddFloatAttribute(*this, "RGB_Y_Red", "Colourspace: red Y", &FlexImageFilm::m_RGB_Y_Red, Queryable::ReadWriteAccess);
	m_RGB_X_Green = d_RGB_X_Green = cs_green[0];
	AddFloatAttribute(*this, "RGB_X_Green", "Colourspace: green X", &FlexImageFilm::m_RGB_X_Green, Queryable::ReadWriteAccess);
	m_RGB_Y_Green = d_RGB_Y_Green = cs_green[1];
	AddFloatAttribute(*this, "RGB_Y_Green", "Colourspace: green Y", &FlexImageFilm::m_RGB_Y_Green, Queryable::ReadWriteAccess);
	m_RGB_X_Blue = d_RGB_X_Blue = cs_blue[0];
	AddFloatAttribute(*this, "RGB_X_Blue", "Colourspace: blue X", &FlexImageFilm::m_RGB_X_Blue, Queryable::ReadWriteAccess);
	m_RGB_Y_Blue = d_RGB_Y_Blue = cs_blue[1];
	AddFloatAttribute(*this, "RGB_Y_Blue", "Colourspace: blue Y", &FlexImageFilm::m_RGB_Y_Blue, Queryable::ReadWriteAccess);
	m_Gamma = d_Gamma = p_Gamma;
	AddFloatAttribute(*this, "Gamma", "Film gamma", &FlexImageFilm::m_Gamma, Queryable::ReadWriteAccess);

	m_BloomUpdateLayer = false;
	m_BloomDeleteLayer = false;
	m_HaveBloomImage = false;
	m_BloomRadius = d_BloomRadius = 0.07f;
	AddFloatAttribute(*this, "BloomRadius", "Bloom radius", &FlexImageFilm::m_BloomRadius, Queryable::ReadWriteAccess);
	m_BloomWeight = d_BloomWeight = 0.25f;
	AddFloatAttribute(*this, "BloomWeight", "Bloom weight", &FlexImageFilm::m_BloomWeight, Queryable::ReadWriteAccess);

	m_VignettingEnabled = d_VignettingEnabled = false;
	m_VignettingScale = d_VignettingScale = 0.4f;
	AddFloatAttribute(*this, "VignettingScale", "Vignetting scale", &FlexImageFilm::m_VignettingScale, Queryable::ReadWriteAccess);

	m_AberrationEnabled = d_AberrationEnabled = false;
	m_AberrationAmount = d_AberrationAmount = 0.005f;
	AddFloatAttribute(*this, "AberrationAmount", "Chromatic abberation amount", &FlexImageFilm::m_AberrationAmount, Queryable::ReadWriteAccess);

	m_GlareUpdateLayer = false;
	m_GlareDeleteLayer = false;
	m_HaveGlareImage = false;
	m_glareImage = NULL;
	m_bloomImage = NULL;
	m_GlareAmount = d_GlareAmount = 0.03f;
	AddFloatAttribute(*this, "GlareAmount", "Glare amount", &FlexImageFilm::m_GlareAmount, Queryable::ReadWriteAccess);
	m_GlareRadius = d_GlareRadius = 0.03f;
	AddFloatAttribute(*this, "GlareRadius", "Glare radius", &FlexImageFilm::m_GlareRadius, Queryable::ReadWriteAccess);
	m_GlareBlades = d_GlareBlades = 3;
	AddIntAttribute(*this, "GlareBlades", "Glare blades", &FlexImageFilm::m_GlareBlades, Queryable::ReadWriteAccess);
	m_GlareThreshold = d_GlareThreshold = .5f;
	AddFloatAttribute(*this, "GlareThreshold", "Glare threshold", &FlexImageFilm::m_GlareThreshold, Queryable::ReadWriteAccess);

	m_HistogramEnabled = d_HistogramEnabled = false;

	m_GREYCStorationParams.Reset();
	d_GREYCStorationParams.Reset();

	m_chiuParams.Reset();
	d_chiuParams.Reset();

	m_CameraResponseFile = d_CameraResponseFile = p_response;
	AddStringAttribute(*this, "CameraResponse", "Path to camera response data file", "", &FlexImageFilm::m_CameraResponseFile, Queryable::ReadWriteAccess);
	m_CameraResponseEnabled = d_CameraResponseEnabled = m_CameraResponseFile != "";

	// init timer
	boost::xtime_get(&lastWriteImageTime, boost::TIME_UTC);
	lastWriteFLMTime = lastWriteImageTime;
}

// Parameter Access functions
void FlexImageFilm::SetParameterValue(luxComponentParameters param, double value, u_int index)
{
	 switch (param) {
		case LUX_FILM_TM_TONEMAPKERNEL:
			m_TonemapKernel = Floor2Int(value);
			break;

		case LUX_FILM_TM_REINHARD_PRESCALE:
			m_ReinhardPreScale = value;
			break;
		case LUX_FILM_TM_REINHARD_POSTSCALE:
			m_ReinhardPostScale = value;
			break;
		case LUX_FILM_TM_REINHARD_BURN:
			m_ReinhardBurn = value;
			break;

		case LUX_FILM_TM_LINEAR_SENSITIVITY:
			m_LinearSensitivity = value;
			break;
		case LUX_FILM_TM_LINEAR_EXPOSURE:
			m_LinearExposure = value;
			break;
		case LUX_FILM_TM_LINEAR_FSTOP:
			m_LinearFStop = value;
			break;
		case LUX_FILM_TM_LINEAR_GAMMA:
			m_LinearGamma = value;
			break;

		case LUX_FILM_TM_CONTRAST_YWA:
			m_ContrastYwa = value;
			break;

		case LUX_FILM_TORGB_X_WHITE:
			m_RGB_X_White = value;
			break;
		case LUX_FILM_TORGB_Y_WHITE:
			m_RGB_Y_White = value;
			break;
		case LUX_FILM_TORGB_X_RED:
			m_RGB_X_Red = value;
			break;
		case LUX_FILM_TORGB_Y_RED:
			m_RGB_Y_Red = value;
			break;
		case LUX_FILM_TORGB_X_GREEN:
			m_RGB_X_Green = value;
			break;
		case LUX_FILM_TORGB_Y_GREEN:
			m_RGB_Y_Green = value;
			break;
		case LUX_FILM_TORGB_X_BLUE:
			m_RGB_X_Blue = value;
			break;
		case LUX_FILM_TORGB_Y_BLUE:
			m_RGB_Y_Blue = value;
			break;
		case LUX_FILM_TORGB_GAMMA:
			m_Gamma = value;
			break;

		case LUX_FILM_CAMERA_RESPONSE_ENABLED:
			m_CameraResponseEnabled = (value != 0.f);
			break;

		case LUX_FILM_UPDATEBLOOMLAYER:
			m_BloomUpdateLayer = (value != 0.f);
			break;
		case LUX_FILM_DELETEBLOOMLAYER:
			m_BloomDeleteLayer = (value != 0.f);
			break;

		case LUX_FILM_BLOOMRADIUS:
			 m_BloomRadius = value;
			break;
		case LUX_FILM_BLOOMWEIGHT:
			 m_BloomWeight = value;
			break;

		case LUX_FILM_VIGNETTING_ENABLED:
			m_VignettingEnabled = (value != 0.f);
			break;
		case LUX_FILM_VIGNETTING_SCALE:
			 m_VignettingScale = value;
			break;

		case LUX_FILM_ABERRATION_ENABLED:
			m_AberrationEnabled = (value != 0.f);
			break;
		case LUX_FILM_ABERRATION_AMOUNT:
			 m_AberrationAmount = value;
			break;

		case LUX_FILM_UPDATEGLARELAYER:
			m_GlareUpdateLayer = (value != 0.f);
			break;
		case LUX_FILM_DELETEGLARELAYER:
			m_GlareDeleteLayer = (value != 0.f);
			break;
		case LUX_FILM_GLARE_AMOUNT:
			m_GlareAmount = value;
			break;
		case LUX_FILM_GLARE_RADIUS:
			m_GlareRadius = value;
			break;
		case LUX_FILM_GLARE_BLADES:
			m_GlareBlades = Round2UInt(value);
			break;
		case LUX_FILM_GLARE_THRESHOLD:
			m_GlareThreshold = value;
			break;

		case LUX_FILM_HISTOGRAM_ENABLED:
			m_HistogramEnabled = (value != 0.f);
			break;

		case LUX_FILM_NOISE_CHIU_ENABLED:
			m_chiuParams.enabled = (value != 0.f);
			break;
		case LUX_FILM_NOISE_CHIU_RADIUS:
			m_chiuParams.radius = value;
			break;
		case LUX_FILM_NOISE_CHIU_INCLUDECENTER:
			m_chiuParams.includecenter = (value != 0.f);
			break;

		case LUX_FILM_NOISE_GREYC_ENABLED:
			m_GREYCStorationParams.enabled = (value != 0.f);
			break;
		case LUX_FILM_NOISE_GREYC_AMPLITUDE:
			m_GREYCStorationParams.amplitude = value;
			break;
		case LUX_FILM_NOISE_GREYC_NBITER:
			m_GREYCStorationParams.nb_iter = Round2UInt(value);
			break;
		case LUX_FILM_NOISE_GREYC_SHARPNESS:
			m_GREYCStorationParams.sharpness = value;
			break;
		case LUX_FILM_NOISE_GREYC_ANISOTROPY:
			m_GREYCStorationParams.anisotropy = value;
			break;
		case LUX_FILM_NOISE_GREYC_ALPHA:
			m_GREYCStorationParams.alpha = value;
			break;
		case LUX_FILM_NOISE_GREYC_SIGMA:
			m_GREYCStorationParams.sigma = value;
			break;
		case LUX_FILM_NOISE_GREYC_FASTAPPROX:
			m_GREYCStorationParams.fast_approx = (value != 0.f);
			break;
		case LUX_FILM_NOISE_GREYC_GAUSSPREC:
			m_GREYCStorationParams.gauss_prec = value;
			break;
		case LUX_FILM_NOISE_GREYC_DL:
			m_GREYCStorationParams.dl = value;
			break;
		case LUX_FILM_NOISE_GREYC_DA:
			m_GREYCStorationParams.da = value;
			break;
		case LUX_FILM_NOISE_GREYC_INTERP:
			m_GREYCStorationParams.interp = Round2UInt(value);
			break;
		case LUX_FILM_NOISE_GREYC_TILE:
			m_GREYCStorationParams.tile = Round2UInt(value);
			break;
		case LUX_FILM_NOISE_GREYC_BTILE:
			m_GREYCStorationParams.btile = Round2UInt(value);
			break;
		case LUX_FILM_NOISE_GREYC_THREADS:
			m_GREYCStorationParams.threads = Round2UInt(value);
			break;

		case LUX_FILM_LG_SCALE:
			SetGroupScale(index, value);
			break;
		case LUX_FILM_LG_ENABLE:
			SetGroupEnable(index, value != 0.f);
			break;
		case LUX_FILM_LG_SCALE_RED: {
			RGBColor color(GetGroupRGBScale(index));
			color.c[0] = value;
			SetGroupRGBScale(index, color);
			break;
		}
		case LUX_FILM_LG_SCALE_GREEN: {
			RGBColor color(GetGroupRGBScale(index));
			color.c[1] = value;
			SetGroupRGBScale(index, color);
			break;
		}
		case LUX_FILM_LG_SCALE_BLUE: {
			RGBColor color(GetGroupRGBScale(index));
			color.c[2] = value;
			SetGroupRGBScale(index, color);
			break;
		}
		case LUX_FILM_LG_TEMPERATURE: {
			SetGroupTemperature(index, value);
			break;
		}

		 default:
			break;
	 }
}
double FlexImageFilm::GetParameterValue(luxComponentParameters param, u_int index)
{
	 switch (param) {
		case LUX_FILM_TM_TONEMAPKERNEL:
			return m_TonemapKernel;
			break;

		case LUX_FILM_TM_REINHARD_PRESCALE:
			return m_ReinhardPreScale;
			break;
		case LUX_FILM_TM_REINHARD_POSTSCALE:
			return m_ReinhardPostScale;
			break;
		case LUX_FILM_TM_REINHARD_BURN:
			return m_ReinhardBurn;
			break;

		case LUX_FILM_TM_LINEAR_SENSITIVITY:
			return m_LinearSensitivity;
			break;
		case LUX_FILM_TM_LINEAR_EXPOSURE:
			return m_LinearExposure;
			break;
		case LUX_FILM_TM_LINEAR_FSTOP:
			return m_LinearFStop;
			break;
		case LUX_FILM_TM_LINEAR_GAMMA:
			return m_LinearGamma;
			break;

		case LUX_FILM_TM_CONTRAST_YWA:
			return m_ContrastYwa;
			break;

		case LUX_FILM_TORGB_X_WHITE:
			return m_RGB_X_White;
			break;
		case LUX_FILM_TORGB_Y_WHITE:
			return m_RGB_Y_White;
			break;
		case LUX_FILM_TORGB_X_RED:
			return m_RGB_X_Red;
			break;
		case LUX_FILM_TORGB_Y_RED:
			return m_RGB_Y_Red;
			break;
		case LUX_FILM_TORGB_X_GREEN:
			return m_RGB_X_Green;
			break;
		case LUX_FILM_TORGB_Y_GREEN:
			return m_RGB_Y_Green;
			break;
		case LUX_FILM_TORGB_X_BLUE:
			return m_RGB_X_Blue;
			break;
		case LUX_FILM_TORGB_Y_BLUE:
			return m_RGB_Y_Blue;
			break;
		case LUX_FILM_TORGB_GAMMA:
			return m_Gamma;
			break;

		case LUX_FILM_CAMERA_RESPONSE_ENABLED:
			return m_CameraResponseEnabled;

		case LUX_FILM_BLOOMRADIUS:
			return m_BloomRadius;
			break;
		case LUX_FILM_BLOOMWEIGHT:
			return m_BloomWeight;
			break;

		case LUX_FILM_VIGNETTING_ENABLED:
			return m_VignettingEnabled;
			break;
		case LUX_FILM_VIGNETTING_SCALE:
			return m_VignettingScale;
			break;

		case LUX_FILM_ABERRATION_ENABLED:
			return m_AberrationEnabled;
			break;
		case LUX_FILM_ABERRATION_AMOUNT:
			return m_AberrationAmount;
			break;

		case LUX_FILM_GLARE_AMOUNT:
			return m_GlareAmount;
			break;
		case LUX_FILM_GLARE_RADIUS:
			return m_GlareRadius;
			break;
		case LUX_FILM_GLARE_BLADES:
			return m_GlareBlades;
			break;
		case LUX_FILM_GLARE_THRESHOLD:
			return m_GlareThreshold;
			break;

		case LUX_FILM_HISTOGRAM_ENABLED:
			return m_HistogramEnabled;
			break;

		case LUX_FILM_NOISE_CHIU_ENABLED:
			return m_chiuParams.enabled;
			break;
		case LUX_FILM_NOISE_CHIU_RADIUS:
			return m_chiuParams.radius;
			break;
		case LUX_FILM_NOISE_CHIU_INCLUDECENTER:
			return m_chiuParams.includecenter;
			break;

		case LUX_FILM_NOISE_GREYC_ENABLED:
			return m_GREYCStorationParams.enabled;
			break;
		case LUX_FILM_NOISE_GREYC_AMPLITUDE:
			return m_GREYCStorationParams.amplitude;
			break;
		case LUX_FILM_NOISE_GREYC_NBITER:
			return m_GREYCStorationParams.nb_iter;
			break;
		case LUX_FILM_NOISE_GREYC_SHARPNESS:
			return m_GREYCStorationParams.sharpness;
			break;
		case LUX_FILM_NOISE_GREYC_ANISOTROPY:
			return m_GREYCStorationParams.anisotropy;
			break;
		case LUX_FILM_NOISE_GREYC_ALPHA:
			return m_GREYCStorationParams.alpha;
			break;
		case LUX_FILM_NOISE_GREYC_SIGMA:
			return m_GREYCStorationParams.sigma;
			break;
		case LUX_FILM_NOISE_GREYC_FASTAPPROX:
			return m_GREYCStorationParams.fast_approx;
			break;
		case LUX_FILM_NOISE_GREYC_GAUSSPREC:
			return m_GREYCStorationParams.gauss_prec;
			break;
		case LUX_FILM_NOISE_GREYC_DL:
			return m_GREYCStorationParams.dl;
			break;
		case LUX_FILM_NOISE_GREYC_DA:
			return m_GREYCStorationParams.da;
			break;
		case LUX_FILM_NOISE_GREYC_INTERP:
			return m_GREYCStorationParams.interp;
			break;
		case LUX_FILM_NOISE_GREYC_TILE:
			return m_GREYCStorationParams.tile;
			break;
		case LUX_FILM_NOISE_GREYC_BTILE:
			return m_GREYCStorationParams.btile;
			break;
		case LUX_FILM_NOISE_GREYC_THREADS:
			return m_GREYCStorationParams.threads;
			break;

		case LUX_FILM_LG_COUNT:
			return GetNumBufferGroups();
			break;
		case LUX_FILM_LG_ENABLE:
			return GetGroupEnable(index);
			break;
		case LUX_FILM_LG_SCALE:
			return GetGroupScale(index);
			break;
		case LUX_FILM_LG_SCALE_RED:
			return GetGroupRGBScale(index).c[0];
			break;
		case LUX_FILM_LG_SCALE_GREEN:
			return GetGroupRGBScale(index).c[1];
			break;
		case LUX_FILM_LG_SCALE_BLUE:
			return GetGroupRGBScale(index).c[2];
			break;
		case LUX_FILM_LG_TEMPERATURE:
			return GetGroupTemperature(index);
			break;

		default:
			break;
	 }
	 return 0.;
}
double FlexImageFilm::GetDefaultParameterValue(luxComponentParameters param, u_int index)
{
	 switch (param) {
		case LUX_FILM_TM_TONEMAPKERNEL:
			return d_TonemapKernel;
			break;

		case LUX_FILM_TM_REINHARD_PRESCALE:
			return d_ReinhardPreScale;
			break;
		case LUX_FILM_TM_REINHARD_POSTSCALE:
			return d_ReinhardPostScale;
			break;
		case LUX_FILM_TM_REINHARD_BURN:
			return d_ReinhardBurn;
			break;

		case LUX_FILM_TM_LINEAR_SENSITIVITY:
			return d_LinearSensitivity;
			break;
		case LUX_FILM_TM_LINEAR_EXPOSURE:
			return d_LinearExposure;
			break;
		case LUX_FILM_TM_LINEAR_FSTOP:
			return d_LinearFStop;
			break;
		case LUX_FILM_TM_LINEAR_GAMMA:
			return d_LinearGamma;
			break;

		case LUX_FILM_TM_CONTRAST_YWA:
			return d_ContrastYwa;
			break;

		case LUX_FILM_TORGB_X_WHITE:
			return d_RGB_X_White;
			break;
		case LUX_FILM_TORGB_Y_WHITE:
			return d_RGB_Y_White;
			break;
		case LUX_FILM_TORGB_X_RED:
			return d_RGB_X_Red;
			break;
		case LUX_FILM_TORGB_Y_RED:
			return d_RGB_Y_Red;
			break;
		case LUX_FILM_TORGB_X_GREEN:
			return d_RGB_X_Green;
			break;
		case LUX_FILM_TORGB_Y_GREEN:
			return d_RGB_Y_Green;
			break;
		case LUX_FILM_TORGB_X_BLUE:
			return d_RGB_X_Blue;
			break;
		case LUX_FILM_TORGB_Y_BLUE:
			return d_RGB_Y_Blue;
			break;
		case LUX_FILM_TORGB_GAMMA:
			return d_Gamma;
			break;

		case LUX_FILM_CAMERA_RESPONSE_ENABLED:
			return d_CameraResponseEnabled;

		case LUX_FILM_BLOOMRADIUS:
			return d_BloomRadius;
			break;
		case LUX_FILM_BLOOMWEIGHT:
			return d_BloomWeight;
			break;

		case LUX_FILM_VIGNETTING_ENABLED:
			return d_VignettingEnabled;
			break;
		case LUX_FILM_VIGNETTING_SCALE:
			return d_VignettingScale;
			break;

		case LUX_FILM_ABERRATION_ENABLED:
			return d_AberrationEnabled;
			break;
		case LUX_FILM_ABERRATION_AMOUNT:
			return d_AberrationAmount;
			break;

		case LUX_FILM_GLARE_AMOUNT:
			return d_GlareAmount;
			break;
		case LUX_FILM_GLARE_RADIUS:
			return d_GlareRadius;
			break;
		case LUX_FILM_GLARE_BLADES:
			return d_GlareBlades;
			break;
		case LUX_FILM_GLARE_THRESHOLD:
			return d_GlareThreshold;
			break;

		case LUX_FILM_HISTOGRAM_ENABLED:
			return d_HistogramEnabled;
			break;

		case LUX_FILM_NOISE_CHIU_ENABLED:
			return d_chiuParams.enabled;
			break;
		case LUX_FILM_NOISE_CHIU_RADIUS:
			return d_chiuParams.radius;
			break;
		case LUX_FILM_NOISE_CHIU_INCLUDECENTER:
			return d_chiuParams.includecenter;
			break;

		case LUX_FILM_NOISE_GREYC_ENABLED:
			return d_GREYCStorationParams.enabled;
			break;
		case LUX_FILM_NOISE_GREYC_AMPLITUDE:
			return d_GREYCStorationParams.amplitude;
			break;
		case LUX_FILM_NOISE_GREYC_NBITER:
			return d_GREYCStorationParams.nb_iter;
			break;
		case LUX_FILM_NOISE_GREYC_SHARPNESS:
			return d_GREYCStorationParams.sharpness;
			break;
		case LUX_FILM_NOISE_GREYC_ANISOTROPY:
			return d_GREYCStorationParams.anisotropy;
			break;
		case LUX_FILM_NOISE_GREYC_ALPHA:
			return d_GREYCStorationParams.alpha;
			break;
		case LUX_FILM_NOISE_GREYC_SIGMA:
			return d_GREYCStorationParams.sigma;
			break;
		case LUX_FILM_NOISE_GREYC_FASTAPPROX:
			return d_GREYCStorationParams.fast_approx;
			break;
		case LUX_FILM_NOISE_GREYC_GAUSSPREC:
			return d_GREYCStorationParams.gauss_prec;
			break;
		case LUX_FILM_NOISE_GREYC_DL:
			return d_GREYCStorationParams.dl;
			break;
		case LUX_FILM_NOISE_GREYC_DA:
			return d_GREYCStorationParams.da;
			break;
		case LUX_FILM_NOISE_GREYC_INTERP:
			return d_GREYCStorationParams.interp;
			break;
		case LUX_FILM_NOISE_GREYC_TILE:
			return d_GREYCStorationParams.tile;
			break;
		case LUX_FILM_NOISE_GREYC_BTILE:
			return d_GREYCStorationParams.btile;
			break;
		case LUX_FILM_NOISE_GREYC_THREADS:
			return d_GREYCStorationParams.threads;
			break;

		case LUX_FILM_LG_ENABLE:
			return true;
			break;
		case LUX_FILM_LG_SCALE:
			return 1.f;
			break;
		case LUX_FILM_LG_SCALE_RED:
			return 1.f;
			break;
		case LUX_FILM_LG_SCALE_GREEN:
			return 1.f;
			break;
		case LUX_FILM_LG_SCALE_BLUE:
			return 1.f;
			break;
		case LUX_FILM_LG_TEMPERATURE:
			return 0.f;
			break;

		default:
			break;
	 }
	 return 0.;
}

void FlexImageFilm::SetStringParameterValue(luxComponentParameters param, const string& value, u_int index) {
	switch(param) {
		case LUX_FILM_LG_NAME:
			return SetGroupName(index, value);
		case LUX_FILM_CAMERA_RESPONSE_FILE:
			m_CameraResponseFile = value;
		default:
			break;
	}
}
string FlexImageFilm::GetStringParameterValue(luxComponentParameters param, u_int index) {
	switch(param) {
		case LUX_FILM_LG_NAME:
			return GetGroupName(index);
		case LUX_FILM_CAMERA_RESPONSE_FILE:
			return m_CameraResponseFile;
		default:
			break;
	}
	return "";
}


void FlexImageFilm::CheckWriteOuputInterval()
{
	// Check write output interval
	boost::xtime currentTime;
	boost::xtime_get(&currentTime, boost::TIME_UTC);
	bool timeToWriteImage = (currentTime.sec - lastWriteImageTime.sec > writeInterval);
	bool timeToWriteFLM = (currentTime.sec - lastWriteFLMTime.sec > flmWriteInterval);

	// Possibly write out in-progress image
	if (!(timeToWriteImage || timeToWriteFLM))
		return;

	if (!framebuffer) 
		createFrameBuffer();

	ImageType output = IMAGE_NONE;
	if (timeToWriteImage)
		output = static_cast<ImageType>(output | IMAGE_FILEOUTPUT);
	if (timeToWriteFLM)
		output = static_cast<ImageType>(output | IMAGE_FLMOUTPUT);

	WriteImage(output);

	// WriteImage can take a very long time to be executed (i.e. by saving
	// the film. It is better to refresh timestamps after the
	// execution of WriteImage instead than before.
	boost::xtime_get(&currentTime, boost::TIME_UTC);

	if (timeToWriteImage)
		lastWriteImageTime = currentTime;
	if (timeToWriteFLM)
		lastWriteFLMTime = currentTime;
}

vector<RGBColor>& FlexImageFilm::ApplyPipeline(const ColorSystem &colorSpace, vector<XYZColor> &xyzcolor)
{
	// Apply the imaging/tonemapping pipeline
	// not reentrant!
	ParamSet toneParams;
	std::string tmkernel = "reinhard";
	if(m_TonemapKernel == TMK_Reinhard) {
		// Reinhard Tonemapper
		toneParams.AddFloat("prescale", &m_ReinhardPreScale, 1);
		toneParams.AddFloat("postscale", &m_ReinhardPostScale, 1);
		toneParams.AddFloat("burn", &m_ReinhardBurn, 1);
		tmkernel = "reinhard";
	} else if(m_TonemapKernel == TMK_Linear) {
		// Linear Tonemapper
		toneParams.AddFloat("sensitivity", &m_LinearSensitivity, 1);
		toneParams.AddFloat("exposure", &m_LinearExposure, 1);
		toneParams.AddFloat("fstop", &m_LinearFStop, 1);
		toneParams.AddFloat("gamma", &m_LinearGamma, 1);
		tmkernel = "linear";
	} else if(m_TonemapKernel == TMK_Contrast) {
		// Contrast Tonemapper
		toneParams.AddFloat("ywa", &m_ContrastYwa, 1);
		tmkernel = "contrast";
	} else if(m_TonemapKernel == TMK_MaxWhite) {		
		// MaxWhite Tonemapper
		tmkernel = "maxwhite";
	} else { // if(m_TonemapKernel == TMK_AutoLinear) {
		// Auto Linear Tonemapper
		tmkernel = "autolinear";
	}

	// Delete bloom/glare layers if requested
	if (!m_BloomUpdateLayer && m_BloomDeleteLayer && m_HaveBloomImage) {
		m_HaveBloomImage = false;
		delete[] m_bloomImage;
		m_bloomImage = NULL;
		m_BloomDeleteLayer = false;
	}

	if (!m_GlareUpdateLayer && m_GlareDeleteLayer && m_HaveGlareImage) {
		m_HaveGlareImage = false;
		delete[] m_glareImage;
		m_glareImage = NULL;
		m_GlareDeleteLayer = false;
	}

	// use local shared_ptr to keep reference to current cameraResponse
	// so we can pass a regular pointer to ApplyImagingPipeline
	boost::shared_ptr<CameraResponse> crf;
	if (m_CameraResponseFile == "")
		cameraResponse.reset();

	if (m_CameraResponseEnabled) {
		if ((!cameraResponse && m_CameraResponseFile != "") || (cameraResponse && cameraResponse->filmName != m_CameraResponseFile))
			cameraResponse.reset(new CameraResponse(m_CameraResponseFile));

		crf = cameraResponse;
	}

	// Apply chosen tonemapper
	ApplyImagingPipeline(xyzcolor, xPixelCount, yPixelCount, m_GREYCStorationParams, m_chiuParams,
		colorSpace, histogram, m_HistogramEnabled, m_HaveBloomImage, m_bloomImage, m_BloomUpdateLayer,
		m_BloomRadius, m_BloomWeight, m_VignettingEnabled, m_VignettingScale, m_AberrationEnabled, m_AberrationAmount,
		m_HaveGlareImage, m_glareImage, m_GlareUpdateLayer, m_GlareAmount, m_GlareRadius, m_GlareBlades, m_GlareThreshold,
		tmkernel.c_str(), &toneParams, crf.get(), 0.f);

	// Disable further bloom layer updates if used.
	m_BloomUpdateLayer = false;
	m_GlareUpdateLayer = false;

	return reinterpret_cast<vector<RGBColor> &>(xyzcolor);
}

void FlexImageFilm::WriteImage2(ImageType type, vector<XYZColor> &xyzcolor, vector<float> &alpha, string postfix)
{
	// NOTE - this method is not reentrant! 
	// write_mutex must be aquired before calling WriteImage2

	// Construct ColorSystem from values
	colorSpace = ColorSystem(m_RGB_X_Red, m_RGB_Y_Red,
		m_RGB_X_Green, m_RGB_Y_Green,
		m_RGB_X_Blue, m_RGB_Y_Blue,
		m_RGB_X_White, m_RGB_Y_White, 1.f);

	// Construct normalized Z buffer if used
	vector<float> zBuf;
	if(use_Zbuf && (write_EXR_ZBuf || write_PNG_ZBuf || write_TGA_ZBuf)) {
		const u_int nPix = xPixelCount * yPixelCount;
		zBuf.resize(nPix, 0.f);
		for (u_int offset = 0, y = 0; y < yPixelCount; ++y) {
			for (u_int x = 0; x < xPixelCount; ++x,++offset) {
				zBuf[offset] = ZBuffer->GetData(x, y);
			}
		}
	}

	if (type & IMAGE_FILEOUTPUT) {
		// write out untonemapped EXR
		if (write_EXR && !write_EXR_applyimaging) {
			// convert to rgb
			const u_int nPix = xPixelCount * yPixelCount;
			vector<RGBColor> rgbColor(nPix);
			for ( u_int i = 0; i < nPix; i++ )
				rgbColor[i] = colorSpace.ToRGBConstrained(xyzcolor[i]);

			WriteEXRImage(rgbColor, alpha, filename + postfix + ".exr", zBuf);
		}
	}

	// Dade - check if I have to run ApplyImagingPipeline
	if (((type & IMAGE_FRAMEBUFFER) && framebuffer) ||
		((type & IMAGE_FILEOUTPUT) && ((write_EXR && write_EXR_applyimaging) || write_TGA || write_PNG))) {

		// DO NOT USE xyzcolor ANYMORE AFTER THIS POINT
		vector<RGBColor> &rgbcolor = ApplyPipeline(colorSpace, xyzcolor);

		if (type & IMAGE_FILEOUTPUT) {
			// write out tonemapped EXR
			if ((write_EXR && write_EXR_applyimaging))
				WriteEXRImage(rgbcolor, alpha, filename + postfix + ".exr", zBuf);
		}

		// Output to low dynamic range formats
		if ((type & IMAGE_FILEOUTPUT) || (type & IMAGE_FRAMEBUFFER)) {
			// Clamp too high values
			// and apply gamma correction
			const float invGamma = 1.f / m_Gamma;
			const u_int nPix = xPixelCount * yPixelCount;
			for (u_int i = 0; i < nPix; ++i)
				rgbcolor[i] = colorSpace.Limit(rgbcolor[i], clampMethod).Pow(invGamma);

			// write out tonemapped TGA
			if ((type & IMAGE_FILEOUTPUT) && write_TGA)
				WriteTGAImage(rgbcolor, alpha, filename + postfix + ".tga");
			// write out tonemapped PNG
			if ((type & IMAGE_FILEOUTPUT) && write_PNG)
				WritePNGImage(rgbcolor, alpha, filename + postfix + ".png");
			// Copy to framebuffer pixels
			if ((type & IMAGE_FRAMEBUFFER) && framebuffer) {
				for (u_int i = 0; i < nPix; i++) {
					float_framebuffer[3 * i] = rgbcolor[i].c[0];
					float_framebuffer[3 * i + 1] = rgbcolor[i].c[1];
					float_framebuffer[3 * i + 2] = rgbcolor[i].c[2];

					framebuffer[3 * i] = static_cast<unsigned char>(Clamp(256 * rgbcolor[i].c[0], 0.f, 255.f));
					framebuffer[3 * i + 1] = static_cast<unsigned char>(Clamp(256 * rgbcolor[i].c[1], 0.f, 255.f));
					framebuffer[3 * i + 2] = static_cast<unsigned char>(Clamp(256 * rgbcolor[i].c[2], 0.f, 255.f));
				}
			}
		}
	}
}

void FlexImageFilm::WriteImage(ImageType type)
{
	boost::mutex::scoped_lock(write_mutex);
	
	// save the current status of the film if required
	// do it here instead of in WriteImage2 to reduce
	// memory usage
	if (type & IMAGE_FLMOUTPUT) {
		if (writeResumeFlm)
			WriteResumeFilm(filename + ".flm");
	}

	if (!framebuffer || !float_framebuffer || !alpha_buffer || !z_buffer)
		createFrameBuffer();

	const u_int nPix = xPixelCount * yPixelCount;
	vector<XYZColor> pixels(nPix);
	vector<float> alpha(nPix), alphaWeight(nPix, 0.f);

	// NOTE - lordcrc - separated buffer loop into two separate loops
	// in order to eliminate one of the framebuffer copies

	// write stand-alone buffers
	for(u_int j = 0; j < bufferGroups.size(); ++j) {
		if (!bufferGroups[j].enable)
			continue;

		for(u_int i = 0; i < bufferConfigs.size(); ++i) {
			const Buffer &buffer = *(bufferGroups[j].buffers[i]);

			if (!(bufferConfigs[i].output & BUF_STANDALONE))
				continue;

			buffer.GetData(&(pixels[0]), &(alpha[0]));
			WriteImage2(type, pixels, alpha, bufferConfigs[i].postfix);
		}
	}

	float Y = 0.f;
	// in order to fix bug #360
	// ouside loop not to trash the complete picture
	// if there are several buffer groups
	fill(pixels.begin(), pixels.end(), XYZColor(0.f));
	fill(alpha.begin(), alpha.end(), 0.f);

	XYZColor p;
	float a;

	// write framebuffer
	for(u_int j = 0; j < bufferGroups.size(); ++j) {
		if (!bufferGroups[j].enable)
			continue;

		for(u_int i = 0; i < bufferConfigs.size(); ++i) {
			const Buffer &buffer = *(bufferGroups[j].buffers[i]);
			if (!(bufferConfigs[i].output & BUF_FRAMEBUFFER))
				continue;

			for (u_int offset = 0, y = 0; y < yPixelCount; ++y) {
				for (u_int x = 0; x < xPixelCount; ++x,++offset) {

					alphaWeight[offset] += buffer.GetData(x, y, &p, &a);
					pixels[offset] += bufferGroups[j].convert.Adapt(p);
					alpha[offset] += a;
				}
			}
		}
	}
	// outside loop in order to write complete image
	for (u_int pix = 0; pix < nPix; ++pix) {
		if (alphaWeight[pix] > 0.f)
			alpha[pix] /= alphaWeight[pix];
		Y += pixels[pix].c[1];
		alpha_buffer[pix] = alpha[pix];
	}
	Y /= nPix;
	averageLuminance = Y;
	WriteImage2(type, pixels, alpha, "");
	// The relation between EV and luminance in cd.m-2 is:
	// EV = log2(L * S / K)
	// where L is the luminance, S is the ISO speed and K is a constant
	// usually S is taken to be 100 and K to be 12.5
	EV = logf(Y * 8.f) / logf(2.f);
}

void FlexImageFilm::SaveEXR(const string &exrFilename, bool useHalfFloats, bool includeZBuf, int compressionType, bool tonemapped)
{
	boost::mutex::scoped_lock(write_mutex);

	// Seth - Code below based on FlexImageFilm::WriteImage()
	const u_int nPix = xPixelCount * yPixelCount;
	vector<XYZColor> xyzcolor(nPix);
	vector<float> alpha(nPix), alphaWeight(nPix, 0.f);
	
	// in order to fix bug #360
	// ouside loop not to trash the complete picture
	// if there are several buffer groups
	fill(xyzcolor.begin(), xyzcolor.end(), XYZColor(0.f));
	fill(alpha.begin(), alpha.end(), 0.f);
	
	XYZColor p;
	float a;
	
	// write framebuffer (combines multiple buffer groups into a single buffer applying any modifiers to intensity and color)
	for(u_int j = 0; j < bufferGroups.size(); ++j) {
		if (!bufferGroups[j].enable)
			continue;
		
		for(u_int i = 0; i < bufferConfigs.size(); ++i) {
			const Buffer &buffer = *(bufferGroups[j].buffers[i]);
			if (!(bufferConfigs[i].output & BUF_FRAMEBUFFER))
				continue;
			
			for (u_int offset = 0, y = 0; y < yPixelCount; ++y) {
				for (u_int x = 0; x < xPixelCount; ++x,++offset) {
					
					alphaWeight[offset] += buffer.GetData(x, y, &p, &a);
					xyzcolor[offset] += bufferGroups[j].convert.Adapt(p);
					alpha[offset] += a;
				}
			}
		}
	}
	
	// outside loop in order to write complete image
	for (u_int pix = 0; pix < nPix; ++pix) {
		if (alphaWeight[pix] > 0.f)
			alpha[pix] /= alphaWeight[pix];
	}

	// Seth - Code below based on beginning of FlexImageFilm::WriteImage2()
	
	// Construct normalized Z buffer if needed
	vector<float> zBuf;
	if(includeZBuf) {

		// Make sure we have a ZBuffer
		if(!use_Zbuf || ZBuffer == NULL) {
			LOG(LUX_WARNING, LUX_NOERROR) << "Z Buffer output requested but is not available.  Will be omitted from OpenEXR file.";
			LOG(LUX_WARNING, LUX_NOERROR) << "Note: To enable Z Buffer, add the line '\"bool write_exr_ZBuf\" [\"true\"]' to the 'Film \"fleximage\"' section of your LXS file.";
		} else {
			zBuf.resize(nPix, 0.f);
			for (u_int offset = 0, y = 0; y < yPixelCount; ++y) {
				for (u_int x = 0; x < xPixelCount; ++x,++offset) {
					zBuf[offset] = ZBuffer->GetData(x, y);
				}
			}
		}
	}
	
	// convert to rgb
	colorSpace = ColorSystem(m_RGB_X_Red, m_RGB_Y_Red,
							 m_RGB_X_Green, m_RGB_Y_Green,
							 m_RGB_X_Blue, m_RGB_Y_Blue,
							 m_RGB_X_White, m_RGB_Y_White, 1.f);			
	
	// Backup members that affect WriteEXRImage()
	bool lOrigZbuf = write_EXR_ZBuf;
	bool lOrigHalf = write_EXR_halftype;
	int lOrigCompression = write_EXR_compressiontype;
	
	// Set members that affect WriteEXRImage() according to passed parameters
	write_EXR_ZBuf = use_Zbuf ? includeZBuf : false;
	write_EXR_halftype = useHalfFloats;
	write_EXR_compressiontype = compressionType;
	
	vector<RGBColor> &rgbcolor = reinterpret_cast<vector<RGBColor> &>(xyzcolor);
	// DO NOT USE xyzcolor ANYMORE AFTER THIS POINT
	if (tonemapped) {
		rgbcolor = ApplyPipeline(colorSpace, xyzcolor);
	} else {
		for ( u_int i = 0; i < nPix; i++ )
			rgbcolor[i] = colorSpace.ToRGBConstrained(xyzcolor[i]);
	}
	WriteEXRImage(rgbcolor, alpha, exrFilename, zBuf);

	// Restore the write_EXR members
	write_EXR_ZBuf = lOrigZbuf;
	write_EXR_halftype = lOrigHalf;
	write_EXR_compressiontype = lOrigCompression;
}

// GUI LDR framebuffer access methods
void FlexImageFilm::createFrameBuffer()
{
	// allocate pixels
	unsigned int nPix = xPixelCount * yPixelCount;
	framebuffer = new unsigned char[3*nPix];			// TODO delete data
	float_framebuffer = new float[3*nPix];
	alpha_buffer = new float[nPix];
	z_buffer = new float[nPix];

	// zero it out
	memset(framebuffer,0,sizeof(*framebuffer)*3*nPix);
	memset(float_framebuffer,0,sizeof(*float_framebuffer)*3*nPix);
	memset(alpha_buffer,0,sizeof(*alpha_buffer)*nPix);
	memset(z_buffer,0,sizeof(*z_buffer)*nPix);
}

void FlexImageFilm::updateFrameBuffer()
{
	if(!framebuffer) {
		createFrameBuffer();
	}

	WriteImage(IMAGE_FRAMEBUFFER);
}
unsigned char* FlexImageFilm::getFrameBuffer()
{
	if(!framebuffer)
		createFrameBuffer();

	return framebuffer;
}

float* FlexImageFilm::getFloatFrameBuffer()
{
	if (!float_framebuffer)
		createFrameBuffer();

	return float_framebuffer;
}

float* FlexImageFilm::getAlphaBuffer()
{
	if (!alpha_buffer)
		createFrameBuffer();

	return alpha_buffer;
}

float* FlexImageFilm::getZBuffer()
{
	if (!z_buffer)
		createFrameBuffer();

	if (ZBuffer)
	{
		for (u_int offset = 0, y = 0; y < yPixelCount; ++y) {
			for (u_int x = 0; x < xPixelCount; ++x,++offset) {
				z_buffer[offset] = ZBuffer->GetData(x, y);
			}
		}
	}

	return z_buffer;
}

void FlexImageFilm::WriteTGAImage(vector<RGBColor> &rgb, vector<float> &alpha, const string &filename)
{
	// Write Truevision Targa TGA image
	LOG( LUX_INFO,LUX_NOERROR)<< "Writing Tonemapped TGA image to file "<< filename;
	WriteTargaImage(write_TGA_channels, write_TGA_ZBuf, filename, rgb, alpha,
		xPixelCount, yPixelCount,
		xResolution, yResolution,
		xPixelStart, yPixelStart);
}

void FlexImageFilm::WritePNGImage(vector<RGBColor> &rgb, vector<float> &alpha, const string &filename)
{
	// Write Portable Network Graphics PNG image
	LOG( LUX_INFO,LUX_NOERROR)<< "Writing Tonemapped PNG image to file "<< filename;
	WritePngImage(write_PNG_channels, write_PNG_16bit, write_PNG_ZBuf, filename, rgb, alpha,
		xPixelCount, yPixelCount,
		xResolution, yResolution,
		xPixelStart, yPixelStart, colorSpace, m_Gamma);
}

void FlexImageFilm::WriteEXRImage(vector<RGBColor> &rgb, vector<float> &alpha, const string &filename, vector<float> &zbuf)
{
	
	if(write_EXR_ZBuf) {
		if(write_EXR_ZBuf_normalizationtype == CameraStartEnd) {
			// Camera normalization
		} else if(write_EXR_ZBuf_normalizationtype == MinMax) {
			// Min/Max normalization
			const u_int nPix = xPixelCount * yPixelCount;
			float min = 0.f;
			float max = INFINITY;
			for(u_int i=0; i<nPix; i++) {
				if(zbuf[i] > 0.f) {
					if(zbuf[i] > min) min = zbuf[i];
					if(zbuf[i] < max) max = zbuf[i];
				}
			}

			vector<float> zBuf(nPix);
			for (u_int i=0; i<nPix; i++)
				zBuf[i] = (zbuf[i]-min) / (max-min);

			LOG( LUX_INFO,LUX_NOERROR)<< "Writing OpenEXR image to file "<< filename;
			WriteOpenEXRImage(write_EXR_channels, write_EXR_halftype, write_EXR_ZBuf, write_EXR_compressiontype, filename, rgb, alpha,
				xPixelCount, yPixelCount,
				xResolution, yResolution,
				xPixelStart, yPixelStart, zBuf);
			return;
		}
	}

	// Write OpenEXR RGBA image
	LOG( LUX_INFO,LUX_NOERROR)<< "Writing OpenEXR image to file "<< filename;
	WriteOpenEXRImage(write_EXR_channels, write_EXR_halftype, write_EXR_ZBuf, write_EXR_compressiontype, filename, rgb, alpha,
		xPixelCount, yPixelCount,
		xResolution, yResolution,
		xPixelStart, yPixelStart, zbuf);
}

void FlexImageFilm::GetColorspaceParam(const ParamSet &params, const string name, float values[2]) {
	u_int i;
	const float *v = params.FindFloat(name, &i);
	if (v && i == 2) {
		values[0] = v[0];
		values[1] = v[1];
	}
}

// params / creation
Film* FlexImageFilm::CreateFilm(const ParamSet &params, Filter *filter)
{
	// General
	bool premultiplyAlpha = params.FindOneBool("premultiplyalpha", false);

	int xres = params.FindOneInt("xresolution", 800);
	int yres = params.FindOneInt("yresolution", 600);

	float crop[4] = { 0, 1, 0, 1 };
	u_int cwi;
	const float *cr = params.FindFloat("cropwindow", &cwi);
	if (cr && cwi == 4) {
		crop[0] = Clamp(min(cr[0], cr[1]), 0.f, 1.f);
		crop[1] = Clamp(max(cr[0], cr[1]), 0.f, 1.f);
		crop[2] = Clamp(min(cr[2], cr[3]), 0.f, 1.f);
		crop[3] = Clamp(max(cr[2], cr[3]), 0.f, 1.f);
	}

	// Filter quality
	u_int filtRes = 1 << max(params.FindOneInt("filterquality", 4), 1);

	// Output Image File Formats
	string clampMethodString = params.FindOneString("ldr_clamp_method", "lum");
	int clampMethod = 0;
	if (clampMethodString == "lum")
		clampMethod = 0;
	else if (clampMethodString == "hue")
		clampMethod = 1;
	else if (clampMethodString == "cut")
		clampMethod = 2;
	else {
		LOG(LUX_WARNING,LUX_BADTOKEN)<< "LDR clamping method  '" << clampMethodString << "' unknown. Using \"lum\".";
	}

	// OpenEXR
	bool w_EXR = params.FindOneBool("write_exr", false);

	OutputChannels w_EXR_channels = RGB;
	string w_EXR_channelsStr = params.FindOneString("write_exr_channels", "RGB");
	if (w_EXR_channelsStr == "Y") w_EXR_channels = Y;
	else if (w_EXR_channelsStr == "YA") w_EXR_channels = YA;
	else if (w_EXR_channelsStr == "RGB") w_EXR_channels = RGB;
	else if (w_EXR_channelsStr == "RGBA") w_EXR_channels = RGBA;
	else {
		LOG(LUX_WARNING,LUX_BADTOKEN) << "OpenEXR Output Channels  '" << w_EXR_channelsStr << "' unknown. Using \"RGB\".";
		w_EXR_channels = RGB;
	}

	bool w_EXR_halftype = params.FindOneBool("write_exr_halftype", true);

	int w_EXR_compressiontype = 1;
	string w_EXR_compressiontypeStr = params.FindOneString("write_exr_compressiontype", "PIZ (lossless)");
	if (w_EXR_compressiontypeStr == "RLE (lossless)") w_EXR_compressiontype = 0;
	else if (w_EXR_compressiontypeStr == "PIZ (lossless)") w_EXR_compressiontype = 1;
	else if (w_EXR_compressiontypeStr == "ZIP (lossless)") w_EXR_compressiontype = 2;
	else if (w_EXR_compressiontypeStr == "Pxr24 (lossy)") w_EXR_compressiontype = 3;
	else if (w_EXR_compressiontypeStr == "None") w_EXR_compressiontype = 4;
	else {
		LOG(LUX_WARNING,LUX_BADTOKEN) << "OpenEXR Compression Type '" << w_EXR_compressiontypeStr << "' unknown. Using \"PIZ (lossless)\".";
		w_EXR_compressiontype = 1;
	}

	bool w_EXR_applyimaging = params.FindOneBool("write_exr_applyimaging", true);
	bool w_EXR_gamutclamp = params.FindOneBool("write_exr_gamutclamp", true);

	bool w_EXR_ZBuf = params.FindOneBool("write_exr_ZBuf", false);

	ZBufNormalization w_EXR_ZBuf_normalizationtype = None;
	string w_EXR_ZBuf_normalizationtypeStr = params.FindOneString("write_exr_zbuf_normalizationtype", "None");
	if (w_EXR_ZBuf_normalizationtypeStr == "None") w_EXR_ZBuf_normalizationtype = None;
	else if (w_EXR_ZBuf_normalizationtypeStr == "Camera Start/End clip") w_EXR_ZBuf_normalizationtype = CameraStartEnd;
	else if (w_EXR_ZBuf_normalizationtypeStr == "Min/Max") w_EXR_ZBuf_normalizationtype = MinMax;
	else {
		LOG(LUX_WARNING,LUX_BADTOKEN) << "OpenEXR ZBuf Normalization Type '" << w_EXR_ZBuf_normalizationtypeStr << "' unknown. Using \"None\".";
		w_EXR_ZBuf_normalizationtype = None;
	}

	// Portable Network Graphics (PNG)
	bool w_PNG = params.FindOneBool("write_png", true);

	OutputChannels w_PNG_channels = RGB;
	string w_PNG_channelsStr = params.FindOneString("write_png_channels", "RGB");
	if (w_PNG_channelsStr == "Y") w_PNG_channels = Y;
	else if (w_PNG_channelsStr == "YA") w_PNG_channels = YA;
	else if (w_PNG_channelsStr == "RGB") w_PNG_channels = RGB;
	else if (w_PNG_channelsStr == "RGBA") w_PNG_channels = RGBA;
	else {
		LOG(LUX_WARNING,LUX_BADTOKEN) << "PNG Output Channels  '" << w_PNG_channelsStr << "' unknown. Using \"RGB\".";
		w_PNG_channels = RGB;
	}

	bool w_PNG_16bit = params.FindOneBool("write_png_16bit", false);
	bool w_PNG_gamutclamp = params.FindOneBool("write_png_gamutclamp", true);

	bool w_PNG_ZBuf = params.FindOneBool("write_png_ZBuf", false);

	ZBufNormalization w_PNG_ZBuf_normalizationtype = MinMax;
	string w_PNG_ZBuf_normalizationtypeStr = params.FindOneString("write_png_zbuf_normalizationtype", "Min/Max");
	if (w_PNG_ZBuf_normalizationtypeStr == "None") w_PNG_ZBuf_normalizationtype = None;
	else if (w_PNG_ZBuf_normalizationtypeStr == "Camera Start/End clip") w_PNG_ZBuf_normalizationtype = CameraStartEnd;
	else if (w_PNG_ZBuf_normalizationtypeStr == "Min/Max") w_PNG_ZBuf_normalizationtype = MinMax;
	else {
		LOG(LUX_WARNING,LUX_BADTOKEN) << "PNG ZBuf Normalization Type '" << w_PNG_ZBuf_normalizationtypeStr << "' unknown. Using \"Min/Max\".";
		w_PNG_ZBuf_normalizationtype = MinMax;
	}

	// TGA
	bool w_TGA = params.FindOneBool("write_tga", false);

	OutputChannels w_TGA_channels = RGB;
	string w_TGA_channelsStr = params.FindOneString("write_tga_channels", "RGB");
	if (w_TGA_channelsStr == "Y") w_TGA_channels = Y;
	else if (w_TGA_channelsStr == "RGB") w_TGA_channels = RGB;
	else if (w_TGA_channelsStr == "RGBA") w_TGA_channels = RGBA;
	else {
		LOG(LUX_WARNING,LUX_BADTOKEN) << "TGA Output Channels  '" << w_TGA_channelsStr << "' unknown. Using \"RGB\".";
		w_TGA_channels = RGB;
	}

	bool w_TGA_gamutclamp = params.FindOneBool("write_tga_gamutclamp", true);

	bool w_TGA_ZBuf = params.FindOneBool("write_tga_ZBuf", false);

	ZBufNormalization w_TGA_ZBuf_normalizationtype = MinMax;
	string w_TGA_ZBuf_normalizationtypeStr = params.FindOneString("write_tga_zbuf_normalizationtype", "Min/Max");
	if (w_TGA_ZBuf_normalizationtypeStr == "None") w_TGA_ZBuf_normalizationtype = None;
	else if (w_TGA_ZBuf_normalizationtypeStr == "Camera Start/End clip") w_TGA_ZBuf_normalizationtype = CameraStartEnd;
	else if (w_TGA_ZBuf_normalizationtypeStr == "Min/Max") w_TGA_ZBuf_normalizationtype = MinMax;
	else {
		LOG(LUX_WARNING,LUX_BADTOKEN) << "TGA ZBuf Normalization Type '" << w_TGA_ZBuf_normalizationtypeStr << "' unknown. Using \"Min/Max\".";
		w_TGA_ZBuf_normalizationtype = MinMax;
	}


	// Output FILM / FLM 
    bool w_resume_FLM = params.FindOneBool("write_resume_flm", false);
	bool restart_resume_FLM = params.FindOneBool("restart_resume_flm", false);

	// output filenames
	string filename = params.FindOneString("filename", "luxout");
	filename = boost::filesystem::path(filename).string();

	// intervals
	int writeInterval = params.FindOneInt("writeinterval", 60);
	int flmWriteInterval = params.FindOneInt("flmwriteinterval", writeInterval);
	int displayInterval = params.FindOneInt("displayinterval", 12);

	// Rejection mechanism
	int reject_warmup = params.FindOneInt("reject_warmup", 64); // minimum samples/px before rejecting

	int outlierrejection_k = params.FindOneInt("outlierrejection_k", 0); // k for k-nearest in outlier rejection, 0 = off

	// Debugging mode (display erratic sample values and disable rejection mechanism)
	bool debug_mode = params.FindOneBool("debug", false);

	const int haltspp = params.FindOneInt("haltspp", -1);
	const int halttime = params.FindOneInt("halttime", -1);

	// Color space primaries and white point
	// default is SMPTE
	float red[2] = {0.63f, 0.34f};
	GetColorspaceParam(params, "colorspace_red", red);

	float green[2] = {0.31f, 0.595f};
	GetColorspaceParam(params, "colorspace_green", green);

	float blue[2] = {0.155f, 0.07f};
	GetColorspaceParam(params, "colorspace_blue", blue);

	float white[2] = {0.314275f, 0.329411f};
	GetColorspaceParam(params, "colorspace_white", white);

	// Tonemapping
	int s_TonemapKernel = TMK_Reinhard;
	string tmkernelStr = params.FindOneString("tonemapkernel", "reinhard");
	if (tmkernelStr == "reinhard") s_TonemapKernel = TMK_Reinhard;
	else if (tmkernelStr == "linear") s_TonemapKernel = TMK_Linear;
	else if (tmkernelStr == "contrast") s_TonemapKernel = TMK_Contrast;
	else if (tmkernelStr == "maxwhite") s_TonemapKernel = TMK_MaxWhite;
	else if (tmkernelStr == "autolinear") s_TonemapKernel = TMK_AutoLinear;
	else {
		LOG(LUX_WARNING,LUX_BADTOKEN) << "Tonemap kernel  '" << tmkernelStr << "' unknown. Using \"reinhard\".";
		s_TonemapKernel = TMK_Reinhard;
	}

	float s_ReinhardPreScale = params.FindOneFloat("reinhard_prescale", 1.f);
	float s_ReinhardPostScale = params.FindOneFloat("reinhard_postscale", 1.f);
	float s_ReinhardBurn = params.FindOneFloat("reinhard_burn", 6.f);
	float s_LinearSensitivity = params.FindOneFloat("linear_sensitivity", 50.f);
	float s_LinearExposure = params.FindOneFloat("linear_exposure", 1.f);
	float s_LinearFStop = params.FindOneFloat("linear_fstop", 2.8f);
	float s_LinearGamma = params.FindOneFloat("linear_gamma", 1.0f);
	float s_ContrastYwa = params.FindOneFloat("contrast_ywa", 1.f);

	string response = AdjustFilename(params.FindOneString("cameraresponse", ""));

	float s_Gamma = params.FindOneFloat("gamma", 2.2f);

	return new FlexImageFilm(xres, yres, filter, filtRes, crop,
		filename, premultiplyAlpha, writeInterval, flmWriteInterval, displayInterval,
		clampMethod, w_EXR, w_EXR_channels, w_EXR_halftype, w_EXR_compressiontype, w_EXR_applyimaging, w_EXR_gamutclamp, w_EXR_ZBuf, w_EXR_ZBuf_normalizationtype,
		w_PNG, w_PNG_channels, w_PNG_16bit, w_PNG_gamutclamp, w_PNG_ZBuf, w_PNG_ZBuf_normalizationtype,
		w_TGA, w_TGA_channels, w_TGA_gamutclamp, w_TGA_ZBuf, w_TGA_ZBuf_normalizationtype, 
		w_resume_FLM, restart_resume_FLM, haltspp, halttime,
		s_TonemapKernel, s_ReinhardPreScale, s_ReinhardPostScale, s_ReinhardBurn, s_LinearSensitivity,
		s_LinearExposure, s_LinearFStop, s_LinearGamma, s_ContrastYwa, response, s_Gamma,
		red, green, blue, white, reject_warmup, debug_mode, outlierrejection_k);
}


Film *FlexImageFilm::CreateFilmFromFLM(const string& flmFileName) {

	// NOTE - lordcrc - FlexImageFilm takes ownership of filter
	ParamSet dummyParams;
	Filter *dummyFilter = MakeFilter("box", dummyParams);

	// Create the default film
	const string filename = flmFileName.substr(0, flmFileName.length() - 4); // remove .flm extention
	static const bool boolTrue = true;
	static const bool boolFalse = false;
	ParamSet filmParams;
	filmParams.AddString("filename", &filename );
	//filmParams.AddInt("xresolution", 1);
	//filmParams.AddInt("yresolution", 1);
	filmParams.AddBool("write_resume_flm", &boolTrue);
	filmParams.AddBool("restart_resume_flm", &boolFalse);
	filmParams.AddBool("write_exr", &boolFalse);
	filmParams.AddBool("write_exr_ZBuf", &boolFalse);
	filmParams.AddBool("write_png", &boolFalse);
	filmParams.AddBool("write_png_ZBuf", &boolFalse);
	filmParams.AddBool("write_tga", &boolFalse);
	filmParams.AddBool("write_tga_ZBuf", &boolFalse);
	Film *film = FlexImageFilm::CreateFilm(filmParams, dummyFilter);

	if (!film->LoadResumeFilm(flmFileName)) {
		delete film;
		return NULL;
	}

	return film;
}

static DynamicLoader::RegisterFilm<FlexImageFilm> r1("fleximage");
static DynamicLoader::RegisterFilm<FlexImageFilm> r2("multiimage");
