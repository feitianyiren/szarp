/*
  SZARP: SCADA software
  

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/
#include "wx/config.h"
#include <wx/numdlg.h>
#include <wx/xrc/xmlres.h>
#include <wx/tokenzr.h>
#include <map>
#include "cconv.h"

#include "version.h"
#include "szhlpctrl.h"
#include "szframe.h"

#include "ids.h"
#include "classes.h"

#include "drawtime.h"
#include "dbinquirer.h"
#include "database.h"
#include "draw.h"
#include "coobs.h"
#include "drawsctrl.h"
#include "cfgmgr.h"
#include "defcfg.h"

#include "drawobs.h"
#include "drawapp.h"
#include "frmmgr.h"
#include "drawpick.h"
#include "drawpnl.h"
#include "drawtime.h"
#include "cfgdlg.h"
#include "drawurl.h"
#include "errfrm.h"
#include "paramslist.h"
#include "remarks.h"
#include "drawfrm.h"
#include "drawprint.h"
#include "probadddiag.h"

DrawFrame::DrawFrame(FrameManager * fm, DatabaseManager* dm, ConfigManager* cm, RemarksHandler *remarks, wxWindow * parent,
		     wxWindowID id, const wxString & title, const wxString & name, const wxPoint & pos,
		     const wxSize & size, long style)
:szFrame(parent, id, title, pos, size, style | wxFRAME_FLOAT_ON_PARENT, name), m_name(name), frame_manager(fm), config_manager(cm),
database_manager(dm), m_notebook(NULL), draw_panel(NULL), remarks_handler(remarks)
{

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(sizer);

	menu_bar = (wxMenuBar*) wxXmlResource::Get()->LoadObject(this,_T("menubar"),_T("wxMenuBar"));

	SetMenuBar(menu_bar);

	params_dialog = NULL;

	ignore_menu_events = false;

	panel_is_added = false;
}

void DrawFrame::OnAbout(wxCommandEvent & event)
{
	wxGetApp().ShowAbout();
}

void DrawFrame::OnHelp(wxCommandEvent & event)
{
	wxGetApp().DisplayHelp();
}

class NumberOfUnitsDialog : public wxDialog {
	wxSpinCtrl *m_spin_ctrl;
public:
	NumberOfUnitsDialog(wxWindow *parent, PeriodType pt, size_t initial_count);
	size_t GetNumberOfUnits();
};

NumberOfUnitsDialog::NumberOfUnitsDialog(wxWindow *parent, PeriodType pt, size_t initial_count) : wxDialog(parent, wxWindowID(wxID_ANY), wxString(_("Number of displayed values"))) {
	wxSizer* sizer = new wxBoxSizer(wxVERTICAL);
	wxString text;
	int min, max;
	switch (pt) {
		case PERIOD_T_DECADE:
			text = _("Select number of displayed years");
			min = 1;
			max = 36;
			break;
		case PERIOD_T_YEAR:
			text = _("Select number of displayed months");
			min = 1;
			max = 36;
			break;
		case PERIOD_T_MONTH:
			text = _("Select number of displayed days");
			min = 1;
			max = 62;
			break;
		case PERIOD_T_WEEK:
			min = 1;
			max = 21;
			text = _("Select number of displayed days");
			break;
		case PERIOD_T_DAY:
			min = 1;
			max = 72;
			text = _("Select number of displayed hours");
			break;
		case PERIOD_T_30MINUTE:
			min = 1;
			max = 50;
			text = _("Select number of displayed minutes");
			break;
		case PERIOD_T_SEASON:
			min = 1;
			max = 80;
			text = _("Select number of displayed weeks");
			break;
		default:
			assert(false);
	}

	sizer->Add(new wxStaticText(this, wxID_ANY, text), 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);
	sizer->Add(new wxStaticLine(this, wxID_ANY), 0, wxALL | wxEXPAND, 5);

	m_spin_ctrl = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, min, max);
	m_spin_ctrl->SetValue(initial_count);

	sizer->Add(m_spin_ctrl, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);
	sizer->Add(new wxStaticLine(this, wxID_ANY), 0, wxALL | wxEXPAND, 5);
	sizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_CENTER_HORIZONTAL, 5);

	SetSizer(sizer);
	sizer->Fit(this);
}

size_t NumberOfUnitsDialog::GetNumberOfUnits() {
	return m_spin_ctrl->GetValue();
}

void DrawFrame::OnNumberOfUnits(wxCommandEvent &event) {
	NumberOfUnitsDialog* dlg = new NumberOfUnitsDialog(this, draw_panel->GetPeriod(), draw_panel->GetNumberOfUnits());
	if (dlg->ShowModal() == wxID_OK)
		draw_panel->SetNumberOfUnits(dlg->GetNumberOfUnits());
	dlg->Destroy();
}

void DrawFrame::OnContextHelp(wxCommandEvent & event) {
	wxContextHelp ch(this);
}

void DrawFrame::OnExit(wxCommandEvent & event)
{
	Close();
}

void DrawFrame::OnRefresh(wxCommandEvent & event)
{
	if (draw_panel)
		draw_panel->OnRefresh(event);
}
void DrawFrame::OnFind(wxCommandEvent & event)
{
	draw_panel->StartDrawSearch();
}

void DrawFrame::OnSetParams(wxCommandEvent & evt)
{
	draw_panel->StartPSC();
}

void DrawFrame::OnSave(wxCommandEvent & event)
{
	if (config_manager->SaveDefinedDrawsSets())
		wxMessageBox(_("Defined sets saved succesfully."), _("Information."), 
				wxICON_INFORMATION, this);
}

void DrawFrame::OnEdit(wxCommandEvent & event)
{
	if (draw_panel->IsUserDefined())  {
		DrawPicker *dp = new DrawPicker(this, config_manager, database_manager);
		dp->EditSet(dynamic_cast<DefinedDrawSet*>(draw_panel->GetSelectedSet()), draw_panel->GetPrefix());
		dp->Destroy();
	} else
		wxMessageBox(_("You may edit only user defined sets."),
			     _("Not user defined"), wxOK | wxICON_ERROR, this);

}

void DrawFrame::OnEditSetAsNew(wxCommandEvent &e) {
	DrawPicker *dp = new DrawPicker(this, config_manager, database_manager);
	if (dp->EditAsNew(draw_panel->GetSelectedSet(), draw_panel->GetPrefix()) == wxID_OK) {
		DrawsSets* dss = config_manager->GetConfigByPrefix(draw_panel->GetPrefix());
		DrawSet* ds = dss->GetDrawsSets()[dp->GetNewSetName()];
		draw_panel->SelectSet(ds);
	}
	dp->Destroy();
}

void DrawFrame::OnImportSet(wxCommandEvent &event) {
	config_manager->ImportSet();
}

void DrawFrame::OnExportSet(wxCommandEvent &event) {
	wxString name, password, server;
	bool autofetch;
	if (remarks_handler->Configured())
		remarks_handler->GetConfiguration(name, password, server, autofetch);
	config_manager->ExportSet(dynamic_cast<DefinedDrawSet*>(draw_panel->GetSelectedSet()), name);
}

void DrawFrame::OnDel(wxCommandEvent & event)
{
	if (!draw_panel->IsUserDefined())
		wxMessageBox(_("You may only remove user defined sets."),
			     _("Not user defined"), wxOK | wxICON_ERROR, this);
	else {
		int answer = wxMessageBox(_("Do you really want to remove this set?"),
			     _("Set removal"), wxYES_NO | wxICON_QUESTION, this);
		if (answer != wxYES)
			return;
		DefinedDrawsSets *d = config_manager->GetDefinedDrawsSets();
		d->RemoveSet(draw_panel->GetSelectedSet()->GetName());
	}

}

void DrawFrame::OnAdd(wxCommandEvent& event)
{
	DrawPicker *dp = new DrawPicker(this, config_manager, database_manager);
	if (dp->NewSet(draw_panel->GetPrefix()) == wxID_OK) {
		DrawsSets* dss = config_manager->GetConfigByPrefix(draw_panel->GetPrefix());
		DrawSet* ds = dss->GetDrawsSets()[dp->GetNewSetName()];
		draw_panel->SelectSet(ds);
	}
	dp->Destroy();
}

void DrawFrame::OnLoadConfig(wxCommandEvent & event)
{
	if (event.GetId() == XRCID("NewTab"))
		frame_manager->LoadConfig(this);
	else if (event.GetId() == XRCID("NewWindow"))
		frame_manager->LoadConfig(NULL);
	else
		assert(false);

}


void DrawFrame::SetTitle(const wxString &title, const wxString &prefix)
{
	wxFrame::SetTitle(wxString(_("SZARP Draw")) + _T(" - ") + title + _T(" (") + prefix + _T(")"));
}

bool DrawFrame::AddDrawPanel(const wxString & prefix, const wxString& set, PeriodType pt, time_t time, int selected_draw)
{
	DrawsSets *config = config_manager->GetConfigByPrefix(prefix);

	if (config == NULL)
		return false;

	SortedSetsArray sorted = config->GetSortedDrawSetsNames();
	int count = sorted.size();

	if (count <= 0) {
		wxMessageDialog dlg(this,_("No sets in configuration"),_("No sets"), 
				wxOK | wxICON_ERROR);
		dlg.ShowModal();
		return false;
	}

	wxWindow *parent = NULL;

	wxSizer *sizer = GetSizer();

	panel_is_added = true;

	if (draw_panel != NULL && m_notebook == NULL) {

		m_notebook =
		    new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition,
				   draw_panel->GetSize(), wxAUI_NB_WINDOWLIST_BUTTON | wxAUI_NB_TAB_MOVE | wxAUI_NB_TOP | wxAUI_NB_CLOSE_ON_ACTIVE_TAB | wxAUI_NB_SCROLL_BUTTONS);

		m_notebook->SetHelpText(_T("draw3-base-tabs"));

		sizer->Detach(draw_panel);

		draw_panel->Reparent(m_notebook);

		m_notebook->AddPage(draw_panel, draw_panel->GetConfigName(), true);

		sizer->Add(m_notebook, 1, wxEXPAND);

		parent = m_notebook;
	} else if (draw_panel == NULL)
		parent = this;
	else
		parent = m_notebook;

	DrawPanel *new_panel = new DrawPanel(database_manager,
			config_manager,
			remarks_handler,
			menu_bar,
			config->GetPrefix(),
			set,
			pt,
			time,
			parent,
			wxID_ANY,
			this, selected_draw);

	if (draw_panel)
		draw_panel->SetActive(false);

	draw_panel = new_panel;

	draw_panel->SetActive(true);

	if (m_notebook) {
		wxString title = GetTitleForPanel(config->GetID(), -1);

		m_notebook->AddPage(new_panel, title, true);

	} else {
		sizer->Add(new_panel, 1, wxEXPAND);
		draw_panel->SetFocus();
	}
	SetTitle(new_panel->GetConfigName(),  new_panel->GetPrefix());

	panel_is_added = false;

	return true;
}

void DrawFrame::DetachFromNotebook()
{
	assert(m_notebook);
	assert(m_notebook->GetPageCount() == 1);

	wxSizer *sizer = GetSizer();
	wxWindow *page = m_notebook->GetPage(0);

	draw_panel = wxDynamicCast(page, DrawPanel);

	m_notebook->RemovePage(0);

	draw_panel->Reparent(this);

	sizer->Detach(m_notebook);
	sizer->Add(draw_panel, 1, wxEXPAND);
	sizer->Layout();

	sizer->Show((size_t) 0, true);
	draw_panel->SetFocus();

	m_notebook->Destroy();
	m_notebook = NULL;

	SetTitle(draw_panel->GetConfigName(), draw_panel->GetPrefix());
}

void DrawFrame::OnNotebookPageClose(wxAuiNotebookEvent &event)
{
}

void DrawFrame::CloseTab(int sel)
{
	if (sel == -1)
		return;

	wxWindow *page = m_notebook->GetPage(sel);

	DrawPanel *panel = wxDynamicCast(page, DrawPanel);

	assert(panel != NULL);

	m_notebook->RemovePage(sel);

	panel->Destroy();

}

void DrawFrame::OnCloseTab(wxCommandEvent & evt)
{
	if (m_notebook == NULL)
		Close();
	else
		CloseTab(m_notebook->GetSelection());

}

void DrawFrame::OnSummaryWindowCheck(wxCommandEvent & event)
{
	if (ignore_menu_events == false)
	        draw_panel->ShowSummaryWindow(event.IsChecked());
}

void DrawFrame::RemovePanel(DrawPanel *panel) {
	panels_to_remove.push_back(panel);
}

void DrawFrame::OnNotebookSelectionChange(wxAuiNotebookEvent& event)
{
	if (m_notebook->GetPageCount() == 1) {
		return;
	}

	int i = m_notebook->GetSelection();
	if (i < 0) {
		SetTitle(_T(""), _T(""));
		return;
	}

	if (draw_panel)
		draw_panel->SetActive(false);

	wxWindow *page = m_notebook->GetPage(i);
	draw_panel = wxDynamicCast(page, DrawPanel);

	ignore_menu_events = true;
	draw_panel->SetActive(true);
	ignore_menu_events = false;

	draw_panel->SetFocus();

	SetTitle(draw_panel->GetConfigName(), draw_panel->GetPrefix());

	event.Skip();
}

void DrawFrame::OnClose(wxCloseEvent & event)
{
	frame_manager->ProcessEvent(event);
}

void DrawFrame::OnJumpToDate(wxCommandEvent &)
{
	draw_panel->OnJumpToDate();
}

void DrawFrame::OnPrint(wxCommandEvent &event) {
	draw_panel->Print(false);
}

void DrawFrame::OnPrintPreview(wxCommandEvent &event) {
	draw_panel->Print(true);
}

void DrawFrame::OnNumberOfAxes(wxCommandEvent &event) {
	long int c;

	wxConfigBase *cfg = wxConfig::Get();
	cfg->Read(_T("MaxPrintedAxesNumber"), &c, 3L);

	int ret;
	ret = wxGetNumberFromUser(
				_("Maximum number of printed vertical axes"),
				_("Number of axes:"),
				_("Printing"),
				c,
				1,
				5,
				this);

	if (ret == -1)
		return;

	cfg->Write(_T("MaxPrintedAxesNumber"), int(ret));
	cfg->Flush();

}

void DrawFrame::OnFilterChange(wxCommandEvent &event) {
	if (ignore_menu_events == false)
		draw_panel->OnFilterChange(event);
}

void DrawFrame::OnPieWin(wxCommandEvent &event) {
	if (ignore_menu_events == false)
		draw_panel->ShowPieWindow(event.IsChecked());
}

void DrawFrame::OnRelWin(wxCommandEvent &event) {
	if (ignore_menu_events == false)
		draw_panel->ShowRelWindow(event.IsChecked());
}

void DrawFrame::OnXYDialog(wxCommandEvent &event) {
	wxString prefix = draw_panel->GetPrefix();
	TimeInfo ti = draw_panel->GetCurrentTimeInfo();
	std::vector<DrawInfo*> user_draws = draw_panel->GetDrawInfoList();
	frame_manager->CreateXYGraph(prefix,ti,user_draws);
}

void DrawFrame::OnXYZDialog(wxCommandEvent &event) {
	wxString prefix = draw_panel->GetPrefix();
	std::vector<DrawInfo*> user_draws = draw_panel->GetDrawInfoList();
	TimeInfo ti = draw_panel->GetCurrentTimeInfo();
	frame_manager->CreateXYZGraph(prefix,ti,user_draws);
}

void DrawFrame::OnStatDialog(wxCommandEvent &event) {
	wxString prefix = draw_panel->GetPrefix();
	std::vector<DrawInfo*> user_draws = draw_panel->GetDrawInfoList();
	TimeInfo ti = draw_panel->GetCurrentTimeInfo();
	frame_manager->ShowStatDialog(prefix,ti,user_draws);
}

bool DrawFrame::Destroy()
{
	return wxFrame::Destroy();
}

void DrawFrame::OnRemarks(wxCommandEvent &event) {
	draw_panel->ShowRemarks();
}

void DrawFrame::OnFullScreen(wxCommandEvent &event) {
	ShowFullScreen(event.IsChecked(), wxFULLSCREEN_NOBORDER | wxFULLSCREEN_NOCAPTION | wxFULLSCREEN_NOSTATUSBAR);
}

void DrawFrame::OnSplitCursor(wxCommandEvent &event) {
	if (ignore_menu_events == false)
		draw_panel->ToggleSplitCursor(event);
}

void DrawFrame::OnLatestDataFollow(wxCommandEvent &event) {
	if (ignore_menu_events == false)
		draw_panel->SetLatestDataFollow(event.IsChecked());
}

void DrawFrame::SwitchFullScreen() {
	wxMenuItem *item = GetMenuBar()->FindItem(XRCID("FullScreen"));
	if (IsFullScreen()) {
		item->Check(false);
		ShowFullScreen(false);
	} else {
		item->Check(true);
		ShowFullScreen(true, wxFULLSCREEN_NOBORDER | wxFULLSCREEN_NOCAPTION | wxFULLSCREEN_NOSTATUSBAR);
	}
}

void DrawFrame::SelectNextTab() {
	if (m_notebook == NULL)
		return;

	int i = m_notebook->GetSelection() + 1;
	if (i >= (int)m_notebook->GetPageCount())
		return;

	m_notebook->SetSelection(i);
}

void DrawFrame::SelectPreviousTab() {
	if (m_notebook == NULL)
		return;

	int i = m_notebook->GetSelection();
	if (i <= 0)
		return;

	m_notebook->SetSelection(i - 1);
}

void DrawFrame::RemovePendingPanels() {
	if (panels_to_remove.size() == 0)
		return;

	for (std::vector<DrawPanel*>::iterator i = panels_to_remove.begin();
			i != panels_to_remove.end();
			++i) {

		if (m_notebook) {
			int idx = m_notebook->GetPageIndex(*i);
			if (idx != wxNOT_FOUND) {
				m_notebook->RemovePage(idx);
				(*i)->Destroy();
			}

			if (m_notebook->GetPageCount() == 1)
				DetachFromNotebook();
		} else {
			if (draw_panel != *i)
				continue;

			wxSizer *sizer = GetSizer();
			sizer->Detach(draw_panel);

			draw_panel->Destroy();
			draw_panel = NULL;

		}

	}

	panels_to_remove.resize(0);
}

void DrawFrame::OnIdle(wxIdleEvent &event) {
	if (panel_is_added)
		return;
	if (m_notebook && m_notebook->GetPageCount() == 1)
		DetachFromNotebook();
}

void DrawFrame::OnCopy(wxCommandEvent &event) {
	draw_panel->Copy();
}

void DrawFrame::OnPaste(wxCommandEvent &event) {
	draw_panel->Paste();
}

void DrawFrame::OnClearCache(wxCommandEvent &event) {
	draw_panel->ClearCache();
}

void DrawFrame::OnImportDraw2Def(wxCommandEvent &event) {
	DefinedDrawsSets *d = config_manager->GetDefinedDrawsSets();
	d->LoadDraw2File();
}

void DrawFrame::OnReloadConfig(wxCommandEvent &event) {
	if (config_manager->ReloadConfiguration(draw_panel->GetPrefix()) == true) 
		return;

	wxMessageBox(wxString::Format(_("Configuration '%s' cannot be reloaded, probably new version of this configuration contatins errors."), draw_panel->GetConfigName().c_str()), 
			_("Error"),
			wxICON_HAND, this);

}

void DrawFrame::OnErrorFrame(wxCommandEvent &event) {
	ErrorFrame::ShowFrame();
}

void DrawFrame::OnAverageChange(wxCommandEvent& event) {
	if (ignore_menu_events)
		return;

	PeriodType pt;
	int id = event.GetId();
	if (id == XRCID("DECADE_RADIO"))
		pt = PERIOD_T_DECADE;
	else if (id == XRCID("YEAR_RADIO"))
		pt = PERIOD_T_YEAR;
	else if (id == XRCID("MONTH_RADIO"))
		pt = PERIOD_T_MONTH;
	else if (id == XRCID("WEEK_RADIO"))
		pt = PERIOD_T_WEEK;
	else if (id == XRCID("DAY_RADIO"))
		pt = PERIOD_T_DAY;
	else if (id == XRCID("30MINUTE_RADIO"))
		pt = PERIOD_T_30MINUTE;
	else
		pt = PERIOD_T_SEASON;

	draw_panel->SetPeriod(pt);
}

DrawFrame::~DrawFrame() {
	if (params_dialog)
		params_dialog->Destroy();
}

void
DrawFrame::SaveLayout() {

	bool with_infinity = true;

	if(m_notebook == NULL) {
		wxConfig::Get()->Write(_T("DrawFramePanels_") + m_name, draw_panel->GetUrl(with_infinity));
		wxConfig::Get()->Write(_T("DrawFrameActivePanel_") + m_name, 0);
	} else {
		wxString tmp = wxEmptyString;
		for(unsigned int i = 0; i < m_notebook->GetPageCount(); i++, tmp += _T(";") ) {
			DrawPanel * panel = dynamic_cast<DrawPanel *> ( m_notebook->GetPage(i) );
			tmp += panel->GetUrl(with_infinity);
		}

		wxConfig::Get()->Write(_T("DrawFramePanels_") + m_name, tmp);
		wxConfig::Get()->Write(_T("DrawFrameActivePanel_") + m_name, m_notebook->GetSelection());
	}

}



bool
DrawFrame::LoadLayout() {

	wxString urls;
	long selection;

	if(!wxConfig::Get()->Read(_T("DrawFramePanels_") + m_name, &urls))
		return false;
	if(!wxConfig::Get()->Read(_T("DrawFrameActivePanel_") + m_name, &selection)) //Not used any more...
		return false;

	wxStringTokenizer tok(urls, _T(";"));

	bool requested_prefix_present = false;

	int idx = 0;
	while(tok.HasMoreTokens()) {

		wxString prefix;
		wxString window;
		PeriodType period;
		time_t time;
		int selected_draw;

		bool success = true;

		wxString tmp = tok.GetNextToken();

		try {

		if (decode_url(tmp, prefix, window, period, time, selected_draw, true)) {

			DrawsSets* ds = config_manager->GetConfigByPrefix(prefix);

			if (ds != NULL) {
				AddDrawPanel(prefix, window, period, time, selected_draw);
				if (!requested_prefix_present && prefix == m_name){
					requested_prefix_present = true;
					selection = idx;
				}

			} else
				success = false;

		} else
			success = false;

		} catch (std::runtime_error &e) {
			success = false;
		}

		if (success) {
			idx++;
		}

	}

	if (idx == 0) {
		return false;
	}

	if (idx > 1) {
		m_notebook->SetSelection(selection % m_notebook->GetPageCount());
		draw_panel->SetFocus();
	}

	return requested_prefix_present;

}

void DrawFrame::OnUserParams(wxCommandEvent &evt) {
	if (params_dialog == NULL)
		params_dialog = new ParamsListDialog(this, config_manager->GetDefinedDrawsSets(), database_manager, false);

	params_dialog->SetCurrentPrefix(draw_panel->GetPrefix());
	params_dialog->ShowModal();
}

void DrawFrame::OnLanguageChange(wxCommandEvent &e) {
	wxString lang = wxConfig::Get()->Read(_T("LANGUAGE"), DEFAULT_LANGUAGE);

	typedef std::map<wxString, wxString> SMSS;
	SMSS mapLang;

	// IMPORTANT: sorted by short name
	mapLang[_T("en")] =  _("English");
	mapLang[_T("fr")] =  _("French");
	mapLang[_T("de")] =  _("German");
	mapLang[_T("hu")] =  _("Hungarian");
	mapLang[_T("it")] =  _("Italian");
	mapLang[_T("pl")] =  _("Polish");
	mapLang[_T("sr")] =  _("Serbian");

	wxArrayString choices;

	// init array choices
	for (SMSS::iterator it = mapLang.begin(); it != mapLang.end(); ++it)
		choices.push_back(it->second);

	wxString caption = _("Current language is ");
	caption += mapLang[lang].Len() ? mapLang[lang] : wxString(_("default language"));

	int ret = wxGetSingleChoiceIndex(_("Choose language"), caption, choices, this);
	if (ret == -1)
		return;

	// get a short language name - ugly but better than switch :)
	wxString nl;
	for (SMSS::iterator it = mapLang.begin(); it != mapLang.end(); ++it)
		if (ret == 0) 
		{
			nl = it->first;
			break;
		}
		else
			--ret;
	
	if (nl == lang)
		return;

	wxConfig::Get()->Write(_T("LANGUAGE"), nl);

	wxMessageBox(_("Current language has been changed. You have to restart draw3"), 
			_("Language changed"), wxOK, this);
}

void DrawFrame::OnGraphsView(wxCommandEvent &e) {
	wxString style = wxConfig::Get()->Read(_T("GRAPHS_VIEW"), _T("GCDC"));

	wxArrayString choices;

	choices.push_back(_("Classic"));
	choices.push_back(_("'3D'"));
	choices.push_back(_("Antialiased"));

	int selected;

	if (style == _T("'3D'"))
		selected = 1;
	else if (style == _T("GCDC"))
		selected = 2;
	else
		selected = 0;

	choices[selected] = choices[selected] + _T(" (") + _("currently selected") + _T(")");
	
	int ret = wxGetSingleChoiceIndex(_("Choose graphs window style"), style, choices, this);
	if (ret == -1)
		return;

	if (ret == 2) {
		if (style != _T("GCDC"))
			wxMessageBox(_("You need to restart application for this change to take effect."), _("Graphs view changed."), wxICON_INFORMATION, this);
		style = _T("GCDC");
	} else if (ret == 1) {
		if (style != _T("3D"))
			wxMessageBox(_("You need to restart application for this change to take effect."), _("Graphs view changed."), wxICON_INFORMATION, this);
		style = _T("3D");
	} else {
		if (style != _T("Classic"))
			wxMessageBox(_("You need to restart application for this change to take effect."), _("Graphs view changed."), wxICON_INFORMATION, this);
		style = _T("Classic");
	}

	wxConfig::Get()->Write(_T("GRAPHS_VIEW"), style);
	wxConfig::Get()->Flush();

}

void DrawFrame::OnAddRemark(wxCommandEvent &event) {
	if (!remarks_handler->Configured()) {
		wxMessageBox(_("Remarks sending not configured, You should set username, password, and remarks server adddress"), _("Remarks not configured."), wxICON_HAND, this);
		OnConfigureRemarks(event);
	}

	if (!remarks_handler->Configured())
		return;

	RemarkViewDialog* d = new RemarkViewDialog(this, remarks_handler);

	DrawInfo *di;
	wxDateTime time;
	draw_panel->GetDisplayedDrawInfo(&di, time);

	d->NewRemark(di->GetBasePrefix(),
			config_manager->GetConfigTitles()[di->GetBasePrefix()],
			di->GetSetName(),
			time);

	delete d;

}

void DrawFrame::OnFetchRemarks(wxCommandEvent &event) {
	if (!remarks_handler->Configured()) {
		wxMessageBox(_("Remarks sending not configured, You should set username, password, and remarks server adddress"), _("Remarks not configured."), wxICON_HAND, this);
		OnConfigureRemarks(event);
	}

	if (!remarks_handler->Configured())
		return;

	RemarksConnection* connection = remarks_handler->GetConnection();
	connection->FetchNewRemarks();
}

class RemarksConfigurationDialog : public wxDialog {
	wxString& m_username;
	wxString& m_password;
	wxString& m_server;
	bool &m_auto_fetch;	
public:
	RemarksConfigurationDialog(wxWindow *parent, wxString &username, wxString &password, wxString &server, bool &auto_fetch);
	void OnOK(wxCommandEvent &event);
	void OnChangePasswordButton(wxCommandEvent &event);
	DECLARE_EVENT_TABLE();
};

RemarksConfigurationDialog::RemarksConfigurationDialog(wxWindow *parent, wxString &username, wxString &password, wxString &server, bool &auto_fetch) : 
		m_username(username),
		m_password(password), 
		m_server(server),
		m_auto_fetch(auto_fetch) {

	bool loaded = wxXmlResource::Get()->LoadDialog(this, parent, _T("RemarksSettings"));
	assert(loaded);

	password = wxEmptyString;

	XRCCTRL(*this, "UsernameTextCtrl", wxTextCtrl)->SetValue(m_username);
	XRCCTRL(*this, "ServerTextCtrl", wxTextCtrl)->SetValue(m_server);
	XRCCTRL(*this, "AutoFetchCheckbox", wxCheckBox)->SetValue(m_auto_fetch);

}

void RemarksConfigurationDialog::OnOK(wxCommandEvent &event) {

	m_username = XRCCTRL(*this, "UsernameTextCtrl", wxTextCtrl)->GetValue();
	m_server = XRCCTRL(*this, "ServerTextCtrl", wxTextCtrl)->GetValue();
	m_auto_fetch = XRCCTRL(*this, "AutoFetchCheckbox", wxCheckBox)->GetValue();

	EndModal(wxID_OK);

}

void RemarksConfigurationDialog::OnChangePasswordButton(wxCommandEvent &event) {
	wxString password = wxGetPasswordFromUser(
			_("Enter password:"),
			_("New password"),
			_T(""),
			this);

	if (password.IsEmpty())
		return;

	m_password = password;
}

BEGIN_EVENT_TABLE(RemarksConfigurationDialog, wxDialog)
	EVT_BUTTON(wxID_OK, RemarksConfigurationDialog::OnOK)
	EVT_BUTTON(XRCID("ChangePasswordButton"), RemarksConfigurationDialog::OnChangePasswordButton)
END_EVENT_TABLE()

void DrawFrame::OnConfigureRemarks(wxCommandEvent &event) {

	wxString username, password, server;
	bool autofetch;
	
	remarks_handler->GetConfiguration(username, password, server, autofetch);

	RemarksConfigurationDialog d(this, username, password, server, autofetch);

	if (d.ShowModal() == wxID_OK) {
		remarks_handler->SetConfiguration(username, password, server, autofetch);
		return;
	}
}

void DrawFrame::OnPrintPageSetup(wxCommandEvent& event) {
	Print::PageSetup(this);
}

void DrawFrame::OnGoToLatestDate(wxCommandEvent& event) {
	draw_panel->GoToLatestDate();	
}

void DrawFrame::OnShowRemarks(wxCommandEvent &e) {
	draw_panel->ShowRemarks();	
}

wxString DrawFrame::GetTitleForPanel(wxString title, int panel_no) {
	int nr = 1;
	wxString ret = title;

	bool got_title = false;
	while (!got_title) {
		int i = 0;
		for (; i < (int) m_notebook->GetPageCount(); i++)
			if (i != panel_no && ret == m_notebook->GetPageText(i))
				break;

		if (i < (int) m_notebook->GetPageCount())
			ret = wxString::Format(_T("%s <%d>"), title.c_str(), ++nr);
		else
			got_title = true;
	}

	return ret;
}

void DrawFrame::UpdatePanelName(DrawPanel *panel) {
	if (m_notebook == NULL)
		return;

	int i = m_notebook->GetPageIndex(panel);
	if (i == wxNOT_FOUND)
		return;

	m_notebook->SetPageText(i, GetTitleForPanel(panel->GetConfigName(), i));

}

void DrawFrame::OnProberAddresses(wxCommandEvent &event) {
	std::map<wxString, std::pair<wxString, wxString> > addresses = wxGetApp().GetProbersAddresses();

	ProbersAddressDialog dialog(this, database_manager, config_manager, addresses);
	if (dialog.ShowModal() != wxID_OK)
		return;

	std::map<wxString, std::pair<wxString, wxString> > naddresses = dialog.GetModifiedAddresses();
	for (std::map<wxString, std::pair<wxString, wxString> >::iterator i = naddresses.begin();
			i != naddresses.end();
			i++)
		addresses[i->first] = i->second;

	wxGetApp().SetProbersAddresses(addresses);

}

DrawPanel* DrawFrame::GetCurrentPanel() {
	return draw_panel;
}

void DrawFrame::OnSearchDate(wxCommandEvent &event) {
	draw_panel->SearchDate();
}

/**
 * IMPORTANT: Declare Events which are share by DrawMenuBar and DrawToolBar
 * especialy when we started use more than one bases
 */
BEGIN_EVENT_TABLE(DrawFrame, wxFrame)
    EVT_MENU(XRCID("Quit"), DrawFrame::OnExit)
    EVT_MENU(XRCID("About"), DrawFrame::OnAbout)
    EVT_MENU(XRCID("Help"), DrawFrame::OnHelp)
    EVT_MENU(XRCID("Find"), DrawFrame::OnFind)
    EVT_MENU(XRCID("SetParams"), DrawFrame::OnSetParams)
    EVT_MENU(XRCID("ClearCache"), DrawFrame::OnClearCache)
    EVT_MENU(XRCID("EditSet"), DrawFrame::OnEdit)
    EVT_MENU(XRCID("EditAsNew"), DrawFrame::OnEditSetAsNew)
    EVT_MENU(XRCID("ImportSet"), DrawFrame::OnImportSet)
    EVT_MENU(XRCID("ExportSet"), DrawFrame::OnExportSet)
    EVT_MENU(XRCID("DelSet"), DrawFrame::OnDel)
    EVT_MENU(XRCID("NewSet"), DrawFrame::OnAdd)
    EVT_MENU(XRCID("Save"), DrawFrame::OnSave)
    EVT_MENU(XRCID("NewWindow"), DrawFrame::OnLoadConfig)
    EVT_MENU(XRCID("NewTab"), DrawFrame::OnLoadConfig)
    EVT_MENU(XRCID("CloseTab"), DrawFrame::OnCloseTab)
    EVT_MENU(XRCID("Summary"), DrawFrame::OnSummaryWindowCheck)
    EVT_MENU(XRCID("Jump"), DrawFrame::OnJumpToDate)
    EVT_MENU(XRCID("Axes"), DrawFrame::OnNumberOfAxes)
    EVT_AUINOTEBOOK_PAGE_CHANGED(wxID_ANY, DrawFrame::OnNotebookSelectionChange)
    EVT_AUINOTEBOOK_PAGE_CLOSED(wxID_ANY, DrawFrame::OnNotebookPageClose)
    EVT_MENU(XRCID("Print"), DrawFrame::OnPrint)
    EVT_MENU(XRCID("PrintPrev"), DrawFrame::OnPrintPreview)
    EVT_MENU(XRCID("F0"), DrawFrame::OnFilterChange)
    EVT_MENU(XRCID("F1"), DrawFrame::OnFilterChange)
    EVT_MENU(XRCID("F2"), DrawFrame::OnFilterChange)
    EVT_MENU(XRCID("F3"), DrawFrame::OnFilterChange)
    EVT_MENU(XRCID("F4"), DrawFrame::OnFilterChange)
    EVT_MENU(XRCID("F5"), DrawFrame::OnFilterChange)
    EVT_MENU(XRCID("Pie"), DrawFrame::OnPieWin)
    EVT_MENU(XRCID("Ratio"), DrawFrame::OnRelWin)
    EVT_MENU(XRCID("XYGraph"), DrawFrame::OnXYDialog)
    EVT_MENU(XRCID("XYZGraph"), DrawFrame::OnXYZDialog)
    EVT_MENU(XRCID("ShowRemarks"), DrawFrame::OnRemarks)
    EVT_MENU(XRCID("StatsWin"), DrawFrame::OnStatDialog)
    EVT_MENU(XRCID("FullScreen"), DrawFrame::OnFullScreen)
    EVT_MENU(XRCID("SplitCursor"), DrawFrame::OnSplitCursor)
    EVT_MENU(XRCID("LATEST_DATA_FOLLOW"), DrawFrame::OnLatestDataFollow)
    EVT_MENU(XRCID("DECADE_RADIO"), DrawFrame::OnAverageChange)
    EVT_MENU(XRCID("YEAR_RADIO"), DrawFrame::OnAverageChange)
    EVT_MENU(XRCID("MONTH_RADIO"), DrawFrame::OnAverageChange)
    EVT_MENU(XRCID("WEEK_RADIO"), DrawFrame::OnAverageChange)
    EVT_MENU(XRCID("DAY_RADIO"), DrawFrame::OnAverageChange)
    EVT_MENU(XRCID("30MINUTE_RADIO"), DrawFrame::OnAverageChange)
    EVT_MENU(XRCID("SEASON_RADIO"), DrawFrame::OnAverageChange)
    EVT_MENU(XRCID("Copy"), DrawFrame::OnCopy)
    EVT_MENU(XRCID("Paste"), DrawFrame::OnPaste)
    EVT_MENU(XRCID("ImportDraw2"), DrawFrame::OnImportDraw2Def)
    EVT_MENU(XRCID("ReloadConfiguration"), DrawFrame::OnReloadConfig)
    EVT_MENU(XRCID("ShowErrorWindow"), DrawFrame::OnErrorFrame)
    EVT_MENU(XRCID("USER_PARAMS"), DrawFrame::OnUserParams)
    EVT_MENU(XRCID("Language"), DrawFrame::OnLanguageChange)
    EVT_MENU(XRCID("Graphs view"), DrawFrame::OnGraphsView)
    EVT_MENU(XRCID("Refresh"), DrawFrame::OnRefresh)
    EVT_MENU(XRCID("ContextHelp"), DrawFrame::OnContextHelp)
    EVT_MENU(XRCID("NumberOfPoints"), DrawFrame::OnNumberOfUnits)
    EVT_MENU(XRCID("AddRemark"), DrawFrame::OnAddRemark)
    EVT_MENU(XRCID("FetchRemarks"), DrawFrame::OnFetchRemarks)
    EVT_MENU(XRCID("RemarksConfiguration"), DrawFrame::OnConfigureRemarks)
    EVT_MENU(XRCID("PageSetup"), DrawFrame::OnPrintPageSetup)
    EVT_MENU(XRCID("ProberAddress"), DrawFrame::OnProberAddresses)
    EVT_MENU(drawTB_EXIT, DrawFrame::OnExit)
    EVT_MENU(drawTB_ABOUT, DrawFrame::OnAbout)
    EVT_MENU(drawTB_REMARK, DrawFrame::OnShowRemarks)
    EVT_MENU(drawTB_GOTOLATESTDATE, DrawFrame::OnGoToLatestDate)
    EVT_MENU(XRCID("GoToLatestDate"), DrawFrame::OnGoToLatestDate)
    EVT_MENU(XRCID("SearchDate"), DrawFrame::OnSearchDate)
    EVT_CLOSE(DrawFrame::OnClose)
    EVT_IDLE(DrawFrame::OnIdle)
    EVT_ACTIVATE(DrawFrame::OnActivate)
END_EVENT_TABLE()
