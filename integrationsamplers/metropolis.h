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

// metropolis.h*
#include "sampling.h"
#include "paramset.h"
#include "film.h"

class SampleVector {
public:
	mutable int i;
	SampleVector () : i(0) {}
	virtual float value (const int i, float defval) const = 0;
};

class MetroSample : public SampleVector {
public:
	mutable vector<float> values;
	mutable vector<int> modify;
	int time;
	MetroSample () : time(0) {}
	float mutate (const float x) const;
	float value (const int i, float defval) const;
	MetroSample next () const;
};

class Metropolis : public IntegrationSampler {
public:
	Metropolis() { L = 0.f; nmetro = 0; sum = 0.f; consec_rejects = 0; }
	void SetParams(int mR, float pL) { maxReject = mR; pLarge = pL; }
	void SetFilmRes(int fX, int fY) { xRes = fX, yRes = fY; }
	bool GetNextSample(Sampler *sampler, Sample *sample, u_int *use_pos);
	void GetNext(float& bs1, float& bs2, float& bcs, int pathLength);
	void AddSample(const Sample &sample, const Ray &ray,
		const Spectrum &L, float alpha, Film *film);

	MetroSample msamp, newsamp;
Spectrum L;
int xRes, yRes;
bool large;
float sum;
u_int nmetro;
int maxReject;
float pLarge;
Sample *mSample;
int consec_rejects;

};
