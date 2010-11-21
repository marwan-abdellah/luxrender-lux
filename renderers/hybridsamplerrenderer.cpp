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
#include "hybridsamplerrenderer.h"
#include "randomgen.h"
#include "context.h"

#include "luxrays/core/context.h"
#include "luxrays/core/virtualdevice.h"

using namespace lux;

#if !defined(LUXRAYS_DISABLE_OPENCL)

//------------------------------------------------------------------------------
// HybridSamplerRenderer
//------------------------------------------------------------------------------

HybridSamplerRenderer::HybridSamplerRenderer() : HybridRenderer() {
	state = INIT;

	// Create the LuxRays context
	ctx = new luxrays::Context(LuxRaysDebugHandler);

	// Create the device descriptions
	HRHostDescription *host = new HRHostDescription(this, "Localhost");
	hosts.push_back(host);

	// Add one virtual device to feed all the OpenCL devices
	host->AddDevice(new HRVirtualDeviceDescription(host, "VirtualGPU"));

	// Get the list of devices available
	std::vector<luxrays::DeviceDescription *> deviceDescs = std::vector<luxrays::DeviceDescription *>(ctx->GetAvailableDeviceDescriptions());

	// Add all the OpenCL devices
	for (size_t i = 0; i < deviceDescs.size(); ++i)
		host->AddDevice(new HRHardwareDeviceDescription(host, deviceDescs[i]));

	// Create the virtual device to feed all hardware device
	std::vector<luxrays::DeviceDescription *> hwDeviceDescs = deviceDescs;
	luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_OPENCL, hwDeviceDescs);
	luxrays::OpenCLDeviceDescription::Filter(luxrays::OCL_DEVICE_TYPE_GPU, hwDeviceDescs);

	if (hwDeviceDescs.size() < 1)
		throw std::runtime_error("Unable to find an OpenCL GPU device.");
	hwDeviceDescs.resize(1);

	ctx->AddVirtualM2OIntersectionDevices(0, hwDeviceDescs);

	virtualIDevice = ctx->GetVirtualM2OIntersectionDevices()[0];

	preprocessDone = false;
	suspendThreadsWhenDone = false;

	AddStringConstant(*this, "name", "Name of current renderer", "hybridsampler");
}

HybridSamplerRenderer::~HybridSamplerRenderer() {
	boost::mutex::scoped_lock lock(classWideMutex);

	if ((state != TERMINATE) && (state != INIT))
		throw std::runtime_error("Internal error: called HybridSamplerRenderer::~HybridSamplerRenderer() while not in TERMINATE or INIT state.");

	if (renderThreads.size() > 0)
		throw std::runtime_error("Internal error: called HybridSamplerRenderer::~HybridSamplerRenderer() while list of renderThread sis not empty.");

	delete ctx;

	for (size_t i = 0; i < hosts.size(); ++i)
		delete hosts[i];
}

Renderer::RendererType HybridSamplerRenderer::GetType() const {
	boost::mutex::scoped_lock lock(classWideMutex);

	return HYBRIDSAMPLER;
}

Renderer::RendererState HybridSamplerRenderer::GetState() const {
	boost::mutex::scoped_lock lock(classWideMutex);

	return state;
}

vector<RendererHostDescription *> &HybridSamplerRenderer::GetHostDescs() {
	boost::mutex::scoped_lock lock(classWideMutex);

	return hosts;
}

void HybridSamplerRenderer::SuspendWhenDone(bool v) {
	boost::mutex::scoped_lock lock(classWideMutex);
	suspendThreadsWhenDone = v;
}

