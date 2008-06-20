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

#include <boost/bind.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/thread.hpp>
#include <boost/cast.hpp>

#include <sstream>
#include <clocale>

#include "lux.h"
#include "api.h"
#include "error.h"

#include "wx/app.h"
#include "wx/filedlg.h"
#include "wx/filename.h"
#include "wx/dcbuffer.h"
#include "wx/splash.h"
#include <boost/date_time/posix_time/posix_time.hpp>

#include "wxluxgui.h"
#include "wxglviewer.h"
#include "wximages.h"

using namespace lux;

/*** LuxGui ***/

DEFINE_EVENT_TYPE(lux::wxEVT_LUX_ERROR)
DEFINE_EVENT_TYPE(lux::wxEVT_LUX_PARSEERROR)
DEFINE_EVENT_TYPE(lux::wxEVT_LUX_FINISHED)
DEFINE_EVENT_TYPE(lux::wxEVT_LUX_TONEMAPPED)

BEGIN_EVENT_TABLE(LuxGui, wxFrame)
	EVT_LUX_ERROR (wxID_ANY, LuxGui::OnError)
	EVT_TIMER     (wxID_ANY, LuxGui::OnTimer)
	EVT_SPINCTRL  (wxID_ANY, LuxGui::OnSpin)
	EVT_COMMAND   (wxID_ANY, lux::wxEVT_LUX_TONEMAPPED, LuxGui::OnCommand)
	EVT_COMMAND   (wxID_ANY, lux::wxEVT_LUX_PARSEERROR, LuxGui::OnCommand)
	EVT_COMMAND   (wxID_ANY, lux::wxEVT_LUX_FINISHED, LuxGui::OnCommand)
	EVT_ICONIZE   (LuxGui::OnIconize)
END_EVENT_TABLE()

LuxGui::LuxGui(wxWindow* parent, bool opengl):LuxMainFrame(parent), m_opengl(opengl) {
	// Load images and icons from header.
	LoadImages();

	// Add custom output viewer window
	if(m_opengl)
		m_renderOutput = new LuxGLViewer(m_renderPage);
	else
		m_renderOutput = new LuxOutputWin(m_renderPage);
	m_renderPage->GetSizer()->Add(m_renderOutput, 1, wxALL | wxEXPAND, 5);
	m_renderPage->Layout();

	// Trick to generate resize event and show output window
	// http://lists.wxwidgets.org/pipermail/wx-users/2007-February/097829.html
	SetSize(GetSize());
	m_renderOutput->Update();

	// Create render output update timer
	m_renderTimer = new wxTimer(this, ID_RENDERUPDATE);
	m_statsTimer = new wxTimer(this, ID_STATSUPDATE);
	m_loadTimer = new wxTimer(this, ID_LOADUPDATE);

	m_numThreads = 0;
	m_engineThread = NULL;
	m_updateThread = NULL;

	luxErrorHandler(&LuxGuiErrorHandler);

	ChangeRenderState(WAITING);
}

void LuxGui::ChangeRenderState(LuxGuiRenderState state) {
	switch(state) {
		case WAITING:
			// Waiting for input file. Most controls disabled.
			m_file->Enable(wxID_OPEN, true);
			m_render->Enable(ID_RESUMEITEM, false);
			m_render->Enable(ID_STOPITEM, false);
			m_renderToolBar->EnableTool(ID_RESUMETOOL, false);
			m_renderToolBar->EnableTool(ID_STOPTOOL, false);
			m_threadSpinCtrl->Disable();
			break;
		case RENDERING:
			// Rendering is in progress.
			m_file->Enable(wxID_OPEN, false);
			m_render->Enable(ID_RESUMEITEM, false);
			m_render->Enable(ID_STOPITEM, true);
			m_renderToolBar->EnableTool(ID_RESUMETOOL, false);
			m_renderToolBar->EnableTool(ID_STOPTOOL, true);
			m_threadSpinCtrl->Enable();
			break;
		case IDLE:
			// Rendering is paused.
			m_file->Enable(wxID_OPEN, false);
			m_render->Enable(ID_RESUMEITEM, true);
			m_render->Enable(ID_STOPITEM, false);
			m_renderToolBar->EnableTool(ID_RESUMETOOL, true);
			m_renderToolBar->EnableTool(ID_STOPTOOL, false);
			m_threadSpinCtrl->Enable();
			break;
		case FINISHED:
			// Rendering is finished.
			m_file->Enable(wxID_OPEN, false);
			m_render->Enable(ID_RESUMEITEM, false);
			m_render->Enable(ID_STOPITEM, false);
			m_renderToolBar->EnableTool(ID_RESUMETOOL, false);
			m_renderToolBar->EnableTool(ID_STOPTOOL, false);
			m_threadSpinCtrl->Disable();
			break;
	}
	m_guiRenderState = state;
}

