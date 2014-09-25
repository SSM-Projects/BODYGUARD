// ControlWindows.cpp : ���� ���α׷��� ���� �������� �����մϴ�.
//

#include "stdafx.h"
#pragma warning(disable : 4996)

/* Hooking ON/OFF */
#define HOOK

#define MAX_LOADSTRING 100

// ���� ����:
HINSTANCE	hInst;								// ���� �ν��Ͻ��Դϴ�.
TCHAR		szTitle[MAX_LOADSTRING];					// ���� ǥ���� �ؽ�Ʈ�Դϴ�.
TCHAR		szWindowClass[MAX_LOADSTRING];			// �⺻ â Ŭ���� �̸��Դϴ�.
HANDLE		hReadPipe;
HWND		hhWnd;
int			BGcase;

HINSTANCE hinstDLL; // DLL �ε�
// ��ŷ�� ���� �Լ� ������
typedef HHOOK(*InstallKeyboardHook)();
typedef HHOOK(*InstallMouseHook)();
//typedef void(*SetWindowHandleToDll(HWND))();
typedef void(*UnInstallKeyboardHook)();
typedef void(*UnInstallMouseHook)();
typedef void(*Taskbar_Show_Hide)(BOOL);
typedef void(*StartButton_Show_Hide)(BOOL);
typedef void(*TaskManager_Enable_Disable)(BOOL);

InstallKeyboardHook			installKeyhook;
InstallMouseHook			installMousehook;
UnInstallKeyboardHook		uninstallKeyhook;
UnInstallMouseHook			uninstallMousehook;
Taskbar_Show_Hide			TaskbarShowHide;
StartButton_Show_Hide		StartButtonShowHide;
TaskManager_Enable_Disable	TaskManagerEnableDisable;
//SetWindowHandleToDll WindowHandleToDll;

// �� �ڵ� ��⿡ ��� �ִ� �Լ��� ������ �����Դϴ�.
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
void				initialization(HWND hWnd);
void				termination(HWND hWnd);
void				DLL_Load(HWND hWnd);
unsigned __stdcall	Thread_kill_taskmgr(void *arg);
void				taskmgr_terminate();
int					processkill(char* pname);
unsigned __stdcall	Thread_Recv_data_from_WhoRU(void *arg);
void				mouseMover(void *);

BOOL thread_check;

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	//UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: ���⿡ �ڵ带 �Է��մϴ�.
	MSG msg;
	HACCEL hAccelTable;
	_beginthread(mouseMover, 0, 0);

	hReadPipe = (HANDLE)(_ttoi(lpCmdLine));
	
	// ���� ���ڿ��� �ʱ�ȭ�մϴ�.
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_CONTROLWINDOWS, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// ���� ���α׷� �ʱ�ȭ�� �����մϴ�.
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CONTROLWINDOWS));
	
	// �⺻ �޽��� �����Դϴ�.
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}

//
//  �Լ�: MyRegisterClass()
//
//  ����: â Ŭ������ ����մϴ�.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDC_CONTROLWINDOWS));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCE(IDC_CONTROLWINDOWS);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   �Լ�: InitInstance(HINSTANCE, int)
//
//   ����: �ν��Ͻ� �ڵ��� �����ϰ� �� â�� ����ϴ�.
//
//   ����:
//
//        �� �Լ��� ���� �ν��Ͻ� �ڵ��� ���� ������ �����ϰ�
//        �� ���α׷� â�� ���� ���� ǥ���մϴ�.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd;

	hInst = hInstance; // �ν��Ͻ� �ڵ��� ���� ������ �����մϴ�.

	//SW_SHOWMAXIMIZED �ִ�ȭ 
	//  hWnd = CreateWindow(szWindowClass, szTitle, WS_POPUP, 
	//	   0, 200, GetSystemMetrics(SM_CXSCREEN), 500, NULL, NULL, hInstance, NULL);

	hWnd = CreateWindow(szWindowClass, szTitle, WS_POPUP,
		CW_USEDEFAULT, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, hInstance, NULL);

	if (!hWnd)
	{
		return FALSE;
	}

	hhWnd = hWnd;


	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  �Լ�: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  ����:  �� â�� �޽����� ó���մϴ�.
