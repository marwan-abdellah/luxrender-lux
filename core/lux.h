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
 *   Lux Renderer website : http://www.luxrender.org                       *
 ***************************************************************************/

#ifndef LUX_LUX_H
#define LUX_LUX_H

// lux.h*
// Global Include Files
#include <cmath>

#ifdef __CYGWIN__
#include <ieeefp.h>
#endif
#include <string>
using std::string;
#include <vector>
using std::vector;
#include <iostream>
using std::ostream;
#include <algorithm>
using std::min;
using std::max;
using std::swap;
using std::sort;

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/version.hpp>

#include "luxrays/core/utils.h"
#include "memory.h"

// boost version starting from 1.50 defined TIME_UTC_ instead of TIME_UTC because of a conflict with libc and c++ 2011
// https://svn.boost.org/trac/boost/ticket/6940
// glibc > 1.16 includes a TIME_UTC macro, so boost renamed to TIME_UTC_
//
// This hook allows to build with boost < 1.50 and glibc < 1.16
//
#if (BOOST_VERSION < 105000)
#define TIME_UTC_ TIME_UTC
#endif

// Platform-specific definitions
#if defined(WIN32) && !defined(__CYGWIN__)
#  include <float.h>
#  define isinf(f) (!_finite((f)))
#  if !defined(isnan)
#    define isnan(a) (_isnan(a))
#  endif
#  pragma warning (disable: 4244) // conversion from double to float (VS2005) - Radiance
#  pragma warning (disable: 4305) // truncation from double to float (VS2005) - Radiance
#  pragma warning (disable: 4996) // deprecated functions (VS2005) - Radiance
#  pragma warning (disable: 4267) // conversion from 'size_t' [asio\detail\socket_ops.hpp; boost\serialization\collections_save_imp.hpp] - zcott
#  pragma warning (disable: 4311) // pointer truncation from 'void *' to 'long' [Fl_Widget.H; Fl_Menu_Item.H;; asio\detail\win_iocp_socket_service.hpp] - zcott
#  pragma warning (disable : 4312) // conversion from 'long' to 'void *' of greater size [Fl_Widget.H; Fl_Menu_Item.H; asio\detail\win_iocp_socket_service.hpp] - zcott
//note: the above are duplicated in compiler options, kept here for reference only - zcott
#  pragma warning (disable: 4267 4251 4065 4102)
#  pragma warning (disable: 4190) // extern "C" nonsense when returning a template
#  pragma warning (disable: 4290) // C++ exception specification ignored except to indicate a function is not __declspec(nothrow) ; pointless warning
#  pragma warning (disable: 4355) // 'this' used in base member initializer list
//#define WIN32_LEAN_AND_MEAN //defined in project properties
#  include <windows.h>

#include <boost/cstdint.hpp>
using boost::int8_t;
using boost::int16_t;
using boost::int32_t;
using boost::int64_t;
using boost::uint8_t;
using boost::uint16_t;
using boost::uint32_t;
using boost::uint64_t;

inline float atanhf(float x) {
	// if outside of domain, return NaN
	// not 100% correct but should be good for now
	if(x <= -1.f || x >= 1.f) 
		return sqrtf(-1.f); 
  
	return logf((1.f + x) / (1.f - x)) / 2.f;
}


namespace w32util
{
# include <stdio.h>
# include <wincon.h>
#define FOREGROUND_YELLOW FOREGROUND_RED | FOREGROUND_GREEN
#define FOREGROUND_WHITE FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE
inline void ChangeConsoleColor(WORD col)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// keep user-defined background and foreground intensity
	CONSOLE_SCREEN_BUFFER_INFO screenBufferInfo;
	GetConsoleScreenBufferInfo(hConsole, &screenBufferInfo);
	col |= screenBufferInfo.wAttributes & static_cast<WORD>(FOREGROUND_INTENSITY | BACKGROUND_INTENSITY);

	SetConsoleTextAttribute(hConsole, col);
}
}
#else
using std::isinf;
using std::isnan;
#endif
#if defined (__INTEL_COMPILER) && !defined(WIN32)
// Dade - to fix a problem with expf undefined with Intel CC
inline float expf(float a) { return exp(a); }
#endif

// Global Constants
#ifdef M_PI
#  undef M_PI
#endif
#define M_PI           3.14159265358979323846f
#define INV_PI  0.31830988618379067154f
#define INV_TWOPI  0.15915494309189533577f
#ifndef INFINITY
#  define INFINITY HUGE_VAL
//#define INFINITY std::numeric_limits<float>::max()
#endif
#if defined(WIN32) && !defined(__CYGWIN__)
#  define LUX_PATH_SEP ";"
#else
#  define LUX_PATH_SEP ":"
#endif

// Global Type Declarations
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
#define BC_GRID_SIZE 40
typedef vector<int> SampleGrid[BC_GRID_SIZE][BC_GRID_SIZE];
#define GRID(v) (int((v) * BC_GRID_SIZE))

// Global Forward Declarations
class Timer;

namespace luxrays {
  class BBox;
  class MachineEpsilon;
  class Matrix4x4;
  class Normal;
  class Point;
  class Ray;
  class Transform;
  class Vector;
}