void HybridSamplerRenderer::Render(Scene *s) {
	luxrays::DataSet *dataSet;

	{
		// Section under mutex
		boost::mutex::scoped_lock lock(classWideMutex);

		scene = s;

		if (scene->IsFilmOnly()) {
			state = TERMINATE;
			return;
		}

		if (scene->lights.size() == 0) {
			LOG( LUX_SEVERE,LUX_MISSINGDATA)<< "No light sources defined in scene; nothing to render.";
			state = TERMINATE;
			return;
		}

		if (!scene->surfaceIntegrator->IsDataParallelSupported()) {
			LOG( LUX_SEVERE,LUX_ERROR)<< "The SurfaceIntegrator doesn't support HybridSamplerRenderer.";
			state = TERMINATE;
			return;
		}

		if (!scene->surfaceIntegrator->CheckLightStrategy()) {
			state = TERMINATE;
			return;
		}

		state = RUN;

		// Initialize the stats
		lastSamples = 0.;
		lastTime = 0.;
		stat_Samples = 0.;
		stat_blackSamples = 0.;
		s_Timer.Reset();
	
		// Dade - I have to do initiliaziation here for the current thread.
		// It can be used by the Preprocess() methods.

		// initialize the thread's RandomGenerator
		lastUsedSeed = scene->seedBase - 1;
		LOG(LUX_INFO, LUX_NOERROR) << "Preprocess thread uses seed: " << lastUsedSeed;

		RandomGenerator rng(lastUsedSeed);

		// integrator preprocessing
		scene->sampler->SetFilm(scene->camera->film);
		scene->surfaceIntegrator->Preprocess(rng, *scene);
		scene->volumeIntegrator->Preprocess(rng, *scene);
		scene->camera->film->CreateBuffers();

		// Dade - to support autofocus for some camera model
		scene->camera->AutoFocus(*scene);

		//----------------------------------------------------------------------
		// Compile the scene geometries in a LuxRays compatible format
		//----------------------------------------------------------------------

		dataSet = HybridRenderer::PreprocessGeometry(ctx, scene);
        ctx->Start();

		// start the timer
		s_Timer.Start();

		// Dade - preprocessing done
		preprocessDone = true;
		Context::GetActive()->SceneReady();

		// add a thread
		CreateRenderThread();
	}

	if (renderThreads.size() > 0) {
		// The first thread can not be removed
		// it will terminate when the rendering is finished
		renderThreads[0]->thread->join();

		// rendering done, now I can remove all rendering threads
		{
			boost::mutex::scoped_lock lock(classWideMutex);

			// wait for all threads to finish their job
			for (u_int i = 0; i < renderThreads.size(); ++i) {
				renderThreads[i]->thread->join();
				delete renderThreads[i];
			}
			renderThreads.clear();

			// I change the current signal to exit in order to disable the creation
			// of new threads after this point
			state = TERMINATE;
		}

		// Flush the contribution pool
		scene->camera->film->contribPool->Flush();
		scene->camera->film->contribPool->Delete();
	}

	ctx->Stop();
	delete dataSet;
	scene->dataSet = NULL;
}

void HybridSamplerRenderer::Pause() {
	boost::mutex::scoped_lock lock(classWideMutex);
	state = PAUSE;
}

void HybridSamplerRenderer::Resume() {
	boost::mutex::scoped_lock lock(classWideMutex);
	state = RUN;
}

void HybridSamplerRenderer::Terminate() {
	boost::mutex::scoped_lock lock(classWideMutex);
	state = TERMINATE;
}

//------------------------------------------------------------------------------
// Statistic methods
//------------------------------------------------------------------------------

// Statistics Access
double HybridSamplerRenderer::Statistics(const string &statName) {
	if(statName=="secElapsed") {
		// Dade - s_Timer is inizialized only after the preprocess phase
		if (preprocessDone)
			return s_Timer.Time();
		else
			return 0.0;
	} else if(statName=="samplesSec")
		return Statistics_SamplesPSec();
	else if(statName=="samplesTotSec")
		return Statistics_SamplesPTotSec();
	else if(statName=="samplesPx")
		return Statistics_SamplesPPx();
	else if(statName=="efficiency")
		return Statistics_Efficiency();
	else if(statName=="displayInterval")
		return scene->DisplayInterval();
	else if(statName == "filmEV")
		return scene->camera->film->EV;
	else if(statName == "averageLuminance")
		return scene->camera->film->averageLuminance;
	else if(statName == "numberOfLocalSamples")
		return scene->camera->film->numberOfLocalSamples;
	else if (statName == "enoughSamples")
		return scene->camera->film->enoughSamplesPerPixel;
	else if (statName == "threadCount")
		return renderThreads.size();
	else {
		LOG( LUX_ERROR,LUX_BADTOKEN)<< "luxStatistics - requested an invalid data : " << statName;
		return 0.;
	}
}