//
//  WM_COMMAND	- ���� ���α׷� �޴��� ó���մϴ�.
//  WM_PAINT	- �� â�� �׸��ϴ�.
//  WM_DESTROY	- ���� �޽����� �Խ��ϰ� ��ȯ�մϴ�.
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc, memDC;
	HBITMAP srcBitmap, oldBitmap;
	BITMAP bitmap;
	static HINSTANCE hinstDll;
	static HHOOK hKeyHook;
	int width, height;
	
	switch (message)
	{
	case WM_CHAR:
		hdc = BeginPaint(hWnd, &ps);
		if (wParam == ' ')
		{
#ifdef HOOK
			termination(hWnd);
#endif
			DestroyWindow(hWnd);
		}
		break;
		//case WM_DEVICECHANGE:	// ��ġ �ν� �ڵ�
		//	
		//	PDEV_BROADCAST_HDR lpdb;

		//	switch (wParam)
		//	{
		//	case DBT_DEVICEARRIVAL:
		//		lpdb = (PDEV_BROADCAST_HDR)lParam;
		//		if (lpdb->dbch_devicetype == DBT_DEVTYP_PORT)
		//		{
		//			//SDī�� �ν�
		//		}
		//		if (lpdb->dbch_devicetype == DBT_DEVTYP_VOLUME)
		//		{
		//			//usb �ν�				
		//		}
		//		break;
		//	case DBT_DEVICEREMOVECOMPLETE:
		//		lpdb = (PDEV_BROADCAST_HDR)lParam;
		//		if (lpdb->dbch_devicetype == DBT_DEVTYP_PORT)
		//		{
		//			//SDī�� Ż��
		//		}
		//		if (lpdb->dbch_devicetype == DBT_DEVTYP_VOLUME)
		//		{
		//			//usb Ż��
		//		}
		//		break;
		//	}
		//	break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		
		memDC = CreateCompatibleDC(hdc);
		srcBitmap = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BITMAP1+BGcase));
		GetObject(srcBitmap, sizeof(BITMAP), &bitmap);
		oldBitmap = (HBITMAP)SelectObject(memDC, srcBitmap);
		
		width = GetSystemMetrics(SM_CXSCREEN);
		height = GetSystemMetrics(SM_CYSCREEN);
		SetStretchBltMode(hdc, HALFTONE);
		StretchBlt(hdc, 0, 0, bitmap.bmWidth * width / 1920, bitmap.bmHeight * height / 1080, memDC, 0, 0, bitmap.bmWidth, bitmap.bmHeight, SRCCOPY);

		SelectObject(memDC, oldBitmap);
		DeleteObject(srcBitmap);

		DeleteDC(memDC);

		EndPaint(hWnd, &ps);
		break;
	case WM_CREATE:
#ifdef HOOK
		initialization(hWnd);
#endif
		_beginthreadex(NULL, 0, Thread_Recv_data_from_WhoRU, 0, 0, NULL);
		return 0;
	case WM_DESTROY:
#ifdef HOOK
		termination(hWnd);
#endif
		DestroyWindow(hWnd);
		CloseHandle(hReadPipe);

		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// ���α׷� ���۽� ����
void initialization(HWND hWnd)
{
	DLL_Load(hWnd);
	installKeyhook();
	installMousehook();

	thread_check = TRUE;

	_beginthreadex(NULL, 0, Thread_kill_taskmgr, 0, 0, NULL);  //������ ����

	_beginthreadex(NULL, 0, Thread_Recv_data_from_WhoRU, 0, 0, NULL);
}


// ���α׷� ����� ���� ����
void termination(HWND hWnd)
{
	uninstallKeyhook();
	uninstallMousehook();
	//TaskbarShowHide(TRUE);
	//TaskManagerEnableDisable(TRUE);

	thread_check = FALSE;

	// �޽��� ť ����
	/*int msgcount = 0;
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
	msgcount++;
	TranslateMessage(&msg);
	DispatchMessage(&msg);
	}*/


}

unsigned __stdcall Thread_Recv_data_from_WhoRU(void *arg)
{
	DWORD dwRead;
	CHAR chBuf[10] = { 0 };
	int index;

	while (1)
	{
		if (ReadFile(hReadPipe, chBuf, 10, &dwRead, NULL) == TRUE)
		{
			index = atoi(chBuf);

			// ��� �̹��� �ٲٱ�
			if (index == -1) {
				// �� �ν� ����
				BGcase = 1;
				InvalidateRgn(hhWnd, 0, 0);
				Sleep(2000);
				BGcase = 0;
			}
			else if (index >= 0)
				// �� �ν� ���
				BGcase = 2;
			else if (index == -2) {
				// USB �ν� ����
				BGcase = 3;
				InvalidateRgn(hhWnd, 0, 0);
				Sleep(2000);
				BGcase = 0;
			}

			InvalidateRgn(hhWnd, 0, 0);

			//	MessageBox(hhWnd, chBuf, "aaaaaaa", MB_OK);
			memset(chBuf, 0, 10);
		}
	}

	return 1;
}

