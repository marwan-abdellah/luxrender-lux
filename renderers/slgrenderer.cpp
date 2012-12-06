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

#include <iomanip>

#include "api.h"
#include "scene.h"
#include "camera.h"
#include "film.h"
#include "sampling.h"
#include "slgrenderer.h"
#include "context.h"
#include "renderers/statistics/slgstatistics.h"

#include "luxrays/core/context.h"
#include "luxrays/utils/core/exttrianglemesh.h"
#include "luxrays/opencl/utils.h"
#include "rendersession.h"
#include "cameras/perspective.h"

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <fstream>

using namespace lux;

//------------------------------------------------------------------------------
// SLGHostDescription
//------------------------------------------------------------------------------

SLGHostDescription::SLGHostDescription(SLGRenderer *r, const string &n) : renderer(r), name(n) {
	SLGDeviceDescription *desc = new SLGDeviceDescription(this, "Test");
	devs.push_back(desc);
}

SLGHostDescription::~SLGHostDescription() {
	for (size_t i = 0; i < devs.size(); ++i)
		delete devs[i];
}

void SLGHostDescription::AddDevice(SLGDeviceDescription *devDesc) {
	devs.push_back(devDesc);
}

//------------------------------------------------------------------------------
// SLGRenderer
//------------------------------------------------------------------------------

SLGRenderer::SLGRenderer(const luxrays::Properties &config) : Renderer() {
	state = INIT;

	SLGHostDescription *host = new SLGHostDescription(this, "Localhost");
	hosts.push_back(host);

	preprocessDone = false;
	suspendThreadsWhenDone = false;

	AddStringConstant(*this, "name", "Name of current renderer", "slg");

	rendererStatistics = new SLGStatistics(this);

	overwriteConfig = config;
}

SLGRenderer::~SLGRenderer() {
	boost::mutex::scoped_lock lock(classWideMutex);

	delete rendererStatistics;

	if ((state != TERMINATE) && (state != INIT))
		throw std::runtime_error("Internal error: called SLGRenderer::~SLGRenderer() while not in TERMINATE or INIT state.");

	for (size_t i = 0; i < hosts.size(); ++i)
		delete hosts[i];
}

Renderer::RendererState SLGRenderer::GetState() const {
	boost::mutex::scoped_lock lock(classWideMutex);

	return state;
}

vector<RendererHostDescription *> &SLGRenderer::GetHostDescs() {
	boost::mutex::scoped_lock lock(classWideMutex);

	return hosts;
}

void SLGRenderer::SuspendWhenDone(bool v) {
	boost::mutex::scoped_lock lock(classWideMutex);
	suspendThreadsWhenDone = v;
}

