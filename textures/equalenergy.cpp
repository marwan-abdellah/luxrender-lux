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

// equalenergy.cpp*
#include "equalenergy.h"
#include "dynload.h"

using namespace lux;

// EqualEnergyTexture Method Definitions
Texture<SWCSpectrum> *EqualEnergyTexture::CreateSWCSpectrumTexture(const Transform &tex2world,
	const ParamSet &tp)
{
	return new EqualEnergyTexture(tp.FindOneFloat("energy", 1.f));
}

static DynamicLoader::RegisterSWCSpectrumTexture<EqualEnergyTexture> r("equalenergy");