double HybridSamplerRenderer::Statistics_GetNumberOfSamples() {
	if (s_Timer.Time() - lastTime > .5f) {
		boost::mutex::scoped_lock lock(classWideMutex);

		for (u_int i = 0; i < renderThreads.size(); ++i) {
			fast_mutex::scoped_lock lockStats(renderThreads[i]->statLock);
			stat_Samples += renderThreads[i]->samples;
			stat_blackSamples += renderThreads[i]->blackSamples;
			renderThreads[i]->samples = 0.;
			renderThreads[i]->blackSamples = 0.;
		}
	}

	return stat_Samples + scene->camera->film->numberOfSamplesFromNetwork;
}

double HybridSamplerRenderer::Statistics_SamplesPPx() {
	// divide by total pixels
	int xstart, xend, ystart, yend;
	scene->camera->film->GetSampleExtent(&xstart, &xend, &ystart, &yend);
	return Statistics_GetNumberOfSamples() / ((xend - xstart) * (yend - ystart));
}

double HybridSamplerRenderer::Statistics_SamplesPSec() {
	// Dade - s_Timer is inizialized only after the preprocess phase
	if (!preprocessDone)
		return 0.0;

	double samples = Statistics_GetNumberOfSamples();
	double time = s_Timer.Time();
	double dif_samples = samples - lastSamples;
	double elapsed = time - lastTime;
	lastSamples = samples;
	lastTime = time;

	// return current samples / sec total
	if (elapsed == 0.0)
		return 0.0;
	else
		return dif_samples / elapsed;
}

double HybridSamplerRenderer::Statistics_SamplesPTotSec() {
	// Dade - s_Timer is inizialized only after the preprocess phase
	if (!preprocessDone)
		return 0.0;

	double samples = Statistics_GetNumberOfSamples();
	double time = s_Timer.Time();

	// return current samples / total elapsed secs
	return samples / time;
}

double HybridSamplerRenderer::Statistics_Efficiency() {
	Statistics_GetNumberOfSamples();	// required before eff can be calculated.

	if (stat_Samples == 0.0)
		return 0.0;

	return (100.f * stat_blackSamples) / stat_Samples;
}

//------------------------------------------------------------------------------
// Private methods
//------------------------------------------------------------------------------

void HybridSamplerRenderer::CreateRenderThread() {
	if (scene->IsFilmOnly())
		return;

	// Avoid to create the thread in case signal is EXIT. For instance, it
	// can happen when the rendering is done.
	if ((state == RUN) || (state == PAUSE)) {
		// Add an instance to the LuxRays virtual device
		luxrays::IntersectionDevice * idev = virtualIDevice->AddVirtualDevice();

		RenderThread *rt = new  RenderThread(renderThreads.size(), this, idev);

		renderThreads.push_back(rt);
		rt->thread = new boost::thread(boost::bind(RenderThread::RenderImpl, rt));
	}
}

void HybridSamplerRenderer::RemoveRenderThread() {
	if (renderThreads.size() == 0)
		return;

	renderThreads.back()->thread->interrupt();
	renderThreads.back()->thread->join();
	delete renderThreads.back();
	renderThreads.pop_back();
}

//------------------------------------------------------------------------------
// RenderThread methods
//------------------------------------------------------------------------------

HybridSamplerRenderer::RenderThread::RenderThread(u_int index, HybridSamplerRenderer *r, luxrays::IntersectionDevice * idev) :
	n(index), thread(NULL), renderer(r), iDevice(idev), samples(0.), blackSamples(0.) {
}

HybridSamplerRenderer::RenderThread::~RenderThread() {
}

