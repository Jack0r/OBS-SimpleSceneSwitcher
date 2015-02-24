/********************************************************************************
 Copyright (C) 2013 Christophe Jeannin <chris.j84@free.fr>
                    Hugh Bailey <obs.jim@gmail.com>

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
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/

#include "scenesw.h"


SceneSwitcher::SceneSwitcher()
{
	mainHwnd = NULL;
	thread = 0;
	timer = 0;
	bKillThread = true;

	String configFile;

	configFile << OBSGetPluginDataPath() << CONFIGFILENAME;

	config.Open(configFile, true);
	ReadSettings();

	stopReq = CreateEvent(NULL, FALSE, FALSE, NULL);

	if(startAuto)
	{
		mainHwnd = FindWindow(NULL, mainWndName);
		StartThread();
	}
}


SceneSwitcher::~SceneSwitcher()
{
	StopThread();
	config.Close();
	CloseHandle(stopReq);
}


void SceneSwitcher::ReadSettings()
{
	mainWndName   = config.GetString(TEXT("General"), TEXT("MainWindow"));
	mainSceneName = config.GetString(TEXT("General"), TEXT("MainScene"));
	altSceneName  = config.GetString(TEXT("General"), TEXT("AltScene"));
	startAuto     = config.GetInt(TEXT("General"), TEXT("StartAuto"));
	timeToSleep   = (DWORD)config.GetInt(TEXT("General"), TEXT("CheckFrequency"));

	if(!timeToSleep)
		timeToSleep = TTS_DEFAULT;
	else
		if(timeToSleep < TTS_MIN) timeToSleep = TTS_MIN;
		else if(timeToSleep > TTS_MAX) timeToSleep = TTS_MAX;
}


void SceneSwitcher::WriteSettings()
{
	config.SetString(TEXT("General"), TEXT("MainWindow"), mainWndName);
	config.SetString(TEXT("General"), TEXT("MainScene"), mainSceneName);
	config.SetString(TEXT("General"), TEXT("AltScene"), altSceneName);
	config.SetInt(TEXT("General"), TEXT("StartAuto"), startAuto);
	config.SetInt(TEXT("General"), TEXT("CheckFrequency"), timeToSleep);
}


DWORD SceneSwitcher::Run()
{
	LARGE_INTEGER dueTime;

	if((timer = CreateWaitableTimer(NULL, FALSE, NULL)) == NULL)
		return -1;
	dueTime.QuadPart=0;
	SetWaitableTimer(timer, &dueTime, timeToSleep, NULL, NULL, FALSE);
	HANDLE handles[] = { timer, stopReq };

	while(!bKillThread)
	{
		HWND hwndCurrent = GetForegroundWindow();
		BOOL isMainScn;
		BOOL isMainWnd;

		isMainWnd = (hwndCurrent == mainHwnd);
		isMainScn = (mainSceneName == OBSGetSceneName());

		if(isMainScn && !isMainWnd)
			OBSSetScene(altSceneName, true);
		else if(!isMainScn && isMainWnd)
			OBSSetScene(mainSceneName, true);

		WaitForMultipleObjects(2, handles, FALSE, INFINITE);
	}

	CloseHandle(timer);
	return 0;
}


void SceneSwitcher::StartThread(HWND hDialog)
{
	if(!thread && CheckSettings())
	{
		bKillThread = false;
		thread = CreateThread(NULL, 0, ThreadProc, (LPVOID)this, 0, NULL);

		if(hDialog && thread)
		{
			SetWindowText(GetDlgItem(hDialog, IDC_RUN), TEXT("running"));
			SetWindowText(GetDlgItem(hDialog, IDC_STOP), TEXT("Stop"));
		}
	}
}


void SceneSwitcher::StopThread(HWND hDialog)
{
	if(thread)
	{
		bKillThread = true;

		DWORD retv;
		MSG msg;
		HANDLE handles[] = {thread};
		SetEvent(stopReq);
		//process messages while waiting for the thread to complete, otherwise the thread will be locked forever
		while((retv = MsgWaitForMultipleObjects(1, handles, FALSE, INFINITE, QS_ALLINPUT)) != WAIT_FAILED)
		{
			if(retv == WAIT_OBJECT_0)
				break;
			else if(retv == WAIT_OBJECT_0+1)
				while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
		}
		ResetEvent(stopReq);

		if(thread)
		{
			CloseHandle(thread);
			thread = 0;
		}
	}

	if(hDialog)
	{
		SetWindowText(GetDlgItem(hDialog, IDC_RUN), TEXT("not running"));
		SetWindowText(GetDlgItem(hDialog, IDC_STOP), TEXT("Start"));
	}
}


void SceneSwitcher::ApplyConfig(HWND hDialog)
{
	if(!hDialog)
		return;

	HWND hwndMainScn = GetDlgItem(hDialog, IDC_MAINSCN);
	HWND hwndAltScn  = GetDlgItem(hDialog, IDC_ALTSCN);
	HWND hwndAppList = GetDlgItem(hDialog, IDC_APPLIST);

	String newMainSceneName = GetCBText(hwndMainScn, CB_ERR);
	String newAltSceneName  = GetCBText(hwndAltScn,  CB_ERR);
	String newMainWndName   = GetCBText(hwndAppList, CB_ERR);
	HWND newMainHwnd = (HWND)SendMessage(hwndAppList, CB_GETITEMDATA, SendMessage(hwndAppList, CB_GETCURSEL, 0, 0), 0);

	DWORD newTimeToSleep = GetDlgItemInt(hDialog, IDC_FREQ, NULL, FALSE);
	if(newTimeToSleep < TTS_MIN) newTimeToSleep = TTS_MIN;
	else if(newTimeToSleep > TTS_MAX) newTimeToSleep = TTS_MAX;

	if(newMainSceneName.IsEmpty() || newAltSceneName.IsEmpty() || newMainWndName.IsEmpty() || (newMainSceneName == newAltSceneName)  || !newMainHwnd)
		StopThread(hDialog);	// if we're applying a bad config stop thread first

	if((timeToSleep != newTimeToSleep) && thread && timer)
	{
		LARGE_INTEGER dueTime;
		dueTime.QuadPart=0;
		SetWaitableTimer(timer, &dueTime, newTimeToSleep, NULL, NULL, FALSE);
	}

	timeToSleep   = newTimeToSleep;
	mainSceneName = newMainSceneName;
	altSceneName  = newAltSceneName;
	mainWndName   = newMainWndName;
	mainHwnd      = newMainHwnd;

	if(IsDlgButtonChecked(hDialog, IDC_STARTAUTO) == BST_CHECKED)
		startAuto = 1;
	else
		startAuto = 0;

	WriteSettings();
}


void SceneSwitcher::SaveApp(String strName, String strMain, String strAlt)
{
	static CTSTR txtMain = TEXT("Main");
	static CTSTR txtAlt  = TEXT("Alt");
	static CTSTR txtApps = TEXT("Apps");

	if(strMain == strAlt)
		return;

	config.SetString(txtApps, strName+txtMain, strMain);
	config.SetString(txtApps, strName+txtAlt,  strAlt);
}