namespace lux
{
  class ParamSet;
  template <class T> struct ParamSetItem;
  using luxrays::BBox;
  using luxrays::MachineEpsilon;
  using luxrays::Matrix4x4;
  using luxrays::Normal;
  using luxrays::Point;
  using luxrays::Ray;
  using luxrays::Transform;
  using luxrays::Vector;
  class RayDifferential;
  class DifferentialGeometry;
  class Renderer;
  class Scene;
  class Primitive;
  class AreaLightPrimitive;
  class InstancePrimitive;
  class MotionPrimitive;
  class Aggregate;
  class Intersection;
  class ImageData;
  class MIPMap;
  class SWCSpectrum;
  class SpectrumWavelengths;
  class RGBColor;
  class XYZColor;
  class ColorSystem;
  class SPD;
  class Camera;
  class CameraResponse;
  class ProjectiveCamera;
  class Sampler;
  class PixelSampler;
  class IntegrationSampler;
  class Sample;
  class Filter;
  class Film;
  class ToneMap;
  class BxDF;
  class BRDF;
  class BTDF;
  class Fresnel;
  class FresnelConductor;
  class FresnelDielectric;
  class FresnelGeneral;
  class FresnelGeneric;
  class FresnelNoOp;
  class FresnelSlick;
  class SpecularReflection;
  class SpecularTransmission;
  class Lambertian;
  class OrenNayar;
  class Microfacet;
  class MicrofacetDistribution;
  class BSDF;
  class Material;
  struct CompositingParams;
  class TextureMapping2D;
  class UVMapping2D;
  class SphericalMapping2D;
  class CylindricalMapping2D;
  class PlanarMapping2D;
  class TextureMapping3D;
  class IdentityMapping3D;
  class TriangleMesh;
  class PlyMesh;
  template <class T> class Texture;
  class Volume;
  class Region;
  class Light;
  struct VisibilityTester;
  class AreaLight;
  class Shape;
  class PrimitiveSet;
  class Integrator;
  class SurfaceIntegrator;
  class VolumeIntegrator;
  class RandomGenerator;
  class RenderFarm;
  class Contribution;
  class ContributionBuffer;
  class ContributionPool;
  class ContributionSystem;
  class InterpolatedTransform;
  class MotionSystem;
  class MotionTransform;
  class Distribution1D;
  class Distribution2D;
  class IrregularDistribution1D;
  class SampleableSphericalFunction;

  class Context;

// Global Function Declarations
  //string hashing function
  unsigned int DJBHash(const std::string& str);

  // accepts platform-specific filenames and performs fallback
  ImageData *ReadImage(const string &name);

  // converts paths to portable format and 
  // provides fallback mechanism for missing files
  string AdjustFilename(const string filename, bool silent = false);
}

// Global Inline Functions
template<class T> inline T Lerp(float t, T v1, T v2) {
	return v1 + t * (v2 - v1);
}
template<class T> inline T Clamp(T val, T low, T high) {
	return val > low ? (val < high ? val : high) : low;
}
inline int Round2Int(double val) {
	return static_cast<int>(val > 0. ? val + .5 : val - .5);
}
inline int Round2Int(float val) {
	return static_cast<int>(val > 0.f ? val + .5f : val - .5f);
}
inline u_int Round2UInt(double val) {
	return static_cast<u_int>(val > 0. ? val + .5 : 0.);
}
inline u_int Round2UInt(float val) {
	return static_cast<u_int>(val > 0.f ? val + .5f : 0.f);
}
inline int Mod(int a, int b) {
	// note - radiance - added 0 check to prevent divide by zero error(s)
	if (b == 0)
		b = 1;

	a %= b;
	if (a < 0)
		a += b;
	return a;
}
inline float Radians(float deg) {
	return (M_PI / 180.f) * deg;
}
inline float Degrees(float rad) {
	return (180.f / M_PI) * rad;
}
inline float Log2(float x) {
	return logf(x) / logf(2.f);
}
inline int Log2Int(float v) {
	return Round2Int(Log2(v));
}
inline bool IsPowerOf2(int v) {
	return (v & (v - 1)) == 0;
}
inline bool IsPowerOf2(u_int v) {
	return (v & (v - 1)) == 0;
}
inline u_int RoundUpPow2(u_int v) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	return v+1;
}
template<class T> inline int Float2Int(T val) {
	return static_cast<int>(val);
}
template<class T> inline u_int Float2UInt(T val) {
	return val >= 0 ? static_cast<u_int>(val) : 0;
}
inline int Floor2Int(double val) {
	return static_cast<int>(floor(val));
}
inline int Floor2Int(float val) {
	return static_cast<int>(floorf(val));
}
inline u_int Floor2UInt(double val) {
	return val > 0. ? static_cast<u_int>(floor(val)) : 0;
}
inline u_int Floor2UInt(float val) {
	return val > 0.f ? static_cast<u_int>(floorf(val)) : 0;
}
inline int Ceil2Int(double val) {
	return static_cast<int>(ceil(val));
}
inline int Ceil2Int(float val) {
	return static_cast<int>(ceilf(val));
}
inline u_int Ceil2UInt(double val) {
	return val > 0. ? static_cast<u_int>(ceil(val)) : 0;
}
inline u_int Ceil2UInt(float val) {
	return val > 0.f ? static_cast<u_int>(ceilf(val)) : 0;
}
inline bool Quadratic(float A, float B, float C, float *t0, float *t1) {
	// Find quadratic discriminant
	float discrim = B * B - 4.f * A * C;
	if (discrim < 0.f)
		return false;
	float rootDiscrim = sqrtf(discrim);
	// Compute quadratic _t_ values
	float q;
	if (B < 0)
		q = -.5f * (B - rootDiscrim);
	else
		q = -.5f * (B + rootDiscrim);
	*t0 = q / A;
	*t1 = C / q;
	if (*t0 > *t1)
		swap(*t0, *t1);
	return true;
}
inline float SmoothStep(float min, float max, float value) {
	float v = Clamp((value - min) / (max - min), 0.f, 1.f);
	return v * v * (-2.f * v  + 3.f);
}

template <class T> int SignOf(T x)
{
	return (x > 0) - (x < 0);
}

#endif // LUX_LUX_H