// Ű���� ���콺 ��ŷ DLL �ε�
void DLL_Load(HWND hWnd)
{
	hinstDLL = LoadLibrary(_T("KMHook.dll"));
	if (!hinstDLL)
		MessageBox(hWnd, "KMHook.dll �ε� ����!", "�˸�", MB_OK);

	installKeyhook = (InstallKeyboardHook)GetProcAddress(hinstDLL, "InstallKeyboardHook");
	installMousehook = (InstallMouseHook)GetProcAddress(hinstDLL, "InstallMouseHook");

	uninstallKeyhook = (UnInstallKeyboardHook)GetProcAddress(hinstDLL, "UnInstallKeyboardHook");
	uninstallMousehook = (UnInstallMouseHook)GetProcAddress(hinstDLL, "UnInstallMouseHook");

	TaskbarShowHide = (Taskbar_Show_Hide)GetProcAddress(hinstDLL, "Taskbar_Show_Hide");
	//TaskManagerEnableDisable = (TaskManager_Enable_Disable)GetProcAddress(hinstDLL, "TaskManager_Enable_Disable");

}

unsigned __stdcall Thread_kill_taskmgr(void *arg) //������ �Լ�
{
	while (thread_check)
	{
		processkill("LogonUI.exe");		// ctrl + alt + del 
		processkill("taskmgr.exe");		// �۾� ������
		processkill("sethc.exe");		// ����Ű
		//taskmgr_terminate();
		Sleep(100);
	}

	return 1;
}

// process kill �Լ�
int processkill(char* pname)
{
	DWORD dwSize = 250;
	HANDLE hSnapShot;
	PROCESSENTRY32 pEntry;
	BOOL bCrrent = FALSE;

	hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, NULL);

	pEntry.dwSize = sizeof(pEntry);

	// �������� ���μ������� ù���� ������ �����´�.
	Process32First(hSnapShot, &pEntry);

	// Tool�� ���������� Ȯ��
	while (1)
	{
		// ������ ���μ����� ������ �����´�.
		BOOL hRes = Process32Next(hSnapShot, &pEntry);

		if (hRes == FALSE)
			break;
		if (!strncmp(pEntry.szExeFile, pname, 15))
		{
			bCrrent = TRUE;
		}
		if (bCrrent)
		{

			HANDLE hProcess = OpenProcess(MAXIMUM_ALLOWED, FALSE, pEntry.th32ProcessID);
			if (hProcess)
			{
				if (TerminateProcess(hProcess, 0))
				{
					unsigned long nCode; //���μ��� ���� ����
					GetExitCodeProcess(hProcess, &nCode);
				}
				CloseHandle(hProcess);
				return 1;
			}
			break;
		}

	}
	return 0;
}





/*
* ���� ���� �Լ�
*/

// LogonUI ȭ�� ��� ESC ������ �ݱ� ���� - ����
void taskmgr_terminate()
{
	HANDLE hSnapShot;
	PROCESSENTRY32 pEntry;

	hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, NULL);

	pEntry.dwSize = sizeof(pEntry);
	// �������� ���μ������� ù���� ������ �����´�.
	Process32First(hSnapShot, &pEntry);

	// Tool�� ���������� Ȯ��
	while (1)
	{
		// ������ ���μ����� ������ �����´�.
		BOOL hRes = Process32Next(hSnapShot, &pEntry);

		if (hRes == FALSE)
			break;
		if (!strncmp(pEntry.szExeFile, "LogonUI.exe", 15))
		{

			Sleep(1000);

			keybd_event(VK_ESCAPE, 0, 0, 0);		// esc key
			keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0);		// esc key

		}
	}
}

void mouseMover(void *arg)
{
	// ȭ�� �߾����� ���콺 �̵� �� Ŭ��
	POINT pt;

	int width = GetSystemMetrics(SM_CXSCREEN);
	int height = GetSystemMetrics(SM_CYSCREEN);

	pt.x = width / 2;
	pt.y = height / 2;

	SetCursorPos(pt.x, pt.y);

	Sleep(5000);

	mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
	mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
}