void LuxGui::LoadImages() {
	wxImage::AddHandler(new wxPNGHandler());

	// App icon
	wxIcon appIcon;
	appIcon.CopyFromBitmap(wxMEMORY_BITMAP(luxicon_png));
	SetIcon(appIcon);

	// wxMac has problems changing an existing tool's icon, so we remove and add then again...
	// Resume toolbar tool
	wxToolBarToolBase *rendertool = m_renderToolBar->RemoveTool(ID_RESUMETOOL);
	rendertool->SetNormalBitmap(wxMEMORY_BITMAP(resume_png));
	m_renderToolBar->InsertTool(0, rendertool);

	// Stop toolbar tool
	wxToolBarToolBase *stoptool = m_renderToolBar->RemoveTool(ID_STOPTOOL);
	stoptool->SetNormalBitmap(wxMEMORY_BITMAP(stop_png));
	m_renderToolBar->InsertTool(1, stoptool);
	m_renderToolBar->Realize();

	// wxGTK has problems changing an existing menu item's icon, so we remove and add then again...
	// Resume menu item
	wxMenuItem *renderitem = m_render->Remove(ID_RESUMEITEM);
	renderitem->SetBitmap(wxMEMORY_BITMAP(resume_png));
	m_render->Insert(0,renderitem);
	// Stop menu item
	wxMenuItem *stopitem = m_render->Remove(ID_STOPITEM);
	stopitem->SetBitmap(wxMEMORY_BITMAP(stop_png));
	m_render->Insert(1,stopitem);

	m_auinotebook->SetPageBitmap(0, wxMEMORY_BITMAP(render_png));
	m_auinotebook->SetPageBitmap(1, wxMEMORY_BITMAP(info_png));
	m_auinotebook->SetPageBitmap(2, wxMEMORY_BITMAP(output_png));

	m_splashbmp = wxMEMORY_BITMAP(splash_png);
}

void LuxGui::OnMenu(wxCommandEvent& event) {
	switch (event.GetId()) {
		case ID_RESUMEITEM:
		case ID_RESUMETOOL:
			if(m_guiRenderState != RENDERING) {
				// Start display update timer
				m_renderOutput->Refresh();
				m_renderTimer->Start(1000*luxStatistics("displayInterval"), wxTIMER_CONTINUOUS);
				m_statsTimer->Start(1000, wxTIMER_CONTINUOUS);
				if(m_guiRenderState == IDLE) // Only re-start if we were previously stopped
					luxStart();
				ChangeRenderState(RENDERING);
			}
			break;
		case ID_STOPITEM:
		case ID_STOPTOOL:
			if(m_guiRenderState != IDLE) {
				// Stop display update timer
				m_renderTimer->Stop();
				m_statsTimer->Stop();
				if(m_guiRenderState == RENDERING)
					luxPause();
				ChangeRenderState(IDLE);
			}
			break;
		case wxID_ABOUT:
			new wxSplashScreen(m_splashbmp, wxSPLASH_CENTRE_ON_PARENT, 0, this, -1);
			break;
		case wxID_EXIT:
			Close(false);
			break;
		default:
			break;
	}
}

void LuxGui::OnOpen(wxCommandEvent& event) {
	wxFileDialog filedlg(this,
	                     _("Choose a file to open"),
											 wxEmptyString,
											 wxEmptyString,
											 _("LuxRender scene files (*.lxs)|*.lxs|All files (*.*)|*.*"),
											 wxFD_OPEN | wxFD_FILE_MUST_EXIST);

	if (filedlg.ShowModal() == wxID_OK)
		RenderScenefile(filedlg.GetPath());
}