luxrays::sdl::Scene *SLGRenderer::CreateSLGScene() {
	luxrays::sdl::Scene *slgScene = new luxrays::sdl::Scene();

	PerspectiveCamera *perpCamera = dynamic_cast<PerspectiveCamera *>(scene->camera);
	if (!perpCamera)
		throw std::runtime_error("SLGRenderer supports only PerspectiveCamera");

	//--------------------------------------------------------------------------
	// Setup the camera
	//--------------------------------------------------------------------------

	const Point orig(
			(*perpCamera)["Position.x"].FloatValue(),
			(*perpCamera)["Position.y"].FloatValue(),
			(*perpCamera)["Position.z"].FloatValue());
	const Point target= orig + Vector(
			(*perpCamera)["Normal.x"].FloatValue(),
			(*perpCamera)["Normal.y"].FloatValue(),
			(*perpCamera)["Normal.z"].FloatValue());
	const Vector up(
			(*perpCamera)["Up.x"].FloatValue(),
			(*perpCamera)["Up.y"].FloatValue(),
			(*perpCamera)["Up.z"].FloatValue());

	slgScene->CreateCamera(
		"scene.camera.lookat = " + 
			boost::lexical_cast<string>(orig.x) + " " +
			boost::lexical_cast<string>(orig.y) + " " +
			boost::lexical_cast<string>(orig.z) + " " +
			boost::lexical_cast<string>(target.x) + " " +
			boost::lexical_cast<string>(target.y) + " " +
			boost::lexical_cast<string>(target.z) + "\n"
		"scene.camera.up = " +
			boost::lexical_cast<string>(up.x) + " " +
			boost::lexical_cast<string>(up.y) + " " +
			boost::lexical_cast<string>(up.z) + "\n"
		"scene.camera.fieldofview = " + boost::lexical_cast<string>(Degrees((*perpCamera)["fov"].FloatValue())) + "\n"
		"scene.camera.lensradius = " + boost::lexical_cast<string>(Degrees((*perpCamera)["LensRadius"].FloatValue())) + "\n"
		"scene.camera.focaldistance = " + boost::lexical_cast<string>(Degrees((*perpCamera)["FocalDistance"].FloatValue())) + "\n"
		);

	//--------------------------------------------------------------------------
	// Setup materials
	//--------------------------------------------------------------------------

	slgScene->AddMaterials(
		"scene.materials.matte.mat_white = 0.75 0.75 0.75\n"
		);

	//--------------------------------------------------------------------------
	// Setup lights
	//--------------------------------------------------------------------------

	// Create a SkyLight & SunLight
	slgScene->AddSkyLight(
			"scene.skylight.dir = 0.0 0.0 1.0\n"
			"scene.skylight.turbidity = 2.2\n"
			"scene.skylight.gain = 0.8 0.8 0.8\n"
			);
	slgScene->AddSunLight(
			"scene.sunlight.dir = 0.0 0.0 1.0\n"
			"scene.sunlight.turbidity = 2.2\n"
			"scene.sunlight.gain = 0.8 0.8 0.8\n"
			);

	//--------------------------------------------------------------------------
	// Convert geometry
	//--------------------------------------------------------------------------

	LOG(LUX_INFO, LUX_NOERROR) << "Tesselating " << scene->primitives.size() << " primitives";

	u_int objNumber = 0;
	for (size_t i = 0; i < scene->primitives.size(); ++i) {
		vector<luxrays::ExtTriangleMesh *> meshList;
		scene->primitives[i]->ExtTesselate(&meshList, &scene->tesselatedPrimitives);

		for (vector<luxrays::ExtTriangleMesh *>::const_iterator mesh = meshList.begin(); mesh != meshList.end(); ++mesh) {
			if (!(*mesh)->HasNormals()) {
				// SLG requires shading normals
				(*mesh)->ComputeNormals();
			}

			const string objName = "Object" + boost::lexical_cast<string>(objNumber);
			slgScene->DefineObject(objName, *mesh);
			slgScene->AddObject(objName, "mat_white",
					"scene.objects.mat_white." + objName + ".useplynormals = 1\n"
					);
			++objNumber;
		}
	}

	if (slgScene->objects.size() == 0)
		throw std::runtime_error("SLGRenderer can not render an empty scene");

	return slgScene;
}

luxrays::Properties SLGRenderer::CreateSLGConfig() {
	std::stringstream ss;

	ss << "renderengine.type = PATHOCL\n"
			"sampler.type = INLINED_RANDOM\n"
			"opencl.platform.index = -1\n"
			"opencl.cpu.use = 0\n"
			"opencl.gpu.use = 1\n"
			//"opencl.devices.select = 1101\n"
			;

	Film *film = scene->camera->film;
	int xStart, xEnd, yStart, yEnd;
	film->GetSampleExtent(&xStart, &xEnd, &yStart, &yEnd);
	const int imageWidth = xEnd - xStart;
	const int imageHeight = yEnd - yStart;

	float cropWindow[4] = {
		(*film)["cropWindow.0"].FloatValue(),
		(*film)["cropWindow.1"].FloatValue(),
		(*film)["cropWindow.2"].FloatValue(),
		(*film)["cropWindow.3"].FloatValue()
	};
	if ((cropWindow[0] != 0.f) || (cropWindow[1] != 1.f) ||
			(cropWindow[2] != 0.f) || (cropWindow[3] != 1.f))
		throw std::runtime_error("SLGRenderer doesn't yet support border rendering");

	ss << "image.width = " + boost::lexical_cast<string>(imageWidth) + "\n"
			"image.height = " + boost::lexical_cast<string>(imageHeight) + "\n";

	luxrays::Properties config;
	config.LoadFromString(ss.str());

	// Add overwrite properties
	config.Load(overwriteConfig);

	return config;
}

