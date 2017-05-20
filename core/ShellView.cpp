/*
 * Copyright 2014 Augustin Cavalier <waddlesplash>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "ShellView.h"

#include <Looper.h>

#include <stdio.h>

// #pragma mark - ExecThread

#define BUF_SIZE 512

class ExecThread : public BLooper {
public:
					ExecThread(BString command, BString dir, BMessenger msnger);
		void		MessageReceived(BMessage* msg);

private:
		BMessenger	fMessenger;
		BString		fCommand;
};


ExecThread::ExecThread(BString command, BString dir, BMessenger msnger)
	: BLooper(command.String())
{
	fCommand.SetToFormat("cd \"%s\" && %s ;", dir.String(), command.String());
	fMessenger = msnger;
}


void
ExecThread::MessageReceived(BMessage* msg)
{
	switch (msg->what) {
	case ShellView::SV_LAUNCH: {
		FILE* fd;
		FILE* fd2;
		int result = gb_popen(fCommand.String(), fd, fd2);
		bool fdopen = false;
		bool fd2open = false;
		if (fd != NULL)
		{
			fdopen = true;
		}
		if (fd2 != NULL)
		{
			fd2open = true;
		}


		char buffer[BUF_SIZE];
		char buffer2[BUF_SIZE];
		BString strerr = "";
		while (fdopen || fd2open) {
			if (fdopen && fgets(buffer, BUF_SIZE, fd)) {
				BMessage* newMsg = new BMessage(ShellView::SV_DATA);
				newMsg->RemoveName("data");
				newMsg->AddString("data", BString(buffer, BUF_SIZE).String());
				fMessenger.SendMessage(newMsg);
			} else {
				fdopen = false;
			}
			if (fd2open && fgets(buffer2, BUF_SIZE, fd2)) {
				strerr << BString(buffer2, MIN(strlen(buffer2), BUF_SIZE));
			} else {
				fd2open = false;
			}
		}
		if (strerr != "")
		{
			BMessage* newMsg = new BMessage(ShellView::SV_ERROR);
			newMsg->AddString("data", strerr);
			fMessenger.SendMessage(newMsg);
		}
		gb_pclose(fd);
		gb_pclose(fd2);
		// TODO: error handling
		fMessenger.SendMessage(ShellView::SV_DONE);
	} break;

	default:
		BLooper::MessageReceived(msg);
	break;
	}
}

// #pragma mark - ShellView

ShellView::ShellView(const char* name, BMessenger msngr, uint32 what)
	: BTextView(name, be_fixed_font, NULL, B_WILL_DRAW | B_PULSE_NEEDED),
	  fExecThread(NULL),
	  fExecDir("."),
	  fCommand(""),
	  fMessenger(msngr),
	  fNotifyFinishedWhat(what)
{
	fScrollView = new BScrollView(name, this, B_NAVIGABLE, true, true);
	MakeEditable(false);
	SetWordWrap(false);
}


void
ShellView::MessageReceived(BMessage* msg)
{
	switch (msg->what) {
	case SV_DATA: {
		// Check if the vertical scroll bar is at the end
		float max;
		fScrollView->ScrollBar(B_VERTICAL)->GetRange(NULL, &max);
		bool atEnd = fScrollView->ScrollBar(B_VERTICAL)->Value() == max;

		BString str;
		msg->FindString("data", &str);
		BTextView::Insert(str.String());

		if (atEnd) {
			fScrollView->ScrollBar(B_VERTICAL)->GetRange(NULL, &max);
			fScrollView->ScrollBar(B_VERTICAL)->SetValue(max);
		}
	} break;

	case SV_ERROR: {
		BString str;
		msg->FindString("data", &str);
		BStringList strlist;
		if (!str.Split("\n", true, strlist))
			break;
		BMessage* newmsg = new BMessage(SV_ADD_ERROR);
		for (int a = 0; a < strlist.CountStrings(); a++)
		{
			newmsg->AddString("item", strlist.StringAt(a));
		}
		fMessenger.SendMessage(newmsg);
	} break;

	case SV_DONE:
		if (fExecThread->LockLooper()) {
			fExecThread->Quit();
			fExecThread = NULL;
		}
		fMessenger.SendMessage(fNotifyFinishedWhat);
	break;

	default:
		BTextView::MessageReceived(msg);
	break;
	}
}


status_t
ShellView::SetCommand(BString command)
{
	if (fExecThread != NULL)
		return B_ERROR;

	fCommand = command;
	return B_OK;
}


status_t
ShellView::SetExecDir(BString execDir)
{
	if (fExecThread != NULL)
		return B_ERROR;

	fExecDir = execDir;
	return B_OK;
}


status_t
ShellView::Exec()
{
	if (fExecThread != NULL)
		return B_ERROR;

	fExecThread = new ExecThread(fCommand, fExecDir,
				BMessenger(this, (BLooper*)Window()));
	fExecThread->Run();
	fExecThread->PostMessage(SV_LAUNCH);
	return B_OK;
}


status_t
ShellView::Exec(BString command, BString execDir)
{
	SetCommand(command);
	SetExecDir(execDir);
	return Exec();
}


void
ShellView::Clear()
{
	BTextView::SetText("");
}