void LuxGui::OnExit(wxCloseEvent& event) {
	//if we have a scene file
  if(m_guiRenderState != WAITING) {
		if(m_updateThread)
			m_updateThread->join();

		luxExit();

		if(m_engineThread)
			m_engineThread->join();

		luxError(LUX_NOERROR, LUX_INFO, "Freeing resources.");
		luxCleanup();
	}

	Destroy();
}

void LuxGui::OnError(wxLuxErrorEvent &event) {
	std::stringstream ss("");
	ss << boost::posix_time::second_clock::local_time() << ' ';
	switch(event.GetError()->GetSeverity()) {
		case LUX_INFO:
			ss << "Info: ";	break;
		case LUX_WARNING:
			ss << "Warning: ";	break;
		case LUX_ERROR:
			ss << "Error: ";	break;
		case LUX_SEVERE:
			ss << "Severe error: ";	break;
	}
	ss << "(" << event.GetError()->GetCode() << ") ";
	ss << event.GetError()->GetMessage() << std::endl;
	m_logTextCtrl->AppendText(wxString::FromAscii(ss.str().c_str()));
	m_logTextCtrl->ShowPosition(m_logTextCtrl->GetLastPosition());
}

void LuxGui::OnTimer(wxTimerEvent& event) {
	switch (event.GetId()) {
		case ID_RENDERUPDATE:
			//if(m_guiWindowState == SHOWN && luxStatistics("sceneIsReady")) {
			if(luxStatistics("sceneIsReady")) { // Radiance - note - temporary fix to prevent gui from not updating at all on win32
				luxError(LUX_NOERROR, LUX_INFO, "GUI: Updating framebuffer...");
				m_statusBar->SetStatusText(wxT("Tonemapping..."), 0);
				m_updateThread = new boost::thread(boost::bind(&LuxGui::UpdateThread, this));
			}
			break;
		case ID_STATSUPDATE:
			if(luxStatistics("sceneIsReady"))
				UpdateStatistics();
			break;
		case ID_LOADUPDATE:
			m_progDialog->Pulse();
			if(luxStatistics("sceneIsReady") || m_guiRenderState == FINISHED) {
				m_progDialog->Destroy();
				m_loadTimer->Stop();

				if(luxStatistics("sceneIsReady")) {
					// Scene file loaded
					// Add other render threads if necessary
					int curThreads = 1;
					while(curThreads < m_numThreads) {
						luxAddThread();
						curThreads++;
					}

					// Start updating the display by faking a resume menu item click.
					wxCommandEvent startEvent(wxEVT_COMMAND_MENU_SELECTED, ID_RESUMEITEM);
					GetEventHandler()->AddPendingEvent(startEvent);
				}
			}
			break;
	}
}

void LuxGui::OnSpin(wxSpinEvent& event) {
	SetRenderThreads(event.GetPosition());
}

void LuxGui::OnCommand(wxCommandEvent &event) {
	if(event.GetEventType() == wxEVT_LUX_TONEMAPPED) {
		m_statusBar->SetStatusText(wxT(""), 0);
		m_renderOutput->Refresh();
	} else if(event.GetEventType() == wxEVT_LUX_PARSEERROR) {
		wxMessageBox(wxT("Scene file parse error.\nSee log for details."), wxT("Error"), wxOK | wxICON_ERROR, this);
		ChangeRenderState(FINISHED);
	} else if(event.GetEventType() == wxEVT_LUX_FINISHED) {
		//wxMessageBox(wxT("Rendering is finished."), wxT("LuxRender"), wxOK | wxICON_INFORMATION, this);
		ChangeRenderState(FINISHED);
	}
}

void lux::LuxGui::OnIconize( wxIconizeEvent& event )
{
	if(!event.Iconized())
		m_guiWindowState = SHOWN;
	else
		m_guiWindowState = HIDDEN;
}

