/********************************************************************************
 Copyright (C) 2013 Christophe Jeannin <chris.j84@free.fr>

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

#pragma once


#define CONFIGFILENAME TEXT("\\scenesw.ini")
#define TTS_DEFAULT 300
#define TTS_MIN 50
#define TTS_MAX 5000


class SceneSwitcher
{
	HWND mainHwnd;
	HANDLE thread;
	HANDLE timer;
	HANDLE stopReq;
	DWORD timeToSleep;
	String mainSceneName;
	String altSceneName;
	String mainWndName;
	int startAuto;
	bool bKillThread;
	ConfigFile config;

	inline bool CheckSettings(){return (!mainSceneName.IsEmpty() && !altSceneName.IsEmpty() && !mainWndName.IsEmpty() && (mainSceneName != altSceneName) && mainHwnd);}
	void ReadSettings();
	void WriteSettings();

	DWORD Run();
	static DWORD WINAPI ThreadProc(LPVOID pParam){return static_cast<SceneSwitcher*>(pParam)->Run();}

public:
	inline CTSTR GetmainSceneName(){return mainSceneName;}
	inline CTSTR GetaltSceneName() {return altSceneName;}
	inline CTSTR GetmainWndName()  {return mainWndName;}
	inline DWORD GettimeToSleep()  {return timeToSleep;}
	inline bool IsRunning()		   {return thread!=0;}
	inline bool IsStartAuto()	   {return startAuto!=0;}

	SceneSwitcher();
	~SceneSwitcher();

	void StartThread(HWND hDialog = NULL);
	void StopThread (HWND hDialog = NULL);
	void ApplyConfig(HWND hDialog);
	void SaveApp(String strName, String strMain, String strAlt);
};
