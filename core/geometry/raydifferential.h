
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


#ifndef LUX_RAYDIFFERENTIAL_H
#define LUX_RAYDIFFERENTIAL_H

#include "vector.h"
#include "point.h"
#include "ray.h"

namespace lux
{

class  RayDifferential : public Ray {
public:
	// RayDifferential Methods
	RayDifferential() { hasDifferentials = false; }
	RayDifferential(const Point &org, const Vector &dir)
			: Ray(org, dir) {
		hasDifferentials = false;
	}
	explicit RayDifferential(const Ray &ray) : Ray(ray) {
		hasDifferentials = false;
	}
	// RayDifferential Public Data
	
	Ray rx, ry;
	bool hasDifferentials;
	

};

}//namespace lux

#endif //LUX_RAYDIFFERENTIAL_H
