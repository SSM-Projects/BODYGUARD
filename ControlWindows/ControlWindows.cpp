// ControlWindows.cpp : 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#pragma warning(disable : 4996)

/* Hooking ON/OFF */
#define HOOK

#define MAX_LOADSTRING 100

// 전역 변수:
HINSTANCE	hInst;								// 현재 인스턴스입니다.
TCHAR		szTitle[MAX_LOADSTRING];					// 제목 표시줄 텍스트입니다.
TCHAR		szWindowClass[MAX_LOADSTRING];			// 기본 창 클래스 이름입니다.
HANDLE		hReadPipe;
HWND		hhWnd;
int			BGcase;

HINSTANCE hinstDLL; // DLL 로딩
// 후킹을 위한 함수 포인터
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

// 이 코드 모듈에 들어 있는 함수의 정방향 선언입니다.
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

	// TODO: 여기에 코드를 입력합니다.
	MSG msg;
	HACCEL hAccelTable;
	_beginthread(mouseMover, 0, 0);

	hReadPipe = (HANDLE)(_ttoi(lpCmdLine));
	
	// 전역 문자열을 초기화합니다.
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_CONTROLWINDOWS, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// 응용 프로그램 초기화를 수행합니다.
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CONTROLWINDOWS));
	
	// 기본 메시지 루프입니다.
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
//  함수: MyRegisterClass()
//
//  목적: 창 클래스를 등록합니다.
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
//   함수: InitInstance(HINSTANCE, int)
//
//   목적: 인스턴스 핸들을 저장하고 주 창을 만듭니다.
//
//   설명:
//
//        이 함수를 통해 인스턴스 핸들을 전역 변수에 저장하고
//        주 프로그램 창을 만든 다음 표시합니다.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd;

	hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.

	//SW_SHOWMAXIMIZED 최대화 
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
//  함수: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  목적:  주 창의 메시지를 처리합니다.
//
//  WM_COMMAND	- 응용 프로그램 메뉴를 처리합니다.
//  WM_PAINT	- 주 창을 그립니다.
//  WM_DESTROY	- 종료 메시지를 게시하고 반환합니다.
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
		//case WM_DEVICECHANGE:	// 장치 인식 코드
		//	
		//	PDEV_BROADCAST_HDR lpdb;

		//	switch (wParam)
		//	{
		//	case DBT_DEVICEARRIVAL:
		//		lpdb = (PDEV_BROADCAST_HDR)lParam;
		//		if (lpdb->dbch_devicetype == DBT_DEVTYP_PORT)
		//		{
		//			//SD카드 인식
		//		}
		//		if (lpdb->dbch_devicetype == DBT_DEVTYP_VOLUME)
		//		{
		//			//usb 인식				
		//		}
		//		break;
		//	case DBT_DEVICEREMOVECOMPLETE:
		//		lpdb = (PDEV_BROADCAST_HDR)lParam;
		//		if (lpdb->dbch_devicetype == DBT_DEVTYP_PORT)
		//		{
		//			//SD카드 탈착
		//		}
		//		if (lpdb->dbch_devicetype == DBT_DEVTYP_VOLUME)
		//		{
		//			//usb 탈착
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

// 프로그램 시작시 설정
void initialization(HWND hWnd)
{
	DLL_Load(hWnd);
	installKeyhook();
	installMousehook();

	thread_check = TRUE;

	_beginthreadex(NULL, 0, Thread_kill_taskmgr, 0, 0, NULL);  //스레드 시작

	_beginthreadex(NULL, 0, Thread_Recv_data_from_WhoRU, 0, 0, NULL);
}


// 프로그램 종료시 설정 해제
void termination(HWND hWnd)
{
	uninstallKeyhook();
	uninstallMousehook();
	//TaskbarShowHide(TRUE);
	//TaskManagerEnableDisable(TRUE);

	thread_check = FALSE;

	// 메시지 큐 비우기
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

			// 배경 이미지 바꾸기
			if (index == -1) {
				// 얼굴 인식 실패
				BGcase = 1;
				InvalidateRgn(hhWnd, 0, 0);
				Sleep(2000);
				BGcase = 0;
			}
			else if (index >= 0)
				// 얼굴 인식 통과
				BGcase = 2;
			else if (index == -2) {
				// USB 인식 실패
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

// 키보드 마우스 후킹 DLL 로드
void DLL_Load(HWND hWnd)
{
	hinstDLL = LoadLibrary(_T("KMHook.dll"));
	if (!hinstDLL)
		MessageBox(hWnd, "KMHook.dll 로드 실패!", "알림", MB_OK);

	installKeyhook = (InstallKeyboardHook)GetProcAddress(hinstDLL, "InstallKeyboardHook");
	installMousehook = (InstallMouseHook)GetProcAddress(hinstDLL, "InstallMouseHook");

	uninstallKeyhook = (UnInstallKeyboardHook)GetProcAddress(hinstDLL, "UnInstallKeyboardHook");
	uninstallMousehook = (UnInstallMouseHook)GetProcAddress(hinstDLL, "UnInstallMouseHook");

	TaskbarShowHide = (Taskbar_Show_Hide)GetProcAddress(hinstDLL, "Taskbar_Show_Hide");
	//TaskManagerEnableDisable = (TaskManager_Enable_Disable)GetProcAddress(hinstDLL, "TaskManager_Enable_Disable");

}

unsigned __stdcall Thread_kill_taskmgr(void *arg) //스레드 함수
{
	while (thread_check)
	{
		processkill("LogonUI.exe");		// ctrl + alt + del 
		processkill("taskmgr.exe");		// 작업 관리자
		processkill("sethc.exe");		// 고정키
		//taskmgr_terminate();
		Sleep(100);
	}

	return 1;
}

// process kill 함수
int processkill(char* pname)
{
	DWORD dwSize = 250;
	HANDLE hSnapShot;
	PROCESSENTRY32 pEntry;
	BOOL bCrrent = FALSE;

	hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, NULL);

	pEntry.dwSize = sizeof(pEntry);

	// 실행중인 프로세스들의 첫번재 정보를 가져온다.
	Process32First(hSnapShot, &pEntry);

	// Tool이 실행중인지 확인
	while (1)
	{
		// 다음번 프로세스의 정보를 가져온다.
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
					unsigned long nCode; //프로세스 종료 상태
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
* 구현 중인 함수
*/

// LogonUI 화면 뜰시 ESC 눌러서 닫기 구현 - 실패
void taskmgr_terminate()
{
	HANDLE hSnapShot;
	PROCESSENTRY32 pEntry;

	hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, NULL);

	pEntry.dwSize = sizeof(pEntry);
	// 실행중인 프로세스들의 첫번재 정보를 가져온다.
	Process32First(hSnapShot, &pEntry);

	// Tool이 실행중인지 확인
	while (1)
	{
		// 다음번 프로세스의 정보를 가져온다.
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
	// 화면 중앙으로 마우스 이동 후 클릭
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