void HybridSamplerRenderer::RenderThread::RenderImpl(RenderThread *renderThread) {
	HybridSamplerRenderer *renderer = renderThread->renderer;
	Scene &scene(*(renderer->scene));
	if (scene.IsFilmOnly())
		return;

	// To avoid interrupt exception
	boost::this_thread::disable_interruption di;

	// Dade - wait the end of the preprocessing phase
	while (!renderer->preprocessDone) {
		boost::xtime xt;
		boost::xtime_get(&xt, boost::TIME_UTC);
		++xt.sec;
		boost::thread::sleep(xt);
	}

	// ContribBuffer has to wait until the end of the preprocessing
	// It depends on the fact that the film buffers have been created
	// This is done during the preprocessing phase
	ContributionBuffer *contribBuffer = new ContributionBuffer(scene.camera->film->contribPool);

	// initialize the thread's rangen
	u_long seed;
	{
		boost::mutex::scoped_lock lock(renderer->classWideMutex);
		renderer->lastUsedSeed++;
		seed = renderer->lastUsedSeed;
	}
	LOG(LUX_INFO, LUX_NOERROR) << "Thread " << renderThread->n << " uses seed: " << seed;

	RandomGenerator rng(seed);

	//luxrays::RayBuffer *rayBuffer = renderThread->iDevice->NewRayBuffer();
	luxrays::RayBuffer *rayBuffer = new luxrays::RayBuffer(8192);

	// Init all PathState
	const double t0 = luxrays::WallClockTime();
	vector<SurfaceIntegratorState *> integratorState(rayBuffer->GetSize());
	for (size_t i = 0; i < integratorState.size(); ++i) {
		integratorState[i] = scene.surfaceIntegrator->NewState(scene, contribBuffer, &rng);
		integratorState[i]->Init(scene);
	}

	LOG(LUX_DEBUG, LUX_NOERROR) << "Thread " << renderThread->n << " initialization time: " <<
			std::setiosflags(std::ios::fixed) << std::setprecision(2) <<
			luxrays::WallClockTime() - t0 << " secs";

	size_t currentGenerateIndex = 0;
	size_t currentNextIndex = 0;
	bool renderIsOver = false;
	while (!renderIsOver) {
		while (renderer->state == PAUSE) {
			boost::xtime xt;
			boost::xtime_get(&xt, boost::TIME_UTC);
			xt.sec += 1;
			boost::thread::sleep(xt);
		}
		if ((renderer->state == TERMINATE) || boost::this_thread::interruption_requested())
			break;

		while (rayBuffer->LeftSpace() > 0) {
			if (!scene.surfaceIntegrator->GenerateRays(scene, integratorState[currentGenerateIndex], rayBuffer)) {
				// The RayBuffer is full
				break;
			}

			currentGenerateIndex = (currentGenerateIndex + 1) % integratorState.size();
		}

		// Trace the RayBuffer
		renderThread->iDevice->PushRayBuffer(rayBuffer);
		rayBuffer = renderThread->iDevice->PopRayBuffer();

		// Advance the next step
		u_int nrContribs = 0;
		u_int nrSamples = 0;
		do {
			u_int count;
			if (scene.surfaceIntegrator->NextState(scene, integratorState[currentNextIndex], rayBuffer, &count)) {
				// The sample is finished
				++nrSamples;

				if (!integratorState[currentNextIndex]->Init(scene)) {
					renderer->Pause();

					// Dade - we have done, check what we have to do now
					if (renderer->suspendThreadsWhenDone) {
						// Dade - wait for a resume rendering or exit
						while (renderer->state == PAUSE) {
							boost::xtime xt;
							boost::xtime_get(&xt, boost::TIME_UTC);
							xt.sec += 1;
							boost::thread::sleep(xt);
						}

						if (renderer->state == TERMINATE) {
							renderIsOver = true;
							break;
						} else
							continue;
					} else {
						renderIsOver = true;
						break;
					}
				}
			}

			nrContribs += count;
			currentNextIndex = (currentNextIndex + 1) % integratorState.size();
		} while (currentNextIndex != currentGenerateIndex);

		// Jeanphi - Hijack statistics until volume integrator revamp
		{
			// update samples statistics
			fast_mutex::scoped_lock lockStats(renderThread->statLock);
			renderThread->blackSamples += nrContribs;
			renderThread->samples += nrSamples;
		}

		rayBuffer->Reset();
	}

	scene.camera->film->contribPool->End(contribBuffer);

	// Free memory
	for (size_t i = 0; i < integratorState.size(); ++i)
		delete integratorState[i];
	delete rayBuffer;
}

Renderer *HybridSamplerRenderer::CreateRenderer(const ParamSet &params) {
	return new HybridSamplerRenderer();
}

static DynamicLoader::RegisterRenderer<HybridSamplerRenderer> r("hybrid");
static DynamicLoader::RegisterRenderer<HybridSamplerRenderer> r2("hybridsampler");

#endif // !defined(LUXRAYS_DISABLE_OPENCL)
