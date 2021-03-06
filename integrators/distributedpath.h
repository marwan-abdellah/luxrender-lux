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
 *   Lux Renderer website : http://www.luxrender.net                       *
 ***************************************************************************/

// distributedpath.cpp*
#include "lux.h"
#include "bxdf.h"
#include "light.h"
#include "transport.h"
#include "scene.h"

namespace lux
{

// DistributedPath Declarations

class DistributedPath : public SurfaceIntegrator {
public:
	// DistributedPath types
	enum LightStrategy { SAMPLE_ALL_UNIFORM, SAMPLE_ONE_UNIFORM,
		SAMPLE_AUTOMATIC
	};

	// DistributedPath Public Methods
	DistributedPath(LightStrategy st, bool da, u_int ds, bool dd, bool dg,
		bool ida, u_int ids, bool idd, bool idg, u_int drd, u_int drs,
		u_int dtd, u_int dts, u_int grd, u_int grs, u_int gtd,
		u_int gts, u_int srd, u_int std, bool drer, float drert,
		bool drfr, float drfrt,  bool grer, float grert, bool grfr,
		float grfrt);
	virtual ~DistributedPath() { }

	virtual u_int Li(const Scene &scene, const Sample &sample) const;
	virtual void RequestSamples(Sampler *sampler, const Scene &scene);
	virtual void Preprocess(const RandomGenerator &rng, const Scene &scene);
	static SurfaceIntegrator *CreateSurfaceIntegrator(const ParamSet &params);

private:
	void LiInternal(const Scene &scene, const Sample &sample,
		const Volume *volume, bool scattered, const Ray &ray,
		vector<SWCSpectrum> &L, float *alpha, float *zdepth,
		u_int rayDepth, bool includeEmit, u_int &nrContribs) const;
	void Reject(const SpectrumWavelengths &sw,
		vector< vector<SWCSpectrum> > &LL, vector<SWCSpectrum> &L,
		float rejectRange) const;
	void ComputeEvent(const Scene &scene, const Sample &sample,
		BSDF *bsdf, const Vector &wo, u_int nSamples,
		u_int indirectSampleOffset, u_int indirectComponentOffset,
		u_int sampleOffset, u_int componentOffset, BxDFType type,
		bool reject, float threshold, vector<SWCSpectrum> &L,
		float *alpha, float *zdepth, u_int rayDepth,
		u_int &nrContribs) const;

	// DistributedPath Private Data
	LightStrategy lightStrategy;
	bool directAll, directDiffuse, directGlossy, 
		indirectAll, indirectDiffuse, indirectGlossy;
	u_int directSamples, indirectSamples;
	u_int diffuseReflectDepth, diffuseReflectSamples, diffuseRefractDepth,
		diffuseRefractSamples, glossyReflectDepth, glossyReflectSamples,
		glossyRefractDepth, glossyRefractSamples, specularReflectDepth,
		specularRefractDepth, maxDepth;

	// Declare sample parameters for light source sampling
	u_int sampleOffset, scatterOffset, bufferId;
	u_int lightSampleOffset, lightNumOffset, bsdfSampleOffset,
	      bsdfComponentOffset;
	u_int indirectLightSampleOffset, indirectLightNumOffset,
	      indirectBsdfSampleOffset, indirectBsdfComponentOffset;
	u_int diffuseReflectSampleOffset, diffuseReflectComponentOffset,
	      indirectDiffuseReflectSampleOffset,
	      indirectDiffuseReflectComponentOffset;
	u_int diffuseRefractSampleOffset, diffuseRefractComponentOffset,
	      indirectDiffuseRefractSampleOffset,
	      indirectDiffuseRefractComponentOffset;
	u_int glossyReflectSampleOffset, glossyReflectComponentOffset,
	      indirectGlossyReflectSampleOffset,
	      indirectGlossyReflectComponentOffset;
	u_int glossyRefractSampleOffset, glossyRefractComponentOffset,
	      indirectGlossyRefractSampleOffset,
	      indirectGlossyRefractComponentOffset;

	bool diffuseReflectReject, diffuseRefractReject, glossyReflectReject,
	     glossyRefractReject;
	float diffuseReflectRejectThreshold, diffuseRefractRejectThreshold,
	      glossyReflectRejectThreshold, glossyRefractRejectThreshold;
};

}//namespace lux
