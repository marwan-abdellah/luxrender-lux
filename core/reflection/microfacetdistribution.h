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

#ifndef LUX_MICROFACETDISTRIBUTION_H
#define LUX_MICROFACETDISTRIBUTION_H
// microfacetdistribution.h*
#include "lux.h"


namespace lux
{

class  MicrofacetDistribution {
public:
	// MicrofacetDistribution Interface
	virtual ~MicrofacetDistribution() { }
	virtual float D(const Vector &wh) const = 0;
	virtual void SampleH(float u1, float u2, Vector *wh, float *d,
		float *pdf) const = 0;
	virtual float Pdf(const Vector &wh) const = 0;
	virtual float G(const Vector &wo, const Vector &wi, const Vector &wh) const = 0;
};

}//namespace lux

#endif // LUX_MICROFACETDISTRIBUTION_H