void LuxGui::RenderScenefile(wxString filename) {
	wxFileName fn(filename);
	SetTitle(wxT("LuxRender - ")+fn.GetName());

	// Start main render thread
	m_engineThread = new boost::thread(boost::bind(&LuxGui::EngineThread, this, filename));

	m_progDialog = new wxProgressDialog(wxT("Loading..."), wxT(""), 100, NULL, wxSTAY_ON_TOP);
	m_progDialog->Pulse();
	m_loadTimer->Start(1000, wxTIMER_CONTINUOUS);
}

void LuxGui::EngineThread(wxString filename) {
	boost::filesystem::path fullPath(boost::filesystem::initial_path());
	fullPath = boost::filesystem::system_complete(boost::filesystem::path(filename.fn_str(), boost::filesystem::native));

	chdir(fullPath.branch_path().string().c_str());

	ParseFile(fullPath.leaf().c_str());

	if(luxStatistics("sceneIsReady") == false) {
		wxCommandEvent errorEvent(wxEVT_LUX_PARSEERROR, GetId());
		GetEventHandler()->AddPendingEvent(errorEvent);

		luxWait();
	} else {
		luxWait();

	  luxError(LUX_NOERROR, LUX_INFO, "Rendering done.");
		wxCommandEvent endEvent(wxEVT_LUX_FINISHED, GetId());
		GetEventHandler()->AddPendingEvent(endEvent);
	}
}

void LuxGui::UpdateThread() {
	luxUpdateFramebuffer();
	wxCommandEvent endEvent(wxEVT_LUX_TONEMAPPED, GetId());
	GetEventHandler()->AddPendingEvent(endEvent);
}

void LuxGui::SetRenderThreads(int num) {
	if(luxStatistics("sceneIsReady")) {
		if(num > m_numThreads) {
			for(; num > m_numThreads; m_numThreads++)
				luxAddThread();
		} else {
			for(; num < m_numThreads; m_numThreads--)
				luxRemoveThread();
		}
	} else {
		m_numThreads = num;
	}
	m_threadSpinCtrl->SetValue(m_numThreads);
}

void LuxGui::UpdateStatistics() {
	int samplesSec = Floor2Int(luxStatistics("samplesSec"));
	int samplesTotSec = Floor2Int(luxStatistics("samplesTotSec"));
	int secElapsed = Floor2Int(luxStatistics("secElapsed"));
	double samplesPx = luxStatistics("samplesPx");
	int efficiency = Floor2Int(luxStatistics("efficiency"));

	int secs = (secElapsed) % 60;
	int mins = (secElapsed / 60) % 60;
	int hours = (secElapsed / 3600);

	wxString stats;
	stats.Printf(wxT("%02d:%02d:%02d - %d S/s - %d TotS/s - %.2f S/px - %i%% eff"),
	             hours, mins, secs, samplesSec, samplesTotSec, samplesPx, efficiency);
	m_statusBar->SetStatusText(stats, 1);
}

/*** LuxOutputWin ***/

BEGIN_EVENT_TABLE(LuxOutputWin, wxWindow)
    EVT_PAINT (LuxOutputWin::OnPaint)
END_EVENT_TABLE()

LuxOutputWin::LuxOutputWin(wxWindow *parent)
      : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, -1)) {
}

void LuxOutputWin::OnDraw(wxDC &dc) {
	if (luxStatistics("sceneIsReady")) {
		int w = luxStatistics("filmXres"), h = luxStatistics("filmYres");
		SetVirtualSize(w, h);
		SetScrollRate(1,1);
		unsigned char* fb = luxFramebuffer();
		dc.DrawBitmap(wxBitmap(wxImage(w, h, fb, true)), 0, 0, false);
	}
}

/*** LuxGuiErrorHandler wrapper ***/

void lux::LuxGuiErrorHandler(int code, int severity, const char *msg) {
	boost::shared_ptr<LuxError> error(new LuxError(code, severity, msg));
	wxLuxErrorEvent errorEvent(error, wxEVT_LUX_ERROR);
	wxTheApp->GetTopWindow()->GetEventHandler()->AddPendingEvent(errorEvent);
}

