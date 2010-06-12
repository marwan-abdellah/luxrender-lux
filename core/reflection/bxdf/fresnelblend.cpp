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

// fresnelblend.cpp*
#include "fresnelblend.h"
#include "spectrum.h"
#include "mc.h"
#include "microfacetdistribution.h"

using namespace lux;

FresnelBlend::FresnelBlend(const SWCSpectrum &d,
	const SWCSpectrum &s,
	const SWCSpectrum &a,
	float dep,
	MicrofacetDistribution *dist)
	: BxDF(BxDFType(BSDF_REFLECTION | BSDF_GLOSSY)),
	  Rd(d), Rs(s), Alpha(a), depth(dep)
{
	distribution = dist;
}
void FresnelBlend::f(const TsPack *tspack, const Vector &wo, 
	 const Vector &wi, SWCSpectrum *const f_) const
{
	// absorption
	SWCSpectrum a(1.f);
	
	if (depth > 0.f) {
		float depthfactor = depth * (1.f / fabsf(CosTheta(wi)) + 1.f / fabsf(CosTheta(wo)));
		a = Exp(Alpha * -depthfactor);
	}

	// diffuse part
	f_->AddWeighted((1.f - powf(1.f - .5f * fabsf(CosTheta(wi)), 5.f)) *
		(1.f - powf(1.f - .5f * fabsf(CosTheta(wo)), 5.f)) *
		(28.f/(23.f*M_PI)), a * Rd * (SWCSpectrum(1.f) - Rs));

	Vector wh = Normalize(wi + wo);
	if (wh.z < 0.f)
		wh = -wh;
	// specular part
	f_->AddWeighted(distribution->D(wh) /
		(4.f * AbsDot(wi, wh) *
		max(fabsf(CosTheta(wi)), fabsf(CosTheta(wo)))),
		SchlickFresnel(AbsDot(wi, wh)));	
}

bool FresnelBlend::Sample_f(const TsPack *tspack, const Vector &wo, Vector *wi,
	float u1, float u2, SWCSpectrum *const f_, float *pdf, 
	float *pdfBack, bool reverse) const
{
	Vector wh;
	float d;
	u1 *= 2.f;
	if (u1 < 1.f) {
		// Cosine-sample the hemisphere, flipping the direction if necessary
		*wi = CosineSampleHemisphere(u1, u2);
		if (wo.z < 0.f)
			wi->z *= -1.f;
		Vector wh = Normalize(*wi + wo);
		if (wh.z < 0.f)
			wh = -wh;
		d = distribution->D(wh);
		*pdf = distribution->Pdf(wh);
	} else {
		u1 -= 1.f;
		distribution->SampleH(u1, u2, &wh, &d, pdf);
		*wi = 2.f * Dot(wo, wh) * wh - wo;
	}
	if (*pdf == 0.f) {
		if (pdfBack)
			*pdfBack = 0.f;
		return false;
	}
	if (pdfBack)
		*pdfBack = .5f * (fabsf(wo.z) * INV_PI +
			*pdf / (4.f * AbsDot(*wi, wh)));
	*pdf = .5f * (fabsf(wi->z) * INV_PI +
		*pdf / (4.f * AbsDot(wo, wh)));

	*f_ = SWCSpectrum(0.f);
	// FresnelBlend::f is symetric, no need to special case for revert
	f(tspack, wo, *wi, f_);
	return true;
}
float FresnelBlend::Pdf(const TsPack *tspack, const Vector &wo,
	const Vector &wi) const
{
	if (!SameHemisphere(wo, wi))
		return 0.f;
	Vector wh = Normalize(wi + wo);
	if (wh.z < 0.f)
		wh = -wh;
	return .5f * (fabsf(wi.z) * INV_PI +
		distribution->Pdf(wh) / (4.f * AbsDot(wo, wh)));
}

