/*
 * Copyright 2014-2015 Augustin Cavalier <waddlesplash>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "CentralWindow.h"

#include "ProjectFactory.h"
#include "EditorFactory.h"

#include <Application.h>
#include <MenuBar.h>
#include <ScrollView.h>
#include <TabView.h>
#include <LayoutBuilder.h>
#include <Roster.h>
#include <Path.h>

#include <private/interface/ColumnListView.h>
#include <private/interface/ColumnTypes.h>

#include "ToolBarIcons.h"
#include "ShellView.h"

#define B_TRANSLATION_CONTEXT "CentralWindow"
CentralWindow* central_window;


CentralWindow::CentralWindow(BRect frame)
	: BWindow(frame, "Heidi", B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL, 0L),
	  fOpenProject(NULL)
{
	central_window = this;

	// Create the user interface
	BMessenger thisMsngr(this, this);
	BMessage message(B_REFS_RECEIVED);
	fOpenPanel = new BFilePanel(B_OPEN_PANEL, &thisMsngr, NULL, B_FILE_NODE,
			true, &message);

	BMenuBar* menuBar = new BMenuBar("MenuBar");
	BLayoutBuilder::Menu<>(menuBar)
		.AddMenu(TR("File"))
			.AddItem(TR("Open" B_UTF8_ELLIPSIS), CW_OPEN, 'O')
			.AddSeparator()
			.AddItem(TR("Save" B_UTF8_ELLIPSIS), CW_SAVE, 'S')
			.AddItem(TR("Save as" B_UTF8_ELLIPSIS), CW_SAVEAS)
			.AddSeparator()
			.AddItem(TR("Quit"), B_QUIT_REQUESTED, 'Q')
		.End()
		.AddMenu(TR("Help"))
			.AddItem(TR("Website" B_UTF8_ELLIPSIS), CW_HOMEPAGE)
			.AddItem(TR("GitHub project" B_UTF8_ELLIPSIS), CW_GITHUB)
			.AddSeparator()
			.AddItem(TR("About" B_UTF8_ELLIPSIS), CW_ABOUT)
		.End()
	.End();

	fProjectTree = new BColumnListView("ProjectTree", 0);
	fProjectTree->AddColumn(new BStringColumn(TR("File"),
		175, 10, 10000, 0), 0);
	fEditorsTabView = new BTabView("EditorsTab");
	fOutputsTabView = new BTabView("OutputTab");

	fEditorsTabView->SetTabWidth(B_WIDTH_FROM_WIDEST);
	fOutputsTabView->SetTabWidth(B_WIDTH_FROM_WIDEST);

	fCompileOutput = new ShellView(TR("Compile Output"), thisMsngr, CW_BUILD_FINISHED);
	fBuildIssues = new BListView(TR("Build Issues"));
	fAppOutput = new ShellView(TR("App Output"), thisMsngr, CW_RUN_FINISHED);

	fOutputsTabView->AddTab(fCompileOutput->ScrollView());
	fOutputsTabView->AddTab(new BScrollView("Build Issues", fBuildIssues, B_NAVIGABLE, true, true));
	fOutputsTabView->AddTab(fAppOutput->ScrollView());

	init_tool_bar_icons();
	fToolbar = new BToolBar(B_VERTICAL);
	fToolbar->AddGlue();
	fToolbar->AddSeparator();
	fToolbar->AddAction(CW_BUILD, this, tool_bar_icon(kIconBuild));
	fToolbar->AddAction(CW_RUN, this, tool_bar_icon(kIconRun));
	fToolbar->AddAction(CW_RUN_DEBUG, this, tool_bar_icon(kIconRunDebug));
	CloseProject(); // Resets toolbar state

	fRootLayout = BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.SetInsets(0)
		.Add(menuBar)
		.AddGroup(B_HORIZONTAL, 0)
			.Add(fToolbar)
			.AddSplit(B_HORIZONTAL, B_USE_HALF_ITEM_SPACING) // Main splitter
				.AddSplit(B_VERTICAL, B_USE_HALF_ITEM_SPACING, 1) // Sidebar
					.Add(fProjectTree)
					//.Add() // Something will go here eventually...
				.End()
				.AddSplit(B_VERTICAL, B_USE_HALF_ITEM_SPACING, 3) // Code/output
					.Add(fEditorsTabView, 3)
					.Add(fOutputsTabView, 1)
				.End()
			.End()
		.End()
	.Layout();
}


CentralWindow::~CentralWindow()
{
}


Editor*
CentralWindow::CurrentEditor()
{
	BView* tab = fEditorsTabView->ViewForTab(fEditorsTabView->Selection());
	int32 length = fOpenEditors.CountItems();
	for (int i = 0; i < length; i++) {
		Editor* item = fOpenEditors.ItemAt(i);
		if (item->View() == tab)
			return item;
	}

	return NULL;
}


bool
CentralWindow::OpenProject(entry_ref* ref)
{
	Project* p = ProjectFactory::Load(ref);
	if (p == NULL)
		return false;
	CloseProject();

	fToolbar->SetActionEnabled(CW_BUILD, true);
	if (p->IsApp) {
		fToolbar->SetActionEnabled(CW_RUN, true);
		fToolbar->SetActionEnabled(CW_RUN_DEBUG, true);
	}
	fOpenProject = p;

	BString rootPath(p->DirectoryPath());
	for (int32 i = 0; i < p->Files.CountItems(); i++) {
		BRow* row = new BRow;
		BPath path;
		p->Files.ItemAt(i)->GetPath(&path);
		BString pth(path.Path());
		pth.ReplaceFirst(rootPath, "");
		row->SetField(new BStringField(pth.String()), 0);
		fProjectTree->AddRow(row);
	}

	return true;
}


void
CentralWindow::CloseProject()
{
	fToolbar->SetActionEnabled(CW_BUILD, false);
	fToolbar->SetActionEnabled(CW_RUN, false);
	fToolbar->SetActionEnabled(CW_RUN_DEBUG, false);

	fProjectTree->Clear();

	if (fOpenProject == NULL)
		return;

	// TODO: cancel builds
	delete fOpenProject;
}


void
CentralWindow::MessageReceived(BMessage* msg)
{
	switch (msg->what) {
	case CW_OPEN:
		fOpenPanel->Show();
	break;
	case CW_SAVE:
		CurrentEditor()->Save();
	break;

	case CW_BUILD:
		if (fOpenProject == NULL)
			break;
		fCompileOutput->Clear();
		fCompileOutput->Exec(fOpenProject->BuildCommand(),
					fOpenProject->DirectoryPath());
		fToolbar->SetActionEnabled(CW_BUILD, false);
	break;
	case CW_BUILD_FINISHED:
		fToolbar->SetActionEnabled(CW_BUILD, true);
	break;
	case CW_RUN:
		// TODO: change it to a stop button after it starts
		if (fOpenProject == NULL)
			break;
		fAppOutput->Clear();
		fAppOutput->Exec(fOpenProject->Name,
			fOpenProject->DirectoryPath().Append(DEFAULT_BUILD_DIR));
		fToolbar->SetActionEnabled(CW_RUN, false);
	break;
	case CW_RUN_FINISHED:
		fToolbar->SetActionEnabled(CW_RUN, true);
	break;
	case CW_RUN_DEBUG: {
		BString app = fOpenProject->DirectoryPath()
					.Append(DEFAULT_BUILD_DIR)
					.Append("/").Append(fOpenProject->Name);
		const char* launch = app.String();
		be_roster->Launch("application/x-vnd.Haiku-Debugger", 1,
				(char**) &launch);
	} break;

	case CW_HOMEPAGE: {
		const char* url = HOMEPAGE_URL;
		be_roster->Launch("text/html", 1, (char**) &url);
	} break;
	case CW_GITHUB: {
		const char* url = SOURCE_URL;
		be_roster->Launch("text/html", 1, (char**) &url);
	} break;
	case CW_ABOUT:
		be_app->PostMessage(B_ABOUT_REQUESTED);
	break;

	case B_REFS_RECEIVED:
	{
		entry_ref ref;
		int32 i = 0;
		if (msg->FindRef("refs", i, &ref) == B_OK) {
			if (!OpenProject(&ref)) {
				// Since that failed, try to load it as a file
				Editor* newEd = EditorFactory::Create(&ref);
				if (newEd != NULL) {
					int32 index = fEditorsTabView->CountTabs();
					fEditorsTabView->AddTab(newEd->View());
					fEditorsTabView->Select(index);
					newEd->SetTab(fEditorsTabView->TabAt(index));
					fOpenEditors.AddItem(newEd);
				}
			}
		}
	}
	break;

	case ShellView::SV_ADD_ERROR:
	{
		BString str;
		int idx = 0;
		while (msg->FindString("item", idx, &str) == B_OK)
		{
			BStringItem* item = new BStringItem(str);
			fBuildIssues->AddItem(item);
			idx++;
		}
	} break;
	default:
		BWindow::MessageReceived(msg);
	break;
	}
}


bool
CentralWindow::QuitRequested()
{
	// TODO: check for unsaved stuff
	// TODO: save layout sizes/positions

	if (BWindow::QuitRequested()) {
		be_app->PostMessage(B_QUIT_REQUESTED);
		return true;
	}
	return false;
}
