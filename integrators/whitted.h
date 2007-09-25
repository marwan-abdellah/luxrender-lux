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

// whitted.cpp*
#include "lux.h"
#include "transport.h"
#include "scene.h"
// WhittedIntegrator Declarations
class WhittedIntegrator : public SurfaceIntegrator {
public:
	// WhittedIntegrator Public Methods
	Spectrum Li(const Scene *scene, const RayDifferential &ray,
			const Sample *sample, float *alpha) const;
	WhittedIntegrator(int md) {
		maxDepth = md;
		rayDepth = 0;
	}
	
	static SurfaceIntegrator *CreateSurfaceIntegrator(const ParamSet &params);
private:
	// WhittedIntegrator Private Data
	int maxDepth;
	mutable int rayDepth;
};
