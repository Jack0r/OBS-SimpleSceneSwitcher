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

// Hotkey stuff
UINT hotkeyID;
DWORD hotkey;

void STDCALL HotkeyToggle(DWORD hotkey, UPARAM param, bool bDown)
{
	if (!bDown) {
		if (thePlugin->IsRunning()) {
			thePlugin->StopThread();
		} else {
			thePlugin->StartThread();
		}
	}
}

void ApplyConfig(HWND hWnd) {
	thePlugin->ApplyConfig(hWnd);

	// Redefine the hotkey
	if (hotkeyID)
		OBSDeleteHotkey(hotkeyID);

	if (thePlugin->GetToggleHotkey() != 0)
		hotkeyID = OBSCreateHotkey((DWORD)thePlugin->GetToggleHotkey(), HotkeyToggle, 0);
}

INT_PTR CALLBACK ConfigDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_INITDIALOG: // When loading the dialog
		{
			// Get the listbox control handles
			HWND hwndAppList = GetDlgItem(hWnd, IDC_APPLIST);
			HWND hwndMainScn = GetDlgItem(hWnd, IDC_MAINSCN);
			HWND hwndAltScn  = GetDlgItem(hWnd, IDC_ALTSCN);
			HWND hwndWSMap = GetDlgItem(hWnd, IDC_WSMAP);
			HWND hwndSwButton = GetDlgItem(hWnd, IDC_ALTSWITCH);
			HWND hwndNoswButton = GetDlgItem(hWnd, IDC_ALTNOSWITCH);
			HWND hwndCurrent = GetWindow(GetDesktopWindow(), GW_CHILD); // The top child of the desktop

			// let's fill the listcontrol
			SendMessage(hwndWSMap, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
			LVCOLUMN col1;
			col1.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
			col1.fmt = LVCFMT_RIGHT | LVCFMT_FIXED_WIDTH;
			col1.cx = 422;
			col1.pszText = TEXT("Window Title");
			LVCOLUMN col2;
			col2.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
			col2.fmt = LVCFMT_LEFT | LVCFMT_FIXED_WIDTH;
			col2.cx = 422;
			col2.pszText = TEXT("Scene");
			SendMessage(hwndWSMap, LVM_INSERTCOLUMN, 0, (LPARAM) (LPLVCOLUMN) &col1);
			SendMessage(hwndWSMap, LVM_INSERTCOLUMN, 1, (LPARAM) (LPLVCOLUMN) &col2);
			// Add the items
			for (int i = 0; i < thePlugin->GetnWindowsDefined(); i++)
			{
				String window = thePlugin->GetWindow(i); 
				String scene = thePlugin->GetScene(i);
				LVITEM item;
				item.iItem = 0;
				item.iSubItem = 0;
				item.mask = LVIF_TEXT;
				item.pszText = window;

				LVITEM subItem;
				subItem.iItem = 0;
				subItem.iSubItem = 1;
				subItem.mask = LVIF_TEXT;
				subItem.pszText = scene;

				SendMessage(hwndWSMap, LVM_INSERTITEM, 0, (LPARAM) &item);
				SendMessage(hwndWSMap, LVM_SETITEM, 0, (LPARAM) &subItem);
			}
			// Set the radio buttons and alt scene combo state
			SendMessage(hwndSwButton, BM_SETCHECK, (thePlugin->IsAltDoSwitch() ? BST_CHECKED : BST_UNCHECKED), 0);
			SendMessage(hwndNoswButton, BM_SETCHECK, (thePlugin->IsAltDoSwitch() ? BST_UNCHECKED : BST_CHECKED), 0);
			EnableWindow(hwndAltScn, thePlugin->IsAltDoSwitch());
			
			if(thePlugin->IsMatchExact()) // Match exact window name checkbox
				CheckDlgButton(hWnd, IDC_EXACT, BST_CHECKED);

			do
			{
				if(IsWindowVisible(hwndCurrent))
				{
					// Get the styles for the window
					DWORD exStyles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_EXSTYLE);
					DWORD styles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_STYLE);

					if( (exStyles & WS_EX_TOOLWINDOW) == 0 && (styles & WS_CHILD) == 0)
					{
						// The window is not a toolwindow, and not a child window
						String strWindowName;

						// Get the name of the window
						strWindowName.SetLength(GetWindowTextLength(hwndCurrent));
						GetWindowText(hwndCurrent, strWindowName, strWindowName.Length()+1);
						// Add the Name of the window to the appList
						const int id = (int)SendMessage(hwndAppList, CB_ADDSTRING, 0, (LPARAM)strWindowName.Array());
						// Set the data for the added item to be the window handle
						SendMessage(hwndAppList, CB_SETITEMDATA, id, (LPARAM)hwndCurrent);
					}
				}
			} // Move down the windows in z-order
			while (hwndCurrent = GetNextWindow(hwndCurrent, GW_HWNDNEXT));

			// Get the list of scenes from OBS
			XElement* scnList = OBSGetSceneListElement();
			if(scnList)
			{
				const DWORD numScn = scnList->NumElements();
				for(DWORD i=0; i<numScn; i++)
				{
					CTSTR sceneName = (scnList->GetElementByID(i))->GetName();
					// Add the scene name to both scene lists
					SendMessage(hwndMainScn, CB_ADDSTRING, 0, (LPARAM)sceneName);
					SendMessage(hwndAltScn,  CB_ADDSTRING, 0, (LPARAM)sceneName);
				}
			}

			// Set the selected list items to be the ones from settings
			SendMessage(hwndMainScn, CB_SETCURSEL, SendMessage(hwndMainScn, CB_FINDSTRINGEXACT, -1, (LPARAM)thePlugin->GetmainSceneName()), 0);
			SendMessage(hwndAltScn,  CB_SETCURSEL, SendMessage(hwndAltScn,  CB_FINDSTRINGEXACT, -1, (LPARAM)thePlugin->GetaltSceneName()), 0);
			SendMessage(hwndAppList, CB_SETCURSEL, SendMessage(hwndAppList, CB_FINDSTRINGEXACT, -1, (LPARAM)thePlugin->GetmainWndName()), 0);

			// Set the frequency from the settings
			SetDlgItemInt(hWnd, IDC_FREQ, thePlugin->GettimeToSleep(), FALSE);

			// Set the autostart checkbox value from the settings
			if(thePlugin->IsStartAuto())
				CheckDlgButton(hWnd, IDC_STARTAUTO, BST_CHECKED);

			// Set the toggle hotkey control
			SendMessage(GetDlgItem(hWnd, IDC_TOGGLEHOTKEY), HKM_SETHOTKEY, thePlugin->GetToggleHotkey(), 0);

			// If the plugin is running, update the text values
			if(thePlugin->IsRunning())
			{
				SetWindowText(GetDlgItem(hWnd, IDC_RUN), TEXT("running"));
				SetWindowText(GetDlgItem(hWnd, IDC_STOP), TEXT("Stop"));
			}

			return TRUE;
		}

	case WM_COMMAND: // When a command is received to the dialog
		switch(LOWORD(wParam))
		{
		case IDC_CLEAR_HOTKEY:
			if(HIWORD(wParam) == BN_CLICKED) {
				SendMessage(GetDlgItem(hWnd, IDC_TOGGLEHOTKEY), HKM_SETHOTKEY, 0, 0);
			}
			break;

		case IDC_STOP:
			if(HIWORD(wParam) == BN_CLICKED) // Stop button clicked
			{
				if(thePlugin->IsRunning())
					thePlugin->StopThread(hWnd);
				else
				{
					ApplyConfig(hWnd);
					thePlugin->StartThread(hWnd);
				}
			}
			break;

		case IDOK: // Ok button
			ApplyConfig(hWnd);

		case IDCANCEL: // Cancel button
			EndDialog(hWnd, LOWORD(wParam));
			break;

		case IDUP:
			{
				HWND wsMap = GetDlgItem(hWnd, IDC_WSMAP);
				const int sel = SendMessage(wsMap, LVM_GETSELECTIONMARK, 0, 0);
				if (sel > 0)
				{
					// Get the text from the item
					String wnd;
					wnd.SetLength(256);
					ListView_GetItemText(wsMap, sel, 0, wnd, 256);
					String scn;
					scn.SetLength(256);
					ListView_GetItemText(wsMap, sel, 1, scn, 256);

					// Delete it
					SendMessage(wsMap, LVM_DELETEITEM, sel, 0);

					// Add it above
					LVITEM lv1;
					lv1.mask = LVIF_TEXT;
					lv1.iItem = sel - 1;
					lv1.iSubItem = 0;
					lv1.pszText = wnd;
					LVITEM lv2;
					lv2.mask = LVIF_TEXT;
					lv2.iItem = sel - 1;
					lv2.iSubItem = 1;
					lv2.pszText = scn;
					SendMessage(wsMap, LVM_INSERTITEM, sel - 1, (LPARAM) &lv1);
					SendMessage(wsMap, LVM_SETITEM, sel - 1, (LPARAM) &lv2);

					// Update the selection mark
					SendMessage(wsMap, LVM_SETSELECTIONMARK, 0, sel - 1);
				}
			}
			break;

		case IDDOWN:
			{
				HWND wsMap = GetDlgItem(hWnd, IDC_WSMAP);
				const int sel = SendMessage(wsMap, LVM_GETSELECTIONMARK, 0, 0);
				const int max = SendMessage(wsMap, LVM_GETITEMCOUNT, 0, 0) - 1;
				if (sel > -1 && sel < max)
				{
					// Get the text from the item
					String wnd;
					wnd.SetLength(256);
					ListView_GetItemText(wsMap, sel, 0, wnd, 256);
					String scn;
					scn.SetLength(256);
					ListView_GetItemText(wsMap, sel, 1, scn, 256);

					// Delete it
					SendMessage(wsMap, LVM_DELETEITEM, sel, 0);

					// Add it below
					LVITEM lv1;
					lv1.mask = LVIF_TEXT;
					lv1.iItem = sel + 1;
					lv1.iSubItem = 0;
					lv1.pszText = wnd;
					LVITEM lv2;
					lv2.mask = LVIF_TEXT;
					lv2.iItem = sel + 1;
					lv2.iSubItem = 1;
					lv2.pszText = scn;
					SendMessage(wsMap, LVM_INSERTITEM, sel + 1, (LPARAM) &lv1);
					SendMessage(wsMap, LVM_SETITEM, sel + 1, (LPARAM) &lv2);

					// Update the selection mark
					SendMessage(wsMap, LVM_SETSELECTIONMARK, 0, sel + 1);
				}
			}
			break;
		case IDADD:
			{
				HWND wsMap = GetDlgItem(hWnd, IDC_WSMAP);
				HWND appList = GetDlgItem(hWnd, IDC_APPLIST);
				HWND scnList = GetDlgItem(hWnd, IDC_MAINSCN);
				
				String wnd = GetEditText(appList);
				// First column
				LVITEM lv1;
				lv1.mask = LVIF_TEXT;
				lv1.iItem = 0;
				lv1.iSubItem = 0;
				lv1.pszText = wnd;
				// Second column
				String scn = GetCBText(scnList, CB_ERR);
				LVITEM lv2;
				lv2.mask = LVIF_TEXT;
				lv2.iItem = 0;
				lv2.iSubItem = 1;
				lv2.pszText = scn;
				// Add first column then set second
				SendMessage(wsMap, LVM_INSERTITEM, 0, (LPARAM)&lv1);
				SendMessage(wsMap, LVM_SETITEM, 0, (LPARAM)&lv2);
			}
			break;

		case IDREM:
			{
				// Remove the item
				HWND wsMap = GetDlgItem(hWnd, IDC_WSMAP);
				const int sel = SendMessage(wsMap, LVM_GETSELECTIONMARK, 0, 0);
				if (sel > -1)
					SendMessage(wsMap, LVM_DELETEITEM, sel, 0);
			}
			break;

		case IDEDIT:
			{
				HWND wsMap = GetDlgItem(hWnd, IDC_WSMAP);
				HWND appList = GetDlgItem(hWnd, IDC_APPLIST);
				HWND scnList = GetDlgItem(hWnd, IDC_MAINSCN);
				
				const int sel = SendMessage(wsMap, LVM_GETSELECTIONMARK, 0, 0);
				if (sel < 0) break;

				String wnd = GetEditText(appList);
				// First column
				LVITEM lv1;
				lv1.mask = LVIF_TEXT;
				lv1.iItem = sel;
				lv1.iSubItem = 0;
				lv1.pszText = wnd;
				String scn = GetCBText(scnList, CB_ERR);
				// Second column
				LVITEM lv2;
				lv2.mask = LVIF_TEXT;
				lv2.iItem = sel;
				lv2.iSubItem = 1;
				lv2.pszText = scn;

				// Set the text
				SendMessage(wsMap, LVM_SETITEM, 0, (LPARAM)&lv1);
				SendMessage(wsMap, LVM_SETITEM, 0, (LPARAM)&lv2);
			}
			break;
		case IDC_ALTSWITCH:
		case IDC_ALTNOSWITCH:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				HWND swButton = GetDlgItem(hWnd, IDC_ALTSWITCH);
				HWND altCombo = GetDlgItem(hWnd, IDC_ALTSCN);
				const bool swChecked = (SendMessage(swButton, BM_GETSTATE, 0, 0) & BST_CHECKED) != 0;
				EnableWindow(altCombo, swChecked);
			}
			break;
		}

	case WM_CTLCOLORSTATIC: // Set the correct color for drawing the running status
		if(GetWindowLong((HWND)lParam, GWL_ID) == IDC_RUN)
		{
			HDC hdc = (HDC)wParam;
			SetTextColor(hdc, thePlugin->IsRunning() ? RGB(0,255,0) : RGB(255,0,0));
			SetBkColor(hdc, GetSysColor(COLOR_3DFACE));
			return (INT_PTR)GetSysColorBrush(COLOR_3DFACE);
		}
		break;

	case WM_NOTIFY:
		{
			const NMITEMACTIVATE* lpnmitem = (LPNMITEMACTIVATE)lParam;

			if(lpnmitem->hdr.idFrom == IDC_WSMAP && lpnmitem->hdr.code == NM_CLICK)
			{
				const int sel = lpnmitem->iItem;
				if(sel >= 0)
				{
					HWND wsMap = GetDlgItem(hWnd, IDC_WSMAP);
					HWND hwndAppList = GetDlgItem(hWnd, IDC_APPLIST);
					HWND hwndMainScn = GetDlgItem(hWnd, IDC_MAINSCN);

					// Get the text from the item
					String wnd;
					wnd.SetLength(256);
					ListView_GetItemText(wsMap, sel, 0, wnd, 256);
					String scn;
					scn.SetLength(256);
					ListView_GetItemText(wsMap, sel, 1, scn, 256);

					// Set the combos
					SetWindowText(hwndAppList, wnd);
					SendMessage(hwndMainScn, CB_SETCURSEL, SendMessage(hwndMainScn, CB_FINDSTRINGEXACT, -1, (LPARAM)scn.Array()), 0);
				}

			}
			break;
		}

	case WM_CLOSE:
		EndDialog(hWnd, IDCANCEL);
	}
	return 0;
}

bool LoadPlugin()
{
	InitHotkeyExControl(hinstMain);
	thePlugin = new SceneSwitcher;
	if (thePlugin == 0) return false;

	if (thePlugin->GetToggleHotkey() != 0)
		hotkeyID = OBSCreateHotkey(thePlugin->GetToggleHotkey(), HotkeyToggle, 0);

	return true;
}

void UnloadPlugin()
{
	if (hotkeyID)
		OBSDeleteHotkey(hotkeyID);

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
	return TEXT("Simple scene switcher. It will switch between several scenes according to your active window.");
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
