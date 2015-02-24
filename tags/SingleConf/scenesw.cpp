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


#include "scenesw.h"


extern "C" __declspec(dllexport) bool LoadPlugin();
extern "C" __declspec(dllexport) void UnloadPlugin();
extern "C" __declspec(dllexport) CTSTR GetPluginName();
extern "C" __declspec(dllexport) CTSTR GetPluginDescription();
extern "C" __declspec(dllexport) void ConfigPlugin(HWND);

HINSTANCE hinstMain = NULL;
SceneSwitcher* thePlugin = NULL;


INT_PTR CALLBACK ConfigDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
    {
        case WM_INITDIALOG:
		{
			HWND hwndAppList = GetDlgItem(hWnd, IDC_APPLIST);
			HWND hwndMainScn = GetDlgItem(hWnd, IDC_MAINSCN);
			HWND hwndAltScn  = GetDlgItem(hWnd, IDC_ALTSCN);
			HWND hwndCurrent = GetWindow(GetDesktopWindow(), GW_CHILD);

			do
			{
				if(IsWindowVisible(hwndCurrent))
				{
					DWORD exStyles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_EXSTYLE);
					DWORD styles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_STYLE);

					if( (exStyles & WS_EX_TOOLWINDOW) == 0 && (styles & WS_CHILD) == 0)
					{
						String strWindowName;

						strWindowName.SetLength(GetWindowTextLength(hwndCurrent));
						GetWindowText(hwndCurrent, strWindowName, strWindowName.Length()+1);
						int id = (int)SendMessage(hwndAppList, CB_ADDSTRING, 0, (LPARAM)strWindowName.Array());
						SendMessage(hwndAppList, CB_SETITEMDATA, id, (LPARAM)hwndCurrent);
					}
				}
			} while (hwndCurrent = GetNextWindow(hwndCurrent, GW_HWNDNEXT));

			XElement* scnList = OBSGetSceneListElement();
			if(scnList)
			{
				const DWORD numScn = scnList->NumElements();
				for(DWORD i=0; i<numScn; i++)
				{
					CTSTR sceneName = (scnList->GetElementByID(i))->GetName();
					SendMessage(hwndMainScn, CB_ADDSTRING, 0, (LPARAM)sceneName);
					SendMessage(hwndAltScn,  CB_ADDSTRING, 0, (LPARAM)sceneName);
				}
			}

			SendMessage(hwndMainScn, CB_SETCURSEL, SendMessage(hwndMainScn, CB_FINDSTRINGEXACT, -1, (LPARAM)thePlugin->GetmainSceneName()), 0);
			SendMessage(hwndAltScn,  CB_SETCURSEL, SendMessage(hwndAltScn,  CB_FINDSTRINGEXACT, -1, (LPARAM)thePlugin->GetaltSceneName()), 0);
			SendMessage(hwndAppList, CB_SETCURSEL, SendMessage(hwndAppList, CB_FINDSTRINGEXACT, -1, (LPARAM)thePlugin->GetmainWndName()), 0);

			SetDlgItemInt(hWnd, IDC_FREQ, thePlugin->GettimeToSleep(), FALSE);

			if(thePlugin->IsStartAuto())
				CheckDlgButton(hWnd, IDC_STARTAUTO, BST_CHECKED);

			if(thePlugin->IsRunning())
			{
				SetWindowText(GetDlgItem(hWnd, IDC_RUN), TEXT("running"));
				SetWindowText(GetDlgItem(hWnd, IDC_STOP), TEXT("Stop"));
			}

			return TRUE;
		}

		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case IDC_STOP:
					if(HIWORD(wParam) == BN_CLICKED)
					{
						if(thePlugin->IsRunning())
							thePlugin->StopThread(hWnd);
						else
						{
							thePlugin->ApplyConfig(hWnd);
							thePlugin->StartThread(hWnd);
						}
					}
					break;

				case IDC_SAVE:
					/*if(HIWORD(wParam) == BN_CLICKED)
						thePlugin->SaveApp(GetCBText(GetDlgItem(hWnd, IDC_APPLIST), CB_ERR),
										   GetCBText(GetDlgItem(hWnd, IDC_MAINSCN), CB_ERR),
										   GetCBText(GetDlgItem(hWnd, IDC_ALTSCN),  CB_ERR));*/
					break;

				case IDOK:
					thePlugin->ApplyConfig(hWnd);

				case IDCANCEL:
					EndDialog(hWnd, LOWORD(wParam));
			}
			break;

		case WM_CTLCOLORSTATIC:
			if(GetWindowLong((HWND)lParam, GWL_ID) == IDC_RUN)
			{
				HDC hdc = (HDC)wParam;
				SetTextColor(hdc, thePlugin->IsRunning() ? RGB(0,255,0) : RGB(255,0,0));
				SetBkColor(hdc, GetSysColor(COLOR_3DFACE));
				return (INT_PTR)GetSysColorBrush(COLOR_3DFACE);
			}
			break;

		case WM_CLOSE:
			EndDialog(hWnd, IDCANCEL);
	}
	return 0;
}

bool LoadPlugin()
{
    return (thePlugin = new SceneSwitcher) != 0;
}

void UnloadPlugin()
{
	if(thePlugin)
	{
		delete thePlugin;
		thePlugin = NULL;
	}
}

CTSTR GetPluginName()
{
	return TEXT("OBS Scene switcher");
}

CTSTR GetPluginDescription()
{
	return TEXT("Simple scene switcher, will switch to alternate scene when main window is not the foreground window.");
}

void ConfigPlugin(HWND hWnd)
{
	DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_CONFIG), OBSGetMainWindow(), ConfigDialogProc, (LPARAM)hWnd);
}

BOOL CALLBACK DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID lpBla)
{
	if(dwReason == DLL_PROCESS_ATTACH)
		hinstMain = hInst;

	return TRUE;
}
