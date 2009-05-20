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

// environment.cpp*
#include "camera.h"

namespace lux
{

// EnvironmentCamera Declarations
class EnvironmentCamera : public Camera {
public:
	// EnvironmentCamera Public Methods
	EnvironmentCamera(const Transform &world2camStart, const Transform &world2camEnd, float hither,
		float yon, float sopen, float sclose, int sdist, Film *film);
	float GenerateRay(const Sample &sample, Ray *) const;
	bool Sample_W(const TsPack *tspack, const Scene *scene, float u1, float u2, float u3, BSDF **bsdf, float *pdf, SWCSpectrum *We) const;
	bool Sample_W(const TsPack *tspack, const Scene *scene, const Point &p, const Normal &n, float u1, float u2, float u3, BSDF **bsdf, float *pdf, float *pdfDirect, VisibilityTester *visibility, SWCSpectrum *We) const;
	float Pdf(const Point &p, const Normal &n, const Vector &wi) const;
	bool GetSamplePosition(const Point &p, const Vector &wi, float distance, float *x, float *y) const;
	void ClampRay(Ray &ray) const;
	bool IsDelta() const
	{
		return true;
	}
	BBox Bounds() const;

	EnvironmentCamera* Clone() const {
		return new EnvironmentCamera(*this);
	}

	static Camera *CreateCamera(const Transform &world2camStart, const Transform &world2camEnd, const ParamSet &params, Film *film);

private:
	Point pos;
};

}//namespace lux