void SLGRenderer::Render(Scene *s) {
	luxrays::sdl::Scene *slgScene = NULL;
	luxrays::Properties slgConfigProps;

	{
		// Section under mutex
		boost::mutex::scoped_lock lock(classWideMutex);

		scene = s;

		if (scene->IsFilmOnly()) {
			state = TERMINATE;
			return;
		}

		if (scene->lights.size() == 0) {
			LOG(LUX_SEVERE, LUX_MISSINGDATA) << "No light sources defined in scene; nothing to render.";
			state = TERMINATE;
			return;
		}

		state = RUN;

		// Initialize the stats
		rendererStatistics->reset();
	
		// Dade - I have to do initiliaziation here for the current thread.
		// It can be used by the Preprocess() methods.

		// initialize the thread's rangen
		u_long seed = scene->seedBase - 1;
		LOG(LUX_DEBUG, LUX_NOERROR) << "Preprocess thread uses seed: " << seed;

		RandomGenerator rng(seed);

		// integrator preprocessing
		scene->sampler->SetFilm(scene->camera->film);
		scene->surfaceIntegrator->Preprocess(rng, *scene);
		scene->volumeIntegrator->Preprocess(rng, *scene);
		scene->camera->film->CreateBuffers();

		scene->surfaceIntegrator->RequestSamples(scene->sampler, *scene);
		scene->volumeIntegrator->RequestSamples(scene->sampler, *scene);

		// Dade - to support autofocus for some camera model
		scene->camera->AutoFocus(*scene);

		// TODO: extend SLG library to accept an handler for each rendering session
		luxrays::sdl::LuxRaysSDLDebugHandler = SDLDebugHandler;

		try {
			// Build the SLG scene to render
			slgScene = CreateSLGScene();

			// Build the SLG rendering configuration
			slgConfigProps.Load(CreateSLGConfig());
#if !defined(LUXRAYS_DISABLE_OPENCL)
		} catch (cl::Error err) {
			LOG(LUX_SEVERE, LUX_SYSTEM) << "OpenCL ERROR: " << err.what() << "(" << luxrays::utils::oclErrorString(err.err()) << ")";
			state = TERMINATE;
			return;
#endif
		} catch (std::runtime_error err) {
			LOG(LUX_SEVERE, LUX_SYSTEM) << "RUNTIME ERROR: " << err.what();
			state = TERMINATE;
			return;
		} catch (std::exception err) {
			LOG(LUX_SEVERE, LUX_SYSTEM) << "ERROR: " << err.what();
			state = TERMINATE;
			return;
		}

		// start the timer
		rendererStatistics->start();

		// Dade - preprocessing done
		preprocessDone = true;
		scene->SetReady();
	}

	//----------------------------------------------------------------------
	// Do the render
	//----------------------------------------------------------------------

	slg::RenderConfig *config = new slg::RenderConfig(slgConfigProps, *slgScene);
	slg::RenderSession *session = new slg::RenderSession(config);
	slg::RenderEngine *engine = session->renderEngine;

	unsigned int haltTime = config->cfg.GetInt("batch.halttime", 0);
	unsigned int haltSpp = config->cfg.GetInt("batch.haltspp", 0);
	float haltThreshold = config->cfg.GetFloat("batch.haltthreshold", -1.f);

	// Start the rendering
	session->Start();
	const double startTime = luxrays::WallClockTime();

	double lastFilmUpdate = startTime;
	char buf[512];
	Film *film = scene->camera->film;
	int xStart, xEnd, yStart, yEnd;
	film->GetSampleExtent(&xStart, &xEnd, &yStart, &yEnd);
	const luxrays::utils::Film *slgFilm = session->film; 
	for (;;) {
		if (state == PAUSE) {
			session->BeginEdit();
			while (state == PAUSE && !boost::this_thread::interruption_requested())
				boost::this_thread::sleep(boost::posix_time::seconds(1));
			session->EndEdit();
		}
		if ((state == TERMINATE) || boost::this_thread::interruption_requested())
			break;

		boost::this_thread::sleep(boost::posix_time::millisec(1000));

		// Film update may be required by some render engine to
		// update statistics, convergence test and more
		if (luxrays::WallClockTime() - lastFilmUpdate > film->getldrDisplayInterval()) {
			session->renderEngine->UpdateFilm();

			// Update LuxRender film too
			// TODO: use Film write mutex
			ColorSystem colorSpace = film->GetColorSpace();
			for (int y = yStart; y <= yEnd; ++y) {
				for (int x = xStart; x <= xEnd; ++x) {
					const luxrays::utils::SamplePixel *sp = slgFilm->GetSamplePixel(
						luxrays::utils::PER_PIXEL_NORMALIZED, x - xStart, y - yStart);
					const float alpha = slgFilm->IsAlphaChannelEnabled() ?
						(slgFilm->GetAlphaPixel(x - xStart, y - yStart)->alpha) : 0.f;

					XYZColor xyz = colorSpace.ToXYZ(RGBColor(sp->radiance.r, sp->radiance.g, sp->radiance.b));
					// Flip the image upside down
					Contribution contrib(x, yEnd - 1 - y, xyz, alpha, 0.f, sp->weight);
					film->SetSample(&contrib);
				}
			}
			film->SetSampleCount(session->renderEngine->GetTotalSampleCount());

			lastFilmUpdate =  luxrays::WallClockTime();
		}

		const double now = luxrays::WallClockTime();
		const double elapsedTime = now - startTime;
		const unsigned int pass = engine->GetPass();
		// Convergence test is update inside UpdateFilm()
		const float convergence = engine->GetConvergence();
		if (((film->enoughSamplesPerPixel)) ||
				((haltTime > 0) && (elapsedTime >= haltTime)) ||
				((haltSpp > 0) && (pass >= haltSpp)) ||
				((haltThreshold >= 0.f) && (1.f - convergence <= haltThreshold))) {
			
			if (suspendThreadsWhenDone) {
				// Dade - wait for a resume rendering or exit
				Pause();
				while (state == PAUSE)
					boost::this_thread::sleep(boost::posix_time::millisec(1000));

				if (state == TERMINATE)
					break;
				else {
					// Cancel all halt conditions
					haltTime = 0;
					haltSpp = 0;
					haltThreshold = -1.f;
				}
			} else {
				Terminate();
				break;
			}
			break;
		}

		// Print some information about the rendering progress
		sprintf(buf, "[Elapsed time: %3d/%dsec][Samples %4d/%d][Convergence %f%%][Avg. samples/sec % 3.2fM on %.1fK tris]",
				int(elapsedTime), int(haltTime), pass, haltSpp, 100.f * convergence, engine->GetTotalSamplesSec() / 1000000.0,
				config->scene->dataSet->GetTotalTriangleCount() / 1000.0);

		SLG_LOG(buf);

		film->CheckWriteOuputInterval();
	}

	// Stop the rendering
	session->Stop();

	delete session;
	SLG_LOG("Done.");

	// I change the current signal to exit in order to disable the creation
	// of new threads after this point
	Terminate();

	// Flush the contribution pool
	scene->camera->film->contribPool->Flush();
	scene->camera->film->contribPool->Delete();
}

