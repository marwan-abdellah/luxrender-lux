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

#ifndef LUX_PRIMITIVE_H
#define LUX_PRIMITIVE_H
// primitive.h*
#include "lux.h"
#include "shape.h"
#include "material.h"
// Primitive Declarations
class COREDLL Primitive : public ReferenceCounted {
public:
	// Primitive Interface
	virtual ~Primitive();
	virtual BBox WorldBound() const = 0;
	virtual bool CanIntersect() const;
	virtual bool Intersect(const Ray &r,
		Intersection *in) const = 0;
	virtual bool IntersectP(const Ray &r) const = 0;
	virtual void
		Refine(vector<Reference<Primitive> > &refined) const;
	void FullyRefine(vector<Reference<Primitive> > &refined)
	const;
	virtual const AreaLight *GetAreaLight() const = 0;
	virtual BSDF *GetBSDF(MemoryArena &arena, const DifferentialGeometry &dg,
		const Transform &WorldToObject) const = 0;
};
struct COREDLL Intersection {
	// Intersection Public Methods
	Intersection() { primitive = NULL; }
	BSDF *GetBSDF(MemoryArena &arena, const RayDifferential &ray) const;
	Spectrum Le(const Vector &wo) const;
	DifferentialGeometry dg;
	const Primitive *primitive;
	Transform WorldToObject;
};
class COREDLL GeometricPrimitive : public Primitive {
public:
	// GeometricPrimitive Public Methods
	bool CanIntersect() const;
	void Refine(vector<Reference<Primitive> > &refined) const;
	virtual BBox WorldBound() const;
	virtual bool Intersect(const Ray &r,
	                       Intersection *isect) const;
	virtual bool IntersectP(const Ray &r) const;
	GeometricPrimitive(const Reference<Shape> &s,
	                   const Reference<Material> &m,
	                   AreaLight *a);
	const AreaLight *GetAreaLight() const;
	BSDF *GetBSDF(MemoryArena &arena, const DifferentialGeometry &dg,
	              const Transform &WorldToObject) const;
private:
	// GeometricPrimitive Private Data
	Reference<Shape> shape;
	Reference<Material> material;
	AreaLight *areaLight;
};
class COREDLL InstancePrimitive : public Primitive {
public:
	// InstancePrimitive Public Methods
	InstancePrimitive(Reference<Primitive> &i,
	                  const Transform &i2w) {
		instance = i;
		InstanceToWorld = i2w;
		WorldToInstance = i2w.GetInverse();
	}
	bool Intersect(const Ray &r, Intersection *in) const;
	bool IntersectP(const Ray &r) const;
	const AreaLight *GetAreaLight() const { return NULL; }
	BSDF *GetBSDF(MemoryArena &arena, const DifferentialGeometry &dg,
	              const Transform &WorldToObject) const {
		return NULL;
	}
	BBox WorldBound() const {
		return InstanceToWorld(instance->WorldBound());
	}
private:
	// InstancePrimitive Private Data
	Reference<Primitive> instance;
	Transform InstanceToWorld, WorldToInstance;
};
class COREDLL Aggregate : public Primitive {
public:
	// Aggregate Public Methods
	const AreaLight *GetAreaLight() const;
	BSDF *GetBSDF(MemoryArena &arena, const DifferentialGeometry &dg,
	              const Transform &) const;
};
#endif // LUX_PRIMITIVE_H
