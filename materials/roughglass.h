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

// roughglass.cpp*
#include "lux.h"
#include "material.h"
// RoughGlassGlass Class Declarations
class RoughGlass : public Material {
public:
	// RoughGlass Public Methods
	RoughGlass(boost::shared_ptr<Texture<Spectrum> > r, boost::shared_ptr<Texture<Spectrum> > t, boost::shared_ptr<Texture<float> > rough,
			boost::shared_ptr<Texture<float> > i, boost::shared_ptr<Texture<float> > bump) {
		Kr = r;
		Kt = t;
		roughness = rough;
		index = i;
		bumpMap = bump;
	}
	BSDF *GetBSDF(MemoryArena &arena, const DifferentialGeometry &dgGeom, const DifferentialGeometry &dgShading) const;
	
	static Material * CreateMaterial(const Transform &xform, const TextureParams &mp);
private:
	// RoughGlass Private Data
	boost::shared_ptr<Texture<Spectrum> > Kr, Kt;
	boost::shared_ptr<Texture<float> > index;
	boost::shared_ptr<Texture<float> > roughness;
	boost::shared_ptr<Texture<float> > bumpMap;
};