void SLGRenderer::Pause() {
	boost::mutex::scoped_lock lock(classWideMutex);
	state = PAUSE;
	rendererStatistics->stop();
}

void SLGRenderer::Resume() {
	boost::mutex::scoped_lock lock(classWideMutex);
	state = RUN;
	rendererStatistics->start();
}

void SLGRenderer::Terminate() {
	boost::mutex::scoped_lock lock(classWideMutex);
	state = TERMINATE;
}

//------------------------------------------------------------------------------

void DebugHandler(const char *msg) {
	LOG(LUX_DEBUG, LUX_NOERROR) << "[LuxRays] " << msg;
}

void SDLDebugHandler(const char *msg) {
	LOG(LUX_DEBUG, LUX_NOERROR) << "[LuxRays::SDL] " << msg;
}

void SLGDebugHandler(const char *msg) {
	LOG(LUX_DEBUG, LUX_NOERROR) << "[SLG] " << msg;
}

Renderer *SLGRenderer::CreateRenderer(const ParamSet &params) {
	luxrays::Properties config;

	// Local (for network rendering) host configuration file. It is a properties
	// file that can be used overwrite settings.
	const string configFile = params.FindOneString("configfile", "");
	if (configFile != "")
		config.LoadFromFile(configFile);

	// A list of properties that can be used to overwrite generated properties
	u_int nItems;
	const string *items = params.FindString("config", &nItems);
	if (items) {
		for (u_int i = 0; i < nItems; ++i)
			config.LoadFromString(items[i] + "\n");
	}

	return new SLGRenderer(config);
}

static DynamicLoader::RegisterRenderer<SLGRenderer> r("slg");