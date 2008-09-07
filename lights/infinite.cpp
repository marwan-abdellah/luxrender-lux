/***************************************************************************
 *   Copyright (C) 1998-2008 by authors (see AUTHORS.txt )                 *
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

// infinite.cpp*
#include "infinite.h"
#include "imagereader.h"
#include "mc.h"
#include "paramset.h"
#include "blackbody.h"
using namespace lux;

// InfiniteAreaLight Method Definitions
InfiniteAreaLight::~InfiniteAreaLight() {
	delete radianceMap;
	delete SPDbase;
	delete mapping;
}
InfiniteAreaLight
	::InfiniteAreaLight(const Transform &light2world, const RGBColor &l, int ns, const string &texmap, EnvironmentMapping *m, float gain, float gamma)
	: Light(light2world, ns) {
	radianceMap = NULL;
	if (texmap != "") {
		auto_ptr<ImageData> imgdata(ReadImage(texmap));
		if(imgdata.get()!=NULL)
		{
			radianceMap = imgdata->createMIPMap<RGBColor>(BILINEAR, 8.f, 
				TEXTURE_REPEAT, 1.f, gamma);
		}
		else
			radianceMap=NULL;
	}

	mapping = m;

	// Base illuminant SPD
	SPDbase = new BlackbodySPD();
	SPDbase->Normalize();
	SPDbase->Scale(gain);

	// Base RGB RGBColor
	Lbase = l;
}

SWCSpectrum
	InfiniteAreaLight::Le(const TsPack *tspack, const RayDifferential &r) const {
	Vector w = r.d;
	// Compute infinite light radiance for direction
	
	RGBColor L = Lbase;
	if (radianceMap != NULL) {
		Vector wh = Normalize(WorldToLight(w));

		float s, t;

		mapping->Map(wh, &s, &t);

		L *= radianceMap->Lookup(s, t);
	}

	return SWCSpectrum(tspack, SPDbase) * SWCSpectrum(tspack, L);
}
SWCSpectrum InfiniteAreaLight::Sample_L(const TsPack *tspack, const Point &p,
		const Normal &n, float u1, float u2, float u3,
		Vector *wi, float *pdf,
		VisibilityTester *visibility) const {
	if(!havePortalShape) {
		// Sample cosine-weighted direction on unit sphere
		float x, y, z;
		ConcentricSampleDisk(u1, u2, &x, &y);
		z = sqrtf(max(0.f, 1.f - x*x - y*y));
		if (u3 < .5) z *= -1;
		*wi = Vector(x, y, z);
		// Compute _pdf_ for cosine-weighted infinite light direction
		*pdf = fabsf(wi->z) * INV_TWOPI;
		// Transform direction to world space
		Vector v1, v2;
		CoordinateSystem(Normalize(Vector(n)), &v1, &v2);
		*wi = Vector(v1.x * wi->x + v2.x * wi->y + n.x * wi->z,
					 v1.y * wi->x + v2.y * wi->y + n.y * wi->z,
					 v1.z * wi->x + v2.z * wi->y + n.z * wi->z);
	} else {
	    // Sample Portal
		int shapeidx = 0;
		if(nrPortalShapes > 1) 
			shapeidx = min<float>(nrPortalShapes - 1,
					Floor2Int(tspack->rng->floatValue() * nrPortalShapes));  // TODO - REFACT - add passed value from sample
		Normal ns;
		Point ps;
		bool found = false;
		for (int i = 0; i < nrPortalShapes; ++i) {
			ps = PortalShapes[shapeidx]->Sample(p, u1, u2, tspack->rng->floatValue(), &ns); // TODO - REFACT - add passed value from sample
			*wi = Normalize(ps - p);
			if (Dot(*wi, ns) < 0.f) {
				found = true;
				break;
			}

			if (++shapeidx >= nrPortalShapes)
				shapeidx = 0;
		}

		if (found)
			*pdf = PortalShapes[shapeidx]->Pdf(p, *wi);
		else {
			*pdf = 0.f;
			return SWCSpectrum(0.f);
		}
	}
	visibility->SetRay(p, *wi);
	return Le(tspack, RayDifferential(p, *wi));
}
float InfiniteAreaLight::Pdf(const Point &, const Normal &n,
		const Vector &wi) const {
	return AbsDot(n, wi) * INV_TWOPI;
}
SWCSpectrum InfiniteAreaLight::Sample_L(const TsPack *tspack, const Point &p,
		float u1, float u2, float u3, Vector *wi, float *pdf,
		VisibilityTester *visibility) const {
	if(!havePortalShape) {
		*wi = UniformSampleSphere(u1, u2);
		*pdf = UniformSpherePdf();
	} else {
	    // Sample a random Portal
		int shapeidx = 0;
		if(nrPortalShapes > 1) 
			shapeidx = min<float>(nrPortalShapes - 1,
					Floor2Int(tspack->rng->floatValue() * nrPortalShapes));  // TODO - REFACT - add passed value from sample
		Normal ns;
		Point ps;
		bool found = false;
		for (int i = 0; i < nrPortalShapes; ++i) {
			ps = PortalShapes[shapeidx]->Sample(p, u1, u2, tspack->rng->floatValue(), &ns); // TODO - REFACT - add passed value from sample
			*wi = Normalize(ps - p);
			if (Dot(*wi, ns) < 0.f) {
				found = true;
				break;
			}

			if (++shapeidx >= nrPortalShapes)
				shapeidx = 0;
		}

		if (found)
			*pdf = PortalShapes[shapeidx]->Pdf(p, *wi);
		else {
			*pdf = 0.f;
			return SWCSpectrum(0.f);
		}
	}
	visibility->SetRay(p, *wi);
	return Le(tspack, RayDifferential(p, *wi));
}
float InfiniteAreaLight::Pdf(const Point &, const Vector &) const {
	return 1.f / (4.f * M_PI);
}
SWCSpectrum InfiniteAreaLight::Sample_L(const TsPack *tspack, const Scene *scene,
		float u1, float u2, float u3, float u4,
		Ray *ray, float *pdf) const {
	if(!havePortalShape) {
		// Choose two points _p1_ and _p2_ on scene bounding sphere
		Point worldCenter;
		float worldRadius;
		scene->WorldBound().BoundingSphere(&worldCenter,
											&worldRadius);
		worldRadius *= 1.01f;
		Point p1 = worldCenter + worldRadius *
			UniformSampleSphere(u1, u2);
		Point p2 = worldCenter + worldRadius *
			UniformSampleSphere(u3, u4);
		// Construct ray between _p1_ and _p2_
		ray->o = p1;
		ray->d = Normalize(p2-p1);
		// Compute _InfiniteAreaLight_ ray weight
		Vector to_center = Normalize(worldCenter - p1);
		float costheta = AbsDot(to_center,ray->d);
		*pdf =
			costheta / ((4.f * M_PI * worldRadius * worldRadius));		
	} else {
		// Dade - choose a random portal. This strategy is quite bad if there
		// is more than one portal.
		int shapeidx = 0;
		if(nrPortalShapes > 1) 
			shapeidx = min<float>(nrPortalShapes - 1,
					Floor2Int(tspack->rng->floatValue() * nrPortalShapes));  // TODO - REFACT - add passed value from sample

		Normal ns;
		ray->o = PortalShapes[shapeidx]->Sample(u1, u2, tspack->rng->floatValue(), &ns); // TODO - REFACT - add passed value from sample
		ray->d = UniformSampleSphere(u3, u4);
		if (Dot(ray->d, ns) < 0.) ray->d *= -1;

		*pdf = PortalShapes[shapeidx]->Pdf(ray->o) * INV_TWOPI / nrPortalShapes;
	}

	return Le(tspack, RayDifferential(ray->o, -ray->d));
}
Light* InfiniteAreaLight::CreateLight(const Transform &light2world,
		const ParamSet &paramSet) {
	RGBColor L = paramSet.FindOneRGBColor("L", RGBColor(1.0));
	string texmap = paramSet.FindOneString("mapname", "");
	int nSamples = paramSet.FindOneInt("nsamples", 1);

	EnvironmentMapping *map = NULL;
	string type = paramSet.FindOneString("mapping", "");
	if (type == "" || type == "latlong") {
		map = new LatLongMapping();
	}
	else if (type == "angular") map = new AngularMapping();
	else if (type == "vcross") map = new VerticalCrossMapping();

	// Initialize _ImageTexture_ parameters
	float gain = paramSet.FindOneFloat("gain", 1.0f);
	float gamma = paramSet.FindOneFloat("gamma", 1.0f);

	return new InfiniteAreaLight(light2world, L, nSamples, texmap, map, gain, gamma);
}
