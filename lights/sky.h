/***************************************************************************
 *   Copyright (C) 1998-2007 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of Lux Renderer.                                    *
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

// sky.cpp*
#include "lux.h"
#include "light.h"
#include "texture.h"
#include "shape.h"
#include "scene.h"
#include "mipmap.h"

namespace lux
{

// SkyLight Declarations
class SkyLight : public Light {
public:
	// SkyLight Public Methods
	SkyLight(const Transform &light2world,	const Spectrum &power, int ns, Vector sd, float turb, bool atm);
	~SkyLight();
	SWCSpectrum Power(const Scene *scene) const {
		Point worldCenter;
		float worldRadius;
		scene->WorldBound().BoundingSphere(&worldCenter,
		                                    &worldRadius);
		//return Lbase * GetSkySpectralRadiance(.0, .0) * M_PI * worldRadius * worldRadius;
		return Lbase * M_PI * worldRadius * worldRadius;
	}
	bool IsDeltaLight() const { return false; }
	SWCSpectrum Le(const RayDifferential &r) const;
	SWCSpectrum Sample_L(const Point &p, const Normal &n,
		float u1, float u2, Vector *wi, float *pdf,
		VisibilityTester *visibility) const;
	SWCSpectrum Sample_L(const Point &p, float u1, float u2, Vector *wi, float *pdf,
		VisibilityTester *visibility) const;
	SWCSpectrum Sample_L(const Scene *scene, float u1, float u2,
			float u3, float u4, Ray *ray, float *pdf) const;
	float Pdf(const Point &, const Normal &, const Vector &) const;
	float Pdf(const Point &, const Vector &) const;
	SWCSpectrum Sample_L(const Point &P, Vector *w, VisibilityTester *visibility) const;
	
	static Light *CreateLight(const Transform &light2world,
		const ParamSet &paramSet);

		// internal methods
	Vector  	GetSunPosition() const;
	void 	SunThetaPhi(float &theta, float &phi) const;  
	Spectrum 	GetSunSpectralRadiance() const;
	float	GetSunSolidAngle() const;
	Spectrum  GetSkySpectralRadiance(const Vector &v) const;
	Spectrum  GetSkySpectralRadiance(float theta, float phi) const;
	void GetAtmosphericEffects(const Vector &viewer,
			       const Vector &source,
			       Spectrum &atmAttenuation,
			       Spectrum &atmInscatter ) const;

    void 	InitSunThetaPhi();
    Spectrum ComputeAttenuatedSunlight(float theta, float turbidity);
    Spectrum ChromaticityToSpectrum(float x, float y) const;
    Spectrum AttenuationFactor(float h0, float thetav, float s) const;
    Spectrum InscatteredRadiance(float h0, float thetav, float
				   phiv, float s) const;
    Spectrum GetNeta(float theta, float v) const;
    void CalculateA0(float thetav, float phiv, Spectrum& A0_1, Spectrum& A0_2) const;
    void CreateConstants();
    void InitA0() const;
    float PerezFunction(const float *lam, float theta, float phi, float lvz) const;

private:
	// SkyLight Private Data
	Spectrum Lbase;
	Vector  sundir;
    float 	turbidity;
    Vector 	toSun;
    float	thetaS, phiS;
    Spectrum 	sunSpectralRad;
    float 	sunSolidAngle;
    float zenith_Y, zenith_x, zenith_y;
    float perez_Y[6], perez_x[6], perez_y[6];
    Spectrum beta_m, beta_p, beta_m_ang_prefix,  beta_p_ang_prefix;
    float	V;
    bool atmInited;
};

}//namespace lux

