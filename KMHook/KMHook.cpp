// KMHook.cpp : DLL 응용 프로그램을 위해 내보낸 함수를 정의합니다.
//

#include "stdafx.h"
#include <Windows.h>

#define		TASKBAR			"Shell_TrayWnd"

HINSTANCE g_hInstance;		// Instance Handle
CRITICAL_SECTION cs;

HHOOK	g_hKeyboardHook;	// KeyBoard Hook Handle
HHOOK	g_hMouseHook;		// Mouse Hook Handle

HWND g_hWnd = NULL;		// Main hwnd. We will get this from the App


/* _______________________________________________________________________________

Get Window Handle
_______________________________________________________________________________  */
extern "C" __declspec(dllexport)
void SetWindowHandleToDll(HWND hWnd)
{
	g_hWnd = hWnd;
}


/*************************
* Hide/Show Taskbar.    *
* TRUE=Show, FALSE=Hide *
* (Win9x/NT/2K/XP)      *
*************************/
extern "C" __declspec(dllexport)
int Taskbar_Show_Hide(BOOL bShowHide)
{
	
	HWND    hWnd;

	hWnd = FindWindow(TASKBAR, NULL);
	if (hWnd == NULL)
		return 0;
	
	if (bShowHide)
		SetWindowPos(hWnd, 0, 0, 0, 0, 0, SWP_SHOWWINDOW);
	else
		SetWindowPos(hWnd, 0, 0, 0, 0, 0, SWP_HIDEWINDOW);

	//ShowWindow(hWnd, bShowHide ? SW_SHOW : SW_HIDE);
	//UpdateWindow(hWnd);

	return 1;
}


/****************************************
* Disable Task Manager (CTRL+ALT+DEL). *
* TRUE=Enable, FALSE=Disable           *
* (Win NT/2K/XP)                       *
****************************************/
extern "C" __declspec(dllexport)
int TaskManager_Enable_Disable(BOOL bEnableDisable)
{
#define KEY_DISABLETASKMGR  "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System"
#define VAL_DISABLETASKMGR  "DisableTaskMgr"

	HKEY    hKey;
	DWORD   val;
	LONG	r;

	if (RegOpenKey(HKEY_CURRENT_USER, KEY_DISABLETASKMGR, &hKey) != ERROR_SUCCESS)
	if (RegCreateKey(HKEY_CURRENT_USER, KEY_DISABLETASKMGR, &hKey) != ERROR_SUCCESS)
		return 0;

	if (bEnableDisable) // Enable
	{
		r = RegDeleteValue(hKey, VAL_DISABLETASKMGR);
	}
	else                // Disable
	{
		val = 1;
		r = RegSetValueEx(hKey, VAL_DISABLETASKMGR, 0, REG_DWORD, (BYTE *)&val, sizeof(val));
	}

	RegCloseKey(hKey);

	return (r == ERROR_SUCCESS ? 1 : 0);
}


/* _______________________________________________________________________________

Hook Procedure
_______________________________________________________________________________  */

// Keyboard Hook Procedure
extern "C" __declspec(dllexport)
LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	KBDLLHOOKSTRUCT *pKey = (KBDLLHOOKSTRUCT *)lParam;
	
	if (nCode < 0)
		return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);

	if (pKey->vkCode == 32)
		return FALSE;

	return TRUE;
}


// Mouse Hook Procedure
extern "C" __declspec(dllexport)
LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam){
	POINT pt = { 0 };
	GetCursorPos(&pt);
	PMSLLHOOKSTRUCT ms;
	ms = (PMSLLHOOKSTRUCT)lParam;

	int nWidth = GetSystemMetrics(SM_CXSCREEN);
	int nHeight = GetSystemMetrics(SM_CYSCREEN);

	HWND hwnd = FindWindow(NULL, TEXT("ControlWindows"));

	if (nCode < 0)
		return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);

	if (wParam == WM_LBUTTONDOWN){ // 왼쪽 버튼 DOWN
		return FALSE;
	}
	else if (wParam == WM_LBUTTONUP){ // 왼쪽 버튼 UP
		return FALSE;
	}

	return TRUE;
}


/* _______________________________________________________________________________

Install Method (Hook)
_______________________________________________________________________________  */

// WH_KEYBOARD : 키보드 후킹
extern "C" __declspec(dllexport)
HHOOK InstallKeyboardHook()
{
	g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, g_hInstance, 0);

	return g_hKeyboardHook;
}

// WH_MOUSE_LL 마우스 후킹
extern "C" __declspec(dllexport)
HHOOK InstallMouseHook(){
	g_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, g_hInstance, 0);

	return g_hMouseHook;
}

/* _______________________________________________________________________________

Uninstall Method (Hook)
_______________________________________________________________________________  */

// Keyboard의 훅 프로시져를 해제한다.
extern "C" __declspec(dllexport)
void UnInstallKeyboardHook()
{
	UnhookWindowsHookEx(g_hKeyboardHook);
}

// Mouse 후킹 제거
extern "C" __declspec(dllexport)
void UnInstallMouseHook()
{
	UnhookWindowsHookEx(g_hMouseHook);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{


	return DefWindowProc(hWnd, uiMsg, wParam, lParam);
}