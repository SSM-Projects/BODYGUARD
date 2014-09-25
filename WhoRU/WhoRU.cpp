// WhoRU.cpp : 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include "WhoRU.h"
#include "USBview.h"

#pragma warning(disable:4996) 

/* 최초 사용자 등록모드 ON/OFF */
#define INIT_MODE

/* 시작 프로그램 등록모드 ON/OFF */
 #define ENROLL_START_PROGRAM

/////////////////////////////////////////////////////////////// USB 관련 소스

#define NUM_STRING_DESC_TO_GET	32

#pragma comment (lib, "Setupapi.lib")

LIST_ENTRY EnumeratedHCListHead =
{
	&EnumeratedHCListHead,
	&EnumeratedHCListHead
};

DEVICE_GUID_LIST	gHubList;
DEVICE_GUID_LIST	gDeviceList;
USBINFO*			gUSBInfo;		// 인가 USB 정보 저장
USBINFO*			regUSBInfo;		// 등록할 USB 정보

PCHAR ConnectionStatuses[] =
{
	"",                   // 0  - NoDeviceConnected
	"",                   // 1  - DeviceConnected
	"FailedEnumeration",  // 2  - DeviceFailedEnumeration
	"GeneralFailure",     // 3  - DeviceGeneralFailure
	"Overcurrent",        // 4  - DeviceCausedOvercurrent
	"NotEnoughPower",     // 5  - DeviceNotEnoughPower
	"NotEnoughBandwidth", // 6  - DeviceNotEnoughBandwidth
	"HubNestedTooDeeply", // 7  - DeviceHubNestedTooDeeply
	"InLegacyHub",        // 8  - DeviceInLegacyHub
	"Enumerating",        // 9  - DeviceEnumerating
	"Reset"               // 10 - DeviceReset
};

ULONG				TotalDevicesConnected;
USBSENDDEVICEDESC	sendToDeviceDescData;
BOOL				GetUSBDescSuc;
char				SerialNumber[MAX_USB_SERIAL];
int					GetUSBDesc(void);

///////////////////////////////////////////////////////////////

#define MAX_LOADSTRING	100
#define WM_TRAYNOTIFY	(WM_USER+100)

#define MAX_NAME_LENGTH 32		// 최대 사용자 이름 길이
#define IDC_LISTBOX		1001	// 사용자 리스트 박스
#define IDC_DELETE		1002	// 사용자 삭제버튼
#define IDC_TOGGLE		1003	// 사용자 권한토글버튼
#define IDC_EDITBOX		1004	// 새 사용자 이름 에디트 박스
#define IDC_FACE		1005	// 새 사용자 얼굴등록버튼
#define IDC_USB			1006	// 새 사용자 USB 등록버튼
#define IDC_ADMIN		1007	// 새 사용자 관리자권한 체크박스
#define IDC_SAVE		1008	// 새 사용자 등록완료버튼

#define SCLOCK_NEW_USER		2	// 화면 잠금 실행여부 : 사용자 등록
#define SCLOCK_ACTIVATED	1	// 화면 잠금 실행여부 : 실행중
#define SCLOCK_DEACTIVATED	0	// 화면 잠금 실행여부 : 비 실행중

#define STEP_FAIL_FACE		-1	// 얼굴 인식 실패
#define STEP_FAIL_USB		-2	// USB 인식 실패


// 전역 변수:
int NoUSB;
int ScreenSafe;									// 화면 막기 실행 여부

HWND g_hWnd;									// 주 윈도우 핸들
HWND g_hWnd_userlist;							// 사용자 리스트 윈도우 핸들
HWND g_hWnd_listbox;							// 사용자 리스트 박스 핸들
HWND g_hWnd_newuser;							// 새 사용자 등록 윈도우 핸들
HWND g_hWnd_username;							// 새 사용자 이름 에디트 박스 핸들
HWND g_hWnd_checkbox;							// 새 사용자 권한 체크 박스 핸들

char g_selected_user[MAX_NAME_LENGTH];			// 리스트 현재 커서 위치의 사용자 정보
double matrix[399];								// 새 사용자의 얼굴 벡터
char codeMsg[MAX_USB_SERIAL];					// 새 사용자의 USB 암호문
int recogedUser;								// 얼굴 인식 된 사용자의 PK
int userAuth;									// 로그인 된 사용자의 권한

DWORD ObservePID;								// WhoRU_Observer.exe의 PID
HANDLE hReadPipe, hWritePipe;					// WhoRU_Observer PIPE

HINSTANCE hInst;								// 현재 인스턴스입니다.
TCHAR szTitle[MAX_LOADSTRING];					// 제목 표시줄 텍스트입니다.
TCHAR szWindowClass[MAX_LOADSTRING];			// 기본 창 클래스 이름입니다.
HANDLE face_ReadPipe, face_WritePipe;			// FaceRecognition pipe
HANDLE control_ReadPipe, control_WritePipe;		// ControlWindows pipe

HINSTANCE hinstDLL; // DLL 로딩
typedef void(*Taskbar_Show_Hide)(BOOL);
Taskbar_Show_Hide TaskbarShowHide;



// 이 코드 모듈에 들어 있는 함수의 정방향 선언입니다.
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
void				initialization(HWND hWnd);
void				termination(HWND hWnd);
void				USB_remove(HWND hWnd);
void				USB_connect_succ(void);
int					processkill(char* pname);
void				Enroll_Start_Program(void);
void				Recv_data_from_face(void);

// 관리자 메뉴 관련 함수
void				OnCreate(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void				OnTrayNotify(HWND hWnd, UINT messaged, WPARAM wParam, LPARAM lParam);
void				UpdateUserList(void);
void				ToggleUserAuth(void);
void				DeleteUser(void);
void				RegisterFace(void);
void				RegisterUSB(void);
void				UpdateUSB(char *SerialNumber, char path);
void				SaveNewUser(void);

// 듀얼 프로세스 관련 함수
void				Observer(void *);



int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// 여기에 코드를 입력합니다.
	MSG msg;
	HACCEL hAccelTable;

	// 듀얼 프로세스 모듈 가동
	hReadPipe = (HANDLE)(_ttoi(lpCmdLine));
	_beginthread(Observer, 0, 0);

	// 전역 문자열을 초기화합니다.
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_WHORU, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// 응용 프로그램 초기화를 수행합니다.
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WHORU));

	// 기본 메시지 루프입니다.
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
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

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WHORU));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_WHORU);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

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
   hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.

   g_hWnd = CreateWindow(szWindowClass, szTitle, WS_POPUP,
	   CW_USEDEFAULT, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, hInstance, NULL);

   if (!g_hWnd)
   {
	   return FALSE;
   }
   
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
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_DEVICECHANGE:	// 장치 인식 코드
		PDEV_BROADCAST_HDR lpdb;
		switch (wParam)
		{
		case DBT_DEVICEARRIVAL:
			lpdb = (PDEV_BROADCAST_HDR)lParam;
			if (lpdb->dbch_devicetype == DBT_DEVTYP_VOLUME)
			{
				//USB 장착
				if (ScreenSafe == SCLOCK_ACTIVATED)
					GetUSBDesc();
			}
			break;
		case DBT_DEVICEREMOVECOMPLETE:
			lpdb = (PDEV_BROADCAST_HDR)lParam;
			if (lpdb->dbch_devicetype == DBT_DEVTYP_VOLUME)
			{
				//USB 탈착
				if (ScreenSafe == SCLOCK_DEACTIVATED && !CheckUSB())
					USB_remove(hWnd);
			}
			break;
		}
		break;
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// 메뉴 선택을 구문 분석합니다.
		switch (wmId)
		{
		case ID_TRAY_NEWUSER:
			// 새 사용자 등록 윈도우 생성
			g_hWnd_newuser = CreateWindow(szWindowClass, "New User", WS_OVERLAPPED | WS_SYSMENU,
				GetSystemMetrics(SM_CXSCREEN) - 280, GetSystemMetrics(SM_CYSCREEN) - 340, 180, 300, g_hWnd, NULL, hInst, NULL);
			SetMenu(g_hWnd_newuser, NULL);

			// 초기화
			matrix[0] = -1;
			codeMsg[0] = -1;

			// 이름 입력 에디트 박스
			g_hWnd_username = CreateWindow("edit", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER,
				10, 35, 145, 25, g_hWnd_newuser, (HMENU)IDC_EDITBOX, hInst, NULL);
			// 얼굴 등록 버튼
			CreateWindow("button", "Register Face", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				10, 75, 145, 35, g_hWnd_newuser, (HMENU)IDC_FACE, hInst, NULL);
			// USB 등록 버튼
			CreateWindow("button", "Register USB", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				10, 125, 145, 35, g_hWnd_newuser, (HMENU)IDC_USB, hInst, NULL);
			// 관리자 권한 설정 체크박스
			g_hWnd_checkbox = CreateWindow("button", "Admin Authority", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
				10, 175, 145, 30, g_hWnd_newuser, (HMENU)IDC_ADMIN, hInst, NULL);
			// 등록 버튼
			CreateWindow("button", "Save", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				10, 215, 145, 35, g_hWnd_newuser, (HMENU)IDC_SAVE, hInst, NULL);

			ShowWindow(g_hWnd_newuser, SW_SHOW);
			break;
		case ID_TRAY_USERLIST:
			// 사용자 리스트 윈도우 생성
			g_hWnd_userlist = CreateWindow(szWindowClass, "User List", WS_OVERLAPPED | WS_SYSMENU,
				GetSystemMetrics(SM_CXSCREEN) - 280, GetSystemMetrics(SM_CYSCREEN) - 340, 180, 300, g_hWnd, NULL, hInst, NULL);
			SetMenu(g_hWnd_userlist, NULL);

			// 사용자 리스트 데이터 갱신
			UpdateUserList();

			// 관리자 권한 토글 버튼
			CreateWindow("button", "Admin", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				10, 223, 68, 30, g_hWnd_userlist, (HMENU)IDC_TOGGLE, hInst, NULL);
			// 사용자 삭제 버튼
			CreateWindow("button", "Delete", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				85, 223, 68, 30, g_hWnd_userlist, (HMENU)IDC_DELETE, hInst, NULL);

			ShowWindow(g_hWnd_userlist, SW_SHOW);
			break;
		case IDC_LISTBOX:
			if (wmEvent == LBN_SELCHANGE) {
				// 사용자 리스트 현재 커서 위치 갱신
				int cur;
				cur = SendMessage(g_hWnd_listbox, LB_GETCURSEL, 0, 0);
				SendMessage(g_hWnd_listbox, LB_GETTEXT, cur, (LPARAM)g_selected_user);
			}
			break;
		case IDC_DELETE:
			// 사용자 삭제버튼 핸들
			DeleteUser();
			break;
		case IDC_TOGGLE:
			// 관리자 권한토글 버튼 핸들
			ToggleUserAuth();
			break;
		case IDC_FACE:
			// 얼굴 등록 버튼 핸들
			RegisterFace();			
			break;
		case IDC_USB:
			// USB 등록 버튼 핸들
			RegisterUSB();
			break;
		case IDC_SAVE:
			// 새 사용자 등록 버튼 핸들
			SaveNewUser();
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		if (hWnd == g_hWnd_newuser)
			TextOut(hdc, 10, 10, "Name", strlen("Name"));
		EndPaint(hWnd, &ps);
		break;
	case WM_CREATE:
		// 주 프로그램 시작시에만 진행되도록
		if (g_hWnd != NULL)
			break;
		// 완료후 주석 해제, 시작 프로그램 등록
		initialization(hWnd);
		
		// 트레이 아이콘 생성
		OnCreate(hWnd, message, wParam, lParam);
		break;
	case WM_DESTROY:
		// 주 프로그램 종료시에만 진행되도록
		if (g_hWnd != hWnd)
			break;
		// 완료후 주석 해제, 시작 프로그램 등록
		termination(hWnd);
		PostQuitMessage(0);	
		break;
	case WM_TRAYNOTIFY:
		// 트레이 아이콘 클릭 이벤트
		OnTrayNotify(hWnd, message, wParam, lParam);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}


// 정보 대화 상자의 메시지 처리기입니다.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}


// 초기화
void initialization(HWND hWnd)
{
#ifdef ENROLL_START_PROGRAM
	Enroll_Start_Program();
#endif

	processkill("taskmgr.exe");

	hinstDLL = LoadLibrary(_T("KMHook.dll"));
	if (!hinstDLL)
		MessageBox(hWnd, "KMHook.dll 로드 실패!", "알림", MB_OK);

	TaskbarShowHide = (Taskbar_Show_Hide)GetProcAddress(hinstDLL, "Taskbar_Show_Hide");
	TaskbarShowHide(TRUE);

#ifndef INIT_MODE
	ScreenSafe = 1;

	USB_remove(hWnd);
#endif

}


// 종료
void termination(HWND hWnd)
{
#ifdef ENROLL_START_PROGRAM
	Enroll_Start_Program();
#endif
	TaskbarShowHide(TRUE);

	CloseHandle(face_ReadPipe);
	CloseHandle(face_WritePipe);
	CloseHandle(control_ReadPipe);
	CloseHandle(control_WritePipe);

	processkill("ControlWindows.exe");
	processkill("FaceRecognition.exe");
}


// USB 탈착시 이벤트
void USB_remove(HWND hWnd)
{
	ScreenSafe = SCLOCK_ACTIVATED;
	TaskbarShowHide(FALSE);
	
	SECURITY_ATTRIBUTES control_sec;
	control_sec.nLength = sizeof(SECURITY_ATTRIBUTES);
	control_sec.bInheritHandle = TRUE;
	control_sec.lpSecurityDescriptor = NULL;

	if (!CreatePipe(&control_ReadPipe, &control_WritePipe, &control_sec, 0))
		MessageBox(hWnd, "파이프 생성 실패", "경고", MB_OK);

	STARTUPINFO hook_si = { 0 };
	PROCESS_INFORMATION hook_pi = { 0 };
		
	hook_si.cb = sizeof(hook_si);
	hook_si.lpTitle = _T("Child process! ");

	TCHAR command[100];
	_stprintf(command, _T("ControlWindows.exe %d"), (HANDLE)control_ReadPipe);
	
	if (!CreateProcess(NULL, command, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &hook_si, &hook_pi))
		MessageBox(hWnd, "프로세스 생성 실패", "경고", MB_OK);

	Sleep(100);

	SECURITY_ATTRIBUTES face_sec;
	face_sec.nLength = sizeof(SECURITY_ATTRIBUTES);
	face_sec.bInheritHandle = TRUE;
	face_sec.lpSecurityDescriptor = NULL;

	if (!CreatePipe(&face_ReadPipe, &face_WritePipe, &face_sec, 0))
		MessageBox(hWnd, "파이프 생성 실패", "경고", MB_OK);

	STARTUPINFO face_si = { 0 };
	PROCESS_INFORMATION face_pi = { 0 };

	face_si.cb = sizeof(face_si);
	face_si.lpTitle = _T("Admin Console");

	TCHAR f_command[100];
	_stprintf(f_command, _T("FaceRecognition.exe %d"), (HANDLE)face_WritePipe);
	
	if (!CreateProcessA(NULL, f_command, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &face_si, &face_pi))
		MessageBox(hWnd, "프로세스 생성 실패", "경고", MB_OK);
	
	Recv_data_from_face();
}


// Pipe를 이용해 Data 수신 후 전송
void Recv_data_from_face() 
{
	DWORD dwRead;
	CHAR chBuf[10] = { 0 };
	TCHAR sendString[10];
	DWORD bytesWritten;

	int index;

	while (1)
	{	
		if (ReadFile(face_ReadPipe, chBuf, 10, &dwRead, NULL) == TRUE)
		{
			index = atoi(chBuf);
			recogedUser = index;

			_stprintf(sendString, _T("%s"),chBuf);
						
			WriteFile(control_WritePipe, sendString, 10, &bytesWritten, NULL);

			if (index >= 0)
			{
				GetUSBDesc();
				break;
			}
				
			memset(chBuf, 0, 10);
		}
	}
}


// USB 연결시 이벤트
void USB_connect_succ(void)
{
	TaskbarShowHide(TRUE);

	processkill("ControlWindows.exe");
	processkill("FaceRecognition.exe");
}


// Process Kill 함수
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


// 시작 프로그램 등록
// 현재 프로그램이 시작하는 위치를 레지스트리에 등록하여 시작프로그램 구현
void Enroll_Start_Program()
{
	DWORD dwType, cbData;
	HKEY hKey;
	long lRet;
	char pszString[255];
	char path[1000];

	// 키를 오픈한다.
	if ((lRet = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
		0, KEY_READ | KEY_QUERY_VALUE, &hKey)) == ERROR_SUCCESS)
	{
		cbData = 255;	// 문자열 값을 읽어올 데이터의 크기를 준다.
		if ((lRet = RegQueryValueEx(hKey, "WhoAreYou",
			NULL, &dwType, (LPBYTE)&pszString, &cbData)) == ERROR_SUCCESS)
		{
			// 레지스트리 존재
		}
		else
		{
			// 레지스트리 미존재, 생성소스
			GetModuleFileName(NULL, path, 1000);  //현재실행파일이름+경로알아내기
			RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &hKey);
			// 위의 키 밑에 WhoAreYou란 문자열 타입의 값을 만들고 path의 값으로 초기화
			RegSetValueEx(hKey, "WhoAreYou", 0, REG_SZ, (BYTE*)path, strlen(path));
		}
		RegCloseKey(hKey);
	}
}


// 트레이 아이콘 생성
void OnCreate(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	NOTIFYICONDATAW nid;
	ZeroMemory(&nid, sizeof(nid));

	nid.cbSize = sizeof(NOTIFYICONDATAW);
	nid.hWnd = hWnd;
	nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
	nid.uCallbackMessage = WM_TRAYNOTIFY;
	nid.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_TRAY));

	lstrcpyW(nid.szTip, L"BODYGUARD");
	Shell_NotifyIconW(NIM_ADD, &nid);
}


// 트레이 아이콘 클릭 핸들
void OnTrayNotify(HWND hWnd, UINT messaged, WPARAM wParam, LPARAM lParam)
{
	HMENU hMenu, hPopupMenu;
	POINT pt;

	switch (lParam)
	{
	case WM_LBUTTONDOWN:
		break;
	case WM_RBUTTONDOWN:
#ifndef INIT_MODE
		if (userAuth == 0)
			break;
#endif
		hMenu = LoadMenuW(hInst, MAKEINTRESOURCEW(IDR_TRAYMENU));
		hPopupMenu = GetSubMenu(hMenu, 0);
		GetCursorPos(&pt);
		SetForegroundWindow(hWnd);
		TrackPopupMenu(hPopupMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
		SetForegroundWindow(hWnd);
		DestroyMenu(hPopupMenu);
		DestroyMenu(hMenu);
		break;
	}
}


// 사용자 리스트 데이터 갱신
void UpdateUserList(void)
{
	char *name, *auth, buf[7000];
	
	if (g_hWnd_listbox)
		DestroyWindow(g_hWnd_listbox);

	// 리스트 박스 생성
	g_hWnd_listbox = CreateWindow("listbox", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | LBS_SORT,
		10, 10, 143, 205, g_hWnd_userlist, (HMENU)IDC_LISTBOX, hInst, NULL);

	// 데이터 불러오기
	FILE *fp = fopen("Userfaces.dat", "rb");
	while (fgets(buf, 7000, fp)) {
		name = strtok(buf, " ");
		auth = strtok(NULL, " ");
		atoi(auth) ? strcat(name, " (Admin)") : strcat(name, "");
		SendMessage(g_hWnd_listbox, LB_ADDSTRING, 0, (LPARAM)name);
	}
	fclose(fp);
}


// 관리자 권한 토글
void ToggleUserAuth(void)
{
	int i;
	FILE *fp, *fp2;
	char sel[MAX_NAME_LENGTH], name[MAX_NAME_LENGTH], buf[7000];

	for (i = 0; i < MAX_NAME_LENGTH; i++) {
		sel[i] = g_selected_user[i];
		if (sel[i] == ' ') {
			sel[i] = '\0';
			break;
		}
	}

	// 원본 백업
	fp = fopen("Userfaces.dat", "rb");
	fp2 = fopen("Userfaces.bak", "wb");
	while (fgets(buf, 7000, fp))
		fputs(buf, fp2);
	fclose(fp);
	fclose(fp2);

	// 원본 갱신
	fp = fopen("Userfaces.dat", "wb");
	fp2 = fopen("Userfaces.bak", "rb");
	while (fgets(buf, 7000, fp2)) {
		for (i = 0; i < MAX_NAME_LENGTH; i++) {
			name[i] = buf[i];
			if (name[i] == ' ') {
				name[i] = '\0';
				break;
			}
		}
		if (!strcmp(sel, name))
			buf[++i] == '1' ? buf[i] = '0' : buf[i] = '1';
		fputs(buf, fp);
	}
	fclose(fp);
	fclose(fp2);

	// 업데이트 뷰
	UpdateUserList();
}


// 사용자 삭제
void DeleteUser(void)
{
	int i;
	FILE *fp, *fp2;
	char sel[MAX_NAME_LENGTH], name[MAX_NAME_LENGTH], buf[7000];

	for (i = 0; i < MAX_NAME_LENGTH; i++) {
		sel[i] = g_selected_user[i];
		if (sel[i] == ' ') {
			sel[i] = '\0';
			break;
		}
	}

	// 원본 백업
	fp = fopen("Userfaces.dat", "rb");
	fp2 = fopen("Userfaces.bak", "wb");
	while (fgets(buf, 7000, fp))
		fputs(buf, fp2);
	fclose(fp);
	fclose(fp2);

	// 원본 갱신
	fp = fopen("Userfaces.dat", "wb");
	fp2 = fopen("Userfaces.bak", "rb");
	while (fgets(buf, 7000, fp2)) {
		for (i = 0; i < MAX_NAME_LENGTH; i++) {
			name[i] = buf[i];
			if (name[i] == ' ') {
				name[i] = '\0';
				break;
			}
		}
		if (!strcmp(sel, name))
			continue;
		fputs(buf, fp);
	}
	fclose(fp);
	fclose(fp2);

	// 업데이트 뷰
	UpdateUserList();
}


// 새 사용자 얼굴 등록
void RegisterFace(void)
{
	// IPC 파이프 생성
	
	SECURITY_ATTRIBUTES face_sec;
	face_sec.nLength = sizeof(SECURITY_ATTRIBUTES);
	face_sec.bInheritHandle = TRUE;
	face_sec.lpSecurityDescriptor = NULL;

	if (!CreatePipe(&face_ReadPipe, &face_WritePipe, &face_sec, 0))
		MessageBox(g_hWnd, "파이프 생성 실패", "경고", MB_OK);

	// Face 프로세스 얼굴 등록 모드로 실행
	STARTUPINFO face_si = { 0 };
	PROCESS_INFORMATION face_pi = { 0 };

	face_si.cb = sizeof(face_si);
	face_si.lpTitle = _T("Admin Console");

	TCHAR f_command[100];
	_stprintf(f_command, _T("FaceRecognition.exe %d newUserMode"), (HANDLE)face_WritePipe);

	if (!CreateProcessA(NULL, f_command, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &face_si, &face_pi))
		MessageBox(g_hWnd, "프로세스 생성 실패", "경고", MB_OK);

	// 얼굴 데이터 수신
	DWORD dwRead;
	while (1) {
		if (ReadFile(face_ReadPipe, matrix, sizeof(double)* 399, &dwRead, NULL) == TRUE)
			break;
	}

	// Face 프로세스 종료
	CloseHandle(face_ReadPipe);
	CloseHandle(face_WritePipe);
	processkill("FaceRecognition.exe");

	// cam 미연결시 -1, 
	if (matrix[0] == -1)
		MessageBox(g_hWnd, "Cam is not connected!", "Error", MB_OK);
	else
		MessageBox(g_hWnd, "Complete!", "Register Face", MB_OK);
}


// 새 사용자 데이터 저장
void SaveNewUser(void)
{
	FILE *fp;
	int adminAuthority = 0;
	char username[MAX_NAME_LENGTH] = { 0, };

	GetWindowText(g_hWnd_username, username, MAX_NAME_LENGTH);	
	if (SendMessage(g_hWnd_checkbox, BM_GETCHECK, NULL, NULL) == BST_CHECKED)
		adminAuthority = 1;
	
	// 미등록된 것이 있으면 처리
	if (username[0] == 0)
	{
		MessageBox(g_hWnd, "UserName is empty!", "Error", MB_OK);
		return;
	}
	if (matrix[0] == -1)
	{
		MessageBox(g_hWnd, "face information is empty!", "Error", MB_OK);
		return;
	}
	if (codeMsg[0] == -1)
	{
		MessageBox(g_hWnd, "USB information is empty!", "Error", MB_OK);
		return;
	}



	fp = fopen("Userfaces.dat", "ab+");
	fprintf(fp, "%s ", username);
	fprintf(fp, "%d ", adminAuthority);
	for (int i = 0; i < 399; i++)
		fprintf(fp, "%lf ", matrix[i]);
	fprintf(fp, "%s", codeMsg);
	fprintf(fp, "\n");
	fclose(fp);

	for (int i = 0; i < 399; i++)
		matrix[i] = 0.0;
	for (int i = 0; i < MAX_USB_SERIAL; i++)
		codeMsg[i] = 0;

	MessageBox(g_hWnd, "Complete!", "New User", MB_OK);
	DestroyWindow(g_hWnd_newuser);
}


// 프로세스 종료 감시 모듈
void Observer(void *arg)
{
	TCHAR buf[100] = { 0 };
	DWORD dwRead;
	DWORD bytesWritten;
	HANDLE hProcess;
	SECURITY_ATTRIBUTES sec;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	if (hReadPipe == NULL) {
		// 최초 실행
		sec.nLength = sizeof(SECURITY_ATTRIBUTES);
		sec.bInheritHandle = TRUE;
		sec.lpSecurityDescriptor = NULL;

		CreatePipe(&hReadPipe, &hWritePipe, &sec, 0);

		si = { 0 };
		pi = { 0 };

		si.cb = sizeof(si);
		si.lpTitle = _T("WhoRU_Observer ");

		_stprintf(buf, _T("WhoRU_Observer.exe %d"), (HANDLE)hReadPipe);
		CreateProcess(NULL, buf, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);

		_stprintf(buf, _T("%d\0"), GetCurrentProcessId());
		WriteFile(hWritePipe, buf, 100, &bytesWritten, NULL);

		ObservePID = pi.dwProcessId;
	} else {
		// Observer에 의한 실행
		while (1) {
			if (ReadFile(hReadPipe, buf, 100, &dwRead, NULL)) {
				ObservePID = atoi(buf);
				break;
			}
		}
	}

	// 감시 모듈
	while (1) {
		hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ObservePID);
		if (!WaitForSingleObject(hProcess, 500)) {
			sec.nLength = sizeof(SECURITY_ATTRIBUTES);
			sec.bInheritHandle = TRUE;
			sec.lpSecurityDescriptor = NULL;

			CreatePipe(&hReadPipe, &hWritePipe, &sec, 0);

			si = { 0 };
			pi = { 0 };

			si.cb = sizeof(si);
			si.lpTitle = _T("WhoRU_Observer ");

			_stprintf(buf, _T("WhoRU_Observer.exe %d"), (HANDLE)hReadPipe);
			CreateProcess(NULL, buf, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);

			_stprintf(buf, _T("%d\0"), GetCurrentProcessId());
			WriteFile(hWritePipe, buf, 100, &bytesWritten, NULL);

			ObservePID = pi.dwProcessId;
		}
	}
}


// 인가된 USB가 연결중인지 검사 TRUE (연결중) / FALSE (없음)
BOOL CheckUSB(void)
{
	FILE *CHECKfp;
	char path[50] = { 0, };

	if (gUSBInfo != NULL) {
		sprintf(path, "%c:\\BODYGUARD.dat", gUSBInfo->DriverPath);

		if (CHECKfp = fopen(path, "r")) {
			fclose(CHECKfp);
			return TRUE;
		}
		return FALSE;
	}

	return FALSE;
}


// USB에 파일이 있는지 검사 TRUE (있음) / FALSE (없음)
BOOL CheckFile(char *SerialNumber, char path, int serial_len)
{
	BYTE USBbuff[MAX_USB_SERIAL] = { 0, };
	BYTE DBbuff[MAX_USB_SERIAL] = { 0, };
	FILE *USBfp = NULL;
	FILE *DBfp = NULL;
	char filepath[50] = { 0, };

	DWORD WrongUSB;
	DWORD bytesWritten;
	TCHAR sendString[10] = { 0, };

	sprintf(filepath, "%c:\\BODYGUARD.dat", path);



	/********** 화면 제어 중이 아니면 리턴 ***********/

	if (ScreenSafe != SCLOCK_ACTIVATED)
		return TRUE;



	/********** 파일이 존재하지 않으면 리턴 **********/

	if ((USBfp = fopen(filepath, "rb")) == NULL) {
		WrongUSB = STEP_FAIL_USB;

		_stprintf(sendString, _T("%d"), WrongUSB);
		WriteFile(control_WritePipe, sendString, 10, &bytesWritten, NULL);

		return FALSE;
	}



	/********** 파일이 존재하면 확인 진행 ***********/

	fscanf(USBfp, "%s", USBbuff);
	fclose(USBfp);

	int c, cnt = 0;
	DBfp = fopen("Userfaces.dat", "rb");

	if (recogedUser != 0) {
		while ((c = fgetc(DBfp)) != EOF) {
			if (c == '\n') {
				if (++cnt == recogedUser)
					break;
			}
		}
	}

	char gbg_name[MAX_NAME_LENGTH];
	double gbg_face;

	fscanf(DBfp, "%s %d", gbg_name, &userAuth);
	for (int i = 0; i < 399; i++)
		fscanf(DBfp, "%lf", &gbg_face);
	fscanf(DBfp, "%s", DBbuff);
	fclose(DBfp);



	/********** 내용 일치하지 않으면 리턴 ***********/

	if (strcmp((char *)USBbuff, (char *)DBbuff) != 0) {
		WrongUSB = STEP_FAIL_USB;

		_stprintf(sendString, _T("%d"), WrongUSB);
		WriteFile(control_WritePipe, sendString, 10, &bytesWritten, NULL);

		userAuth = 0;
		return FALSE;
	}



	/********** 내용 일치하면 복호화 진행 ***********/

	GenerateKey(BODYGUARD_SEQ_KEY);
	Decrpty(USBbuff, strlen((char *)USBbuff));
	RemoveKey();



	/********** 시리얼 일치하지 않으면 리턴 ***********/

	BYTE SerialNum[MAX_USB_SERIAL] = { 0 };
	strcpy((char*)SerialNum, SerialNumber);

	if (strcmp((char *)USBbuff + 10, (char *)SerialNum) != 0) {
		WrongUSB = STEP_FAIL_USB;

		_stprintf(sendString, _T("%d"), WrongUSB);
		WriteFile(control_WritePipe, sendString, 10, &bytesWritten, NULL);

		userAuth = 0;
		return FALSE;
	}



	/********** 시리얼 일치하면 화면 잠금 해제 ***********/

	UpdateUSB(SerialNumber, path);
	USB_connect_succ();
	ScreenSafe = SCLOCK_DEACTIVATED;

	return TRUE;
}


// 재접속한 사용자의 암호문 갱신
void UpdateUSB(char *SerialNumber, char path)
{
	time_t timeData;
	time(&timeData);

	/* 새로운 암호문 생성 */
	BYTE pbData[MAX_USB_SERIAL] = { 0, };
	GenerateKey(BODYGUARD_SEQ_KEY);
	sprintf((char *)pbData, "%ld", timeData);
	strcat((char *)pbData, SerialNumber);
	Encrypt(pbData, strlen((char *)pbData));
	RemoveKey();

	/* DB 갱신 */
	FILE *fp = fopen("Userfaces.dat", "rb");
	FILE *fp2 = fopen("Userfaces.bak", "wb");
	char buf[7000] = { 0, };
	while (fgets(buf, 7000, fp))
		fputs(buf, fp2);
	fclose(fp);
	fclose(fp2);

	fp = fopen("Userfaces.dat", "wb");
	fp2 = fopen("Userfaces.bak", "rb");
	int i, num = 0, cnt = 0;
	while (fgets(buf, 7000, fp2)) {
		if (recogedUser == num++) {
			for (i = 0; i < 7000; i++) {
				if (buf[i] == ' ')
					++cnt;
				if (cnt == 401)
					break;
			}
			memcpy(buf + (i + 1), pbData, strlen((char *)pbData));
			buf[i + 1 + strlen((char *)pbData)] = '\n';
		}
		fputs(buf, fp);
	}
	fclose(fp);
	fclose(fp2);

	/* USB 내부 숨겨진 파일 복원 */
	TCHAR ccommand[100] = { 0, };
	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	sprintf(ccommand, "cmd /c attrib -R -A -S -H -I %c:\\BODYGUARD.dat", path);
	if (!CreateProcess(NULL, ccommand, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
		MessageBox(g_hWnd, "프로세스 생성 실패", "경고", MB_OK);

	Sleep(1000);

	/* USB 내부 갱신 */
	char fullPath[50] = { 0, };
	sprintf(fullPath, "%c:\\BODYGUARD.dat", path);

	FILE *fp3 = fopen(fullPath, "wb");
	fprintf(fp3, "%s", pbData);
	fclose(fp3);

	/* USB 내부 파일 다시 숨기기 */
	TCHAR ccommand2[100] = { 0, };
	STARTUPINFO si2 = { 0 };
	PROCESS_INFORMATION pi2 = { 0 };
	sprintf(ccommand2, "cmd /c attrib +R +A +S +H +I %c:\\BODYGUARD.dat", path);
	if (!CreateProcess(NULL, ccommand2, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si2, &pi2))
		MessageBox(g_hWnd, "프로세스 생성 실패", "경고", MB_OK);
}


// 새 사용자 USB 등록
void RegisterUSB(void)
{
	ScreenSafe = SCLOCK_NEW_USER;

	GetUSBDesc();

	if (regUSBInfo == NULL)
	{
		MessageBox(g_hWnd, "USB is not connected!", "Error", MB_OK);
		return;
	}

	time_t timeData;
	time(&timeData);

	/* 암호문 생성 */
	BYTE pbData[MAX_USB_SERIAL] = { 0, };
	GenerateKey(BODYGUARD_SEQ_KEY);
	sprintf((char *)pbData, "%ld", timeData);
	strcat((char *)pbData, (char *)regUSBInfo->SerialNumber);
	Encrypt(pbData, strlen((char *)pbData));
	RemoveKey();

	/* 암호문 DB 저장 */
	strcpy(codeMsg, (char *)pbData);

	/* USB 내부 파일 있으면 복원 */
	TCHAR ccommand2[100] = { 0, };
	STARTUPINFO si2 = { 0 };
	PROCESS_INFORMATION pi2 = { 0 };
	sprintf(ccommand2, "cmd /c attrib -R -A -S -H -I %c:\\BODYGUARD.dat", regUSBInfo->DriverPath);
	if (!CreateProcess(NULL, ccommand2, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si2, &pi2))
		MessageBox(g_hWnd, "프로세스 생성 실패", "경고", MB_OK);

	Sleep(1000);

	/* USB 파일 심기 */
	char path[50] = { 0, };
	path[0] = regUSBInfo->DriverPath;
	strcat(path, ":\\BODYGUARD.dat");

	FILE *fp = fopen(path, "wb");
	fprintf(fp, "%s", pbData);
	fclose(fp);

	/* USB 내부 파일 숨기기 */
	TCHAR ccommand[100] = { 0, };
	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	sprintf(ccommand, "cmd /c attrib +R +A +S +H +I %c:\\BODYGUARD.dat", regUSBInfo->DriverPath);
	if (!CreateProcess(NULL, ccommand, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
		MessageBox(g_hWnd, "프로세스 생성 실패", "경고", MB_OK);

	MessageBox(g_hWnd, "Complete!", "Register USB", MB_OK);

	ScreenSafe = SCLOCK_DEACTIVATED;
}










/********************************************************************************************************************
*                                             서비스로 숨기기 소스코드                                              *
*********************************************************************************************************************

#pragma comment(lib, "advapi32.lib")

void InstallService(void);
void BeginService(void);
void StopService(void);
void RemoveService(void);

void InstallService(void)
{
	SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if (schSCManager != 0) {
		SC_HANDLE schService = CreateServiceA(schSCManager, _T("BODYGUARD"), _T("BODYGUARD"),
			SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, _T("C:\\WhoRU\\Debug\\WhoRU.exe"), NULL, NULL, NULL, NULL, NULL);
		if (schService != 0) {
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return;
		}
		CloseServiceHandle(schSCManager);
	}
}

void BeginService(void)
{
	SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager != 0) {
		SC_HANDLE schService = OpenService(schSCManager, _T("BODYGUARD"), SERVICE_ALL_ACCESS);
		if (schService != 0) {
			if (StartService(schService, 0, (const TCHAR**)NULL)) {
				CloseServiceHandle(schService);
				CloseServiceHandle(schSCManager);
			}
			CloseServiceHandle(schService);
		}
		CloseServiceHandle(schSCManager);
	}
}

void RemoveService(void)
{
	SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager != 0) {
		SC_HANDLE schService = OpenServiceA(schSCManager, _T("BODYGUARD"), SERVICE_ALL_ACCESS);
		if (schService != 0) {
			DeleteService(schService);
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return;
		}
		CloseServiceHandle(schSCManager);
	}
}

void StopService(void)
{
	SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager != 0) {
		SC_HANDLE schService = OpenService(schSCManager, _T("BODYGUARD"), SERVICE_ALL_ACCESS);
		if (schService != 0) {
			SERVICE_STATUS status;
			if (ControlService(schService, SERVICE_CONTROL_STOP, &status)) {
				CloseServiceHandle(schService);
				CloseServiceHandle(schSCManager);
				return;
			}
			CloseServiceHandle(schService);
		}
		CloseServiceHandle(schSCManager);
	}
}
*********************************************************************************************************************
*                                             서비스로 숨기기 소스코드                                              *
********************************************************************************************************************/










/********************************************************************************************************************
*                                             스텔스 프로세스 소스코드                                              *
*********************************************************************************************************************

typedef void (*PFN_SetProcName)(LPCTSTR szProcName);\

void				HideProcess(void);
DWORD				_EnableNTPrivilege(LPCTSTR szPrivilege, DWORD dwState);
BOOL				InjectAllProcess(LPCTSTR szDllPath);
BOOL				InjectDll(DWORD dwPID, LPCTSTR szDllPath);

// 프로세스 숨기기
void HideProcess(void)
{
	HMODULE hLib = NULL;
	PFN_SetProcName SetProcName = NULL;

	// 권한 향상
	_EnableNTPrivilege(SE_DEBUG_NAME, SE_PRIVILEGE_ENABLED);

	// DLL 로드
	hLib = LoadLibrary(_T("PCHook.dll"));

	// 숨길 프로세스 설정
	SetProcName = (PFN_SetProcName)GetProcAddress(hLib, "SetProcName");
	SetProcName("WhoRU.exe *32");

	InjectAllProcess(_T("PCHook.dll"));

	// 반환
	FreeLibrary(hLib);
}


// WhoRU.exe 프로세스 권한 향상
DWORD _EnableNTPrivilege(LPCTSTR szPrivilege, DWORD dwState)
{
	DWORD dwRtn = 0;
	HANDLE hToken;
	LUID luid;
	DWORD cbTP;

	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
		if (LookupPrivilegeValue(NULL, szPrivilege, &luid)) {
			BYTE t1[sizeof(TOKEN_PRIVILEGES)+sizeof(LUID_AND_ATTRIBUTES)];
			BYTE t2[sizeof(TOKEN_PRIVILEGES)+sizeof(LUID_AND_ATTRIBUTES)];
			cbTP = sizeof(TOKEN_PRIVILEGES)+sizeof(LUID_AND_ATTRIBUTES);

			PTOKEN_PRIVILEGES pTP = (PTOKEN_PRIVILEGES)t1;
			PTOKEN_PRIVILEGES pPrevTP = (PTOKEN_PRIVILEGES)t2;

			pTP->PrivilegeCount = 1;
			pTP->Privileges[0].Luid = luid;
			pTP->Privileges[0].Attributes = dwState;

			if (AdjustTokenPrivileges(hToken, FALSE, pTP, cbTP, pPrevTP, &cbTP))
				dwRtn = pPrevTP->Privileges[0].Attributes;
		}
		CloseHandle(hToken);
	}
	return dwRtn;
}


// 모든 프로세스에 대한 DLL Injection
BOOL InjectAllProcess(LPCTSTR szDllPath)
{
	DWORD dwPID = 0;
	HANDLE hSnapShot = INVALID_HANDLE_VALUE;
	PROCESSENTRY32 pe;

	// 스냅샷
	pe.dwSize = sizeof(PROCESSENTRY32);
	hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, NULL);

	// 프로세스 검색
	Process32First(hSnapShot, &pe);
	do {
		dwPID = pe.th32ProcessID;
		// 시스템의 안정성을 위해서 PID 가 100 이하는 DLL Injection 수행안함
		if (dwPID == 0 || dwPID == 4)
			continue;
		InjectDll(dwPID, szDllPath);
	} while (Process32Next(hSnapShot, &pe));

	CloseHandle(hSnapShot);

	return TRUE;
}


// 한 프로세스에 대한 DLL Injection
BOOL InjectDll(DWORD dwPID, LPCTSTR szDllPath)
{
	HANDLE hProcess, hThread;
	LPVOID pRemoteBuf;
	DWORD dwBufSize = lstrlen(szDllPath) + 1;
	LPTHREAD_START_ROUTINE pThreadProc;

	if (!(hProcess = OpenProcess(MAXIMUM_ALLOWED, FALSE, dwPID)))
		return FALSE;

	pRemoteBuf = VirtualAllocEx(hProcess, NULL, dwBufSize, MEM_COMMIT, PAGE_READWRITE);
	WriteProcessMemory(hProcess, pRemoteBuf, (LPVOID)szDllPath, dwBufSize, NULL);
	pThreadProc = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA");
	hThread = CreateRemoteThread(hProcess, NULL, 0, pThreadProc, pRemoteBuf, 0, NULL);
	WaitForSingleObject(hThread, INFINITE);
	VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);

	CloseHandle(hThread);
	CloseHandle(hProcess);

	return TRUE;
}
*********************************************************************************************************************
*                                             스텔스 프로세스 소스코드                                              *
********************************************************************************************************************/










/********************************************************************************************************************
*                                                 USB 관련 소스코드                                                 *
********************************************************************************************************************/

int GetUSBDesc(void)
{
	HANDLE                           hHCDev = NULL;
	HDEVINFO                         deviceInfo = NULL;
	SP_DEVINFO_DATA                  deviceInfoData;
	SP_DEVICE_INTERFACE_DATA         deviceInterfaceData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA deviceDetailData = NULL;
	ULONG                            index = 0;
	ULONG                            requiredLength = 0;
	BOOL                             success;

	NoUSB = 0;
	GetUSBDescSuc = FALSE;

	ZeroMemory(&sendToDeviceDescData, sizeof(USBSENDDEVICEDESC));
	gHubList.DeviceInfo = INVALID_HANDLE_VALUE;
	InitializeListHead(&gHubList.ListHead);
	gDeviceList.DeviceInfo = INVALID_HANDLE_VALUE;
	InitializeListHead(&gDeviceList.ListHead);

	TotalDevicesConnected = 0;
	TotalHubs = 0;

	// GUID 기반 인터페이스를 사용하여 호스트 컨트롤러를 반복
	// Handle 반환
	//
	deviceInfo = SetupDiGetClassDevs((LPGUID)&GUID_CLASS_USB_HOST_CONTROLLER,
		NULL,
		NULL,
		(DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));

	deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	for (index = 0; SetupDiEnumDeviceInfo(deviceInfo, index, &deviceInfoData); // 디바이스 정보 요소를 명시하는 구조체 반환
		index++)
	{
		deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

		success = SetupDiEnumDeviceInterfaces(deviceInfo,
			0,
			(LPGUID)&GUID_CLASS_USB_HOST_CONTROLLER,
			index,
			&deviceInterfaceData); // 디바이스 interfaces 를 열거
		if (!success)
		{
			OOPS();
			break;
		}

		success = SetupDiGetDeviceInterfaceDetail(deviceInfo,
			&deviceInterfaceData,
			NULL,
			0,
			&requiredLength,
			NULL); // 디바이스 interface의 정보를 읽어온다. 크기와 함께

		if (!success && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		{
			OOPS();
			break;
		}

		deviceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)ALLOC(requiredLength);
		if (deviceDetailData == NULL)
		{
			OOPS();
			break;
		}

		deviceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		success = SetupDiGetDeviceInterfaceDetail(deviceInfo,
			&deviceInterfaceData,
			deviceDetailData,
			requiredLength,
			&requiredLength,
			NULL); // 디바이스의 경로를 가져온다.

		if (!success)
		{
			OOPS();
			break;
		}

		hHCDev = CreateFile(deviceDetailData->DevicePath,
			GENERIC_WRITE | GENERIC_READ, // 읽기? 쓰기?
			FILE_SHARE_WRITE, // 동시에 쓰기가능
			NULL,
			OPEN_EXISTING, // 있으면 열어
			0,
			NULL); // 해당 Path에 있는 디바이스의 Handle을 얻는다.

		// Handle 얻기를 성공한 경우 그 디바이스의 RootHub의 내용을 가져온다.
		//
		if (hHCDev != INVALID_HANDLE_VALUE)
		{

			EnumerateHostController(hHCDev,
				deviceDetailData->DevicePath,
				deviceInfo,
				&deviceInfoData);

			CloseHandle(hHCDev);
		}

		FREE(deviceDetailData);
	}

	SetupDiDestroyDeviceInfoList(deviceInfo); // deviceInfo 메모리 free

	// USB 실패 시 얼굴인식으로 되돌아감
	if (NoUSB == STEP_FAIL_FACE)
		Recv_data_from_face();

	return 0;
}


VOID
EnumerateHostController(
HANDLE                   hHCDev, _Inout_ PCHAR            leafName,
_In_    HANDLE           deviceInfo,
_In_    PSP_DEVINFO_DATA deviceInfoData
)
{
	PCHAR                   driverKeyName = NULL;
	PCHAR                   rootHubName = NULL;
	PLIST_ENTRY             listEntry = NULL;
	PUSBHOSTCONTROLLERINFO  hcInfo = NULL;
	PUSBHOSTCONTROLLERINFO  hcInfoInList = NULL;
	DWORD                   dwSuccess;
	BOOL                    success = FALSE;
	ULONG                   deviceAndFunction = 0;
	PUSB_DEVICE_PNP_STRINGS DevProps = NULL;


	// 메모리 할당
	//
	hcInfo = (PUSBHOSTCONTROLLERINFO)ALLOC(sizeof(USBHOSTCONTROLLERINFO));

	// 메모리 할당 실패
	//
	if (NULL == hcInfo)
		return;

	hcInfo->DeviceInfoType = HostControllerInfo; // enum -> 0

	// host controller로 부터 해당 드라이버 key 이름을 가져온다.
	//
	driverKeyName = GetHCDDriverKeyName(hHCDev);

	if (NULL == driverKeyName)
	{
		OOPS();
		FREE(hcInfo);
		return;
	}

	// 이미 존재할 경우 다시 열거 하지 않는다.
	//
	listEntry = EnumeratedHCListHead.Flink;

	while (listEntry != &EnumeratedHCListHead)
	{
		hcInfoInList = CONTAINING_RECORD(listEntry,
			USBHOSTCONTROLLERINFO,
			ListEntry);


		listEntry = listEntry->Flink;
	}

	// host controller device 속성들을 얻음
	{
		size_t cbDriverName = 0;
		HRESULT hr = S_OK;

		hr = StringCbLength(driverKeyName, MAX_DRIVER_KEY_NAME, &cbDriverName);
		if (SUCCEEDED(hr))
		{
			DevProps = DriverNameToDeviceProperties(driverKeyName, cbDriverName);
		}
	}

	// 드라이버 키를 얻음 
	//
	hcInfo->DriverKey = driverKeyName;

	if (DevProps)
	{
		ULONG   ven, dev, subsys, rev;
		ven = dev = subsys = rev = 0;

		// DeviceId를 얻음
		//
		if (sscanf_s(DevProps->DeviceId,
			"PCI\\VEN_%x&DEV_%x&SUBSYS_%x&REV_%x",
			&ven, &dev, &subsys, &rev) != 4)
		{
			OOPS();
		}

		// 기타 정보들
		//
		hcInfo->VendorID = ven;
		hcInfo->DeviceID = dev;
		hcInfo->SubSysID = subsys;
		hcInfo->Revision = rev;
		hcInfo->UsbDeviceProperties = DevProps;
	}
	else
	{
		OOPS();
	}

	if (DevProps != NULL && DevProps->DeviceDesc != NULL)
	{
		leafName = DevProps->DeviceDesc;
	}
	else
	{
		OOPS();
	}

	// USB Host Controller의 power map을 얻는다.
	//
	dwSuccess = GetHostControllerPowerMap(hHCDev, hcInfo);

	if (ERROR_SUCCESS != dwSuccess)
	{
		OOPS();
	}


	// 버스, 디바이스 그리고 함수를 얻는다.
	//
	hcInfo->BusDeviceFunctionValid = FALSE;

	// 버스 얻는 부분
	//
	success = SetupDiGetDeviceRegistryProperty(deviceInfo,
		deviceInfoData,
		SPDRP_BUSNUMBER,
		NULL,
		(PBYTE)&hcInfo->BusNumber,
		sizeof(hcInfo->BusNumber),
		NULL);

	if (success)
	{
		// 디바이스의 주소를 얻는 부분
		//
		success = SetupDiGetDeviceRegistryProperty(deviceInfo,
			deviceInfoData,
			SPDRP_ADDRESS,
			NULL,
			(PBYTE)&deviceAndFunction,
			sizeof(deviceAndFunction),
			NULL);
	}

	if (success)
	{
		// 함수를 얻는 부분
		hcInfo->BusDevice = deviceAndFunction >> 16;
		hcInfo->BusFunction = deviceAndFunction & 0xffff;
		hcInfo->BusDeviceFunctionValid = TRUE;
	}

	// USB Host Controller info를 얻는 부분
	dwSuccess = GetHostControllerInfo(hHCDev, hcInfo);

	if (ERROR_SUCCESS != dwSuccess)
	{
		OOPS();
	}

	// 열거된 host controller들을 host controller 리스트에 추가
	//
	InsertTailList(&EnumeratedHCListHead,
		&hcInfo->ListEntry);

	// Root Hub의 이름을 얻는 부분
	// host controller와 root hub를 열거하기 위함
	//
	rootHubName = GetRootHubName(hHCDev);

	if (rootHubName != NULL)
	{
		size_t cbHubName = 0;
		HRESULT hr = S_OK;

		hr = StringCbLength(rootHubName, MAX_DRIVER_KEY_NAME, &cbHubName);
		if (SUCCEEDED(hr))
		{
			EnumerateHub(rootHubName,
				cbHubName,
				NULL,       // ConnectionInfo
				NULL,       // ConnectionInfoV2
				NULL,       // PortConnectorProps
				NULL,       // ConfigDesc
				NULL,       // BosDesc
				NULL,       // StringDescs
				NULL);      // We do not pass DevProps for RootHub
		}
	}
	else
	{
		// Failure obtaining root hub name.

		OOPS();
	}

	return;
}


//*****************************************************************************
//
// EnumerateHub()
//
// HubName - 현재 Hub의 이름. 이 포인터는 유지 되어야 하므로 호출자가 이 메모리를
// free 하거나 재사용 할 수 없다.
//
// ConnectionInfo - NULL if this is a root hub, else this is the connection
// info for an external hub.  This pointer is kept so the caller can neither
// free nor reuse this memory.
//
// ConfigDesc - NULL if this is a root hub, else this is the Configuration
// Descriptor for an external hub.  This pointer is kept so the caller can
// neither free nor reuse this memory.
//
// StringDescs - NULL if this is a root hub.
//
// DevProps - Device properties of the hub
//
//****************************************************************************
VOID
EnumerateHub(
_In_reads_(cbHubName) PCHAR                     HubName,
_In_ size_t                                     cbHubName,
_In_opt_ PUSB_NODE_CONNECTION_INFORMATION_EX    ConnectionInfo,
_In_opt_ PUSB_NODE_CONNECTION_INFORMATION_EX_V2 ConnectionInfoV2,
_In_opt_ PUSB_PORT_CONNECTOR_PROPERTIES         PortConnectorProps,
_In_opt_ PUSB_DESCRIPTOR_REQUEST                ConfigDesc,
_In_opt_ PUSB_DESCRIPTOR_REQUEST                BosDesc,
_In_opt_ PSTRING_DESCRIPTOR_NODE                StringDescs,
_In_opt_ PUSB_DEVICE_PNP_STRINGS                DevProps
)
{
	// Initialize locals to not allocated state so the error cleanup routine
	// only tries to cleanup things that were successfully allocated.
	//
	PUSB_NODE_INFORMATION    hubInfo = NULL;
	PUSB_HUB_INFORMATION_EX  hubInfoEx = NULL;
	PUSB_HUB_CAPABILITIES_EX hubCapabilityEx = NULL;
	HANDLE                  hHubDevice = INVALID_HANDLE_VALUE;
	PVOID                   info = NULL;
	PCHAR                   deviceName = NULL;
	ULONG                   nBytes = 0;
	BOOL                    success = 0;
	DWORD                   dwSizeOfLeafName = 0;
	CHAR                    leafName[512] = { 0 };
	HRESULT                 hr = S_OK;
	size_t                  cchHeader = 0;
	size_t                  cchFullHubName = 0;

	// Allocate some space for a USBDEVICEINFO structure to hold the
	// hub info, hub name, and connection info pointers.  GPTR zero
	// initializes the structure for us.
	//
	info = ALLOC(sizeof(USBEXTERNALHUBINFO));
	if (info == NULL)
	{
		OOPS();
		goto EnumerateHubError;
	}

	// Allocate some space for a USB_NODE_INFORMATION structure for this Hub
	//
	hubInfo = (PUSB_NODE_INFORMATION)ALLOC(sizeof(USB_NODE_INFORMATION));
	if (hubInfo == NULL)
	{
		OOPS();
		goto EnumerateHubError;
	}

	hubInfoEx = (PUSB_HUB_INFORMATION_EX)ALLOC(sizeof(USB_HUB_INFORMATION_EX));
	if (hubInfoEx == NULL)
	{
		OOPS();
		goto EnumerateHubError;
	}

	hubCapabilityEx = (PUSB_HUB_CAPABILITIES_EX)ALLOC(sizeof(USB_HUB_CAPABILITIES_EX));
	if (hubCapabilityEx == NULL)
	{
		OOPS();
		goto EnumerateHubError;
	}

	// Keep copies of the Hub Name, Connection Info, and Configuration
	// Descriptor pointers
	//
	((PUSBROOTHUBINFO)info)->HubInfo = hubInfo;
	((PUSBROOTHUBINFO)info)->HubName = HubName;

	if (ConnectionInfo != NULL)
	{
		((PUSBEXTERNALHUBINFO)info)->DeviceInfoType = ExternalHubInfo;
		((PUSBEXTERNALHUBINFO)info)->ConnectionInfo = ConnectionInfo;
		((PUSBEXTERNALHUBINFO)info)->ConfigDesc = ConfigDesc;
		((PUSBEXTERNALHUBINFO)info)->StringDescs = StringDescs;
		((PUSBEXTERNALHUBINFO)info)->PortConnectorProps = PortConnectorProps;
		((PUSBEXTERNALHUBINFO)info)->HubInfoEx = hubInfoEx;
		((PUSBEXTERNALHUBINFO)info)->HubCapabilityEx = hubCapabilityEx;
		((PUSBEXTERNALHUBINFO)info)->BosDesc = BosDesc;
		((PUSBEXTERNALHUBINFO)info)->ConnectionInfoV2 = ConnectionInfoV2;
		((PUSBEXTERNALHUBINFO)info)->UsbDeviceProperties = DevProps;
	}
	else
	{
		((PUSBROOTHUBINFO)info)->DeviceInfoType = RootHubInfo;
		((PUSBROOTHUBINFO)info)->HubInfoEx = hubInfoEx;
		((PUSBROOTHUBINFO)info)->HubCapabilityEx = hubCapabilityEx;
		((PUSBROOTHUBINFO)info)->PortConnectorProps = PortConnectorProps;
		((PUSBROOTHUBINFO)info)->UsbDeviceProperties = DevProps;
	}

	// Allocate a temp buffer for the full hub device name.
	//
	hr = StringCbLength("\\\\.\\", MAX_DEVICE_PROP, &cchHeader);
	if (FAILED(hr))
	{
		goto EnumerateHubError;
	}
	cchFullHubName = cchHeader + cbHubName + 1;
	deviceName = (PCHAR)ALLOC((DWORD)cchFullHubName);
	if (deviceName == NULL)
	{
		OOPS();
		goto EnumerateHubError;
	}

	// Create the full hub device name
	//
	hr = StringCchCopyN(deviceName, cchFullHubName, "\\\\.\\", cchHeader);
	if (FAILED(hr))
	{
		goto EnumerateHubError;
	}
	hr = StringCchCatN(deviceName, cchFullHubName, HubName, cbHubName);
	if (FAILED(hr))
	{
		goto EnumerateHubError;
	}

	// Try to hub the open device
	//
	hHubDevice = CreateFile(deviceName,
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	// Done with temp buffer for full hub device name
	//
	FREE(deviceName);

	if (hHubDevice == INVALID_HANDLE_VALUE)
	{
		OOPS();
		goto EnumerateHubError;
	}

	//
	// Now query USBHUB for the USB_NODE_INFORMATION structure for this hub.
	// This will tell us the number of downstream ports to enumerate, among
	// other things.
	//
	success = DeviceIoControl(hHubDevice,
		IOCTL_USB_GET_NODE_INFORMATION,
		hubInfo,
		sizeof(USB_NODE_INFORMATION),
		hubInfo,
		sizeof(USB_NODE_INFORMATION),
		&nBytes,
		NULL);

	if (!success)
	{
		OOPS();
		goto EnumerateHubError;
	}

	success = DeviceIoControl(hHubDevice,
		IOCTL_USB_GET_HUB_INFORMATION_EX,
		hubInfoEx,
		sizeof(USB_HUB_INFORMATION_EX),
		hubInfoEx,
		sizeof(USB_HUB_INFORMATION_EX),
		&nBytes,
		NULL);

	//
	// Fail gracefully for downlevel OS's from Win8
	//
	if (!success || nBytes < sizeof(USB_HUB_INFORMATION_EX))
	{
		FREE(hubInfoEx);
		hubInfoEx = NULL;
		if (ConnectionInfo != NULL)
		{
			((PUSBEXTERNALHUBINFO)info)->HubInfoEx = NULL;
		}
		else
		{
			((PUSBROOTHUBINFO)info)->HubInfoEx = NULL;
		}
	}

	//
	// Obtain Hub Capabilities
	//
	success = DeviceIoControl(hHubDevice,
		IOCTL_USB_GET_HUB_CAPABILITIES_EX,
		hubCapabilityEx,
		sizeof(USB_HUB_CAPABILITIES_EX),
		hubCapabilityEx,
		sizeof(USB_HUB_CAPABILITIES_EX),
		&nBytes,
		NULL);

	//
	// Fail gracefully
	//
	if (!success || nBytes < sizeof(USB_HUB_CAPABILITIES_EX))
	{
		FREE(hubCapabilityEx);
		hubCapabilityEx = NULL;
		if (ConnectionInfo != NULL)
		{
			((PUSBEXTERNALHUBINFO)info)->HubCapabilityEx = NULL;
		}
		else
		{
			((PUSBROOTHUBINFO)info)->HubCapabilityEx = NULL;
		}
	}

	// Build the leaf name from the port number and the device description
	//
	dwSizeOfLeafName = sizeof(leafName);
	if (ConnectionInfo)
	{
		StringCchPrintf(leafName, dwSizeOfLeafName, "[Port%d] ", ConnectionInfo->ConnectionIndex);
		StringCchCat(leafName,
			dwSizeOfLeafName,
			ConnectionStatuses[ConnectionInfo->ConnectionStatus]);
		StringCchCatN(leafName,
			dwSizeOfLeafName,
			" :  ",
			sizeof(" :  "));
	}

	if (DevProps)
	{
		size_t cbDeviceDesc = 0;
		hr = StringCbLength(DevProps->DeviceDesc, MAX_DRIVER_KEY_NAME, &cbDeviceDesc);
		if (SUCCEEDED(hr))
		{
			StringCchCatN(leafName,
				dwSizeOfLeafName,
				DevProps->DeviceDesc,
				cbDeviceDesc);
		}
	}
	else
	{
		if (ConnectionInfo != NULL)
		{
			// External hub
			StringCchCatN(leafName,
				dwSizeOfLeafName,
				HubName,
				cbHubName);
		}
		else
		{
			// Root hub
			StringCchCatN(leafName,
				dwSizeOfLeafName,
				"RootHub",
				sizeof("RootHub"));
		}
	}
	/*
	// Now add an item to the TreeView with the PUSBDEVICEINFO pointer info
	// as the LPARAM reference value containing everything we know about the
	// hub.
	//

	if (hItem == NULL)
	{
	OOPS();
	goto EnumerateHubError;
	}
	*/
	// Now recursively enumerate the ports of this hub.
	//
	EnumerateHubPorts(
		hHubDevice,
		hubInfo->u.HubInformation.HubDescriptor.bNumberOfPorts
		);


	CloseHandle(hHubDevice);
	return;

EnumerateHubError:
	//
	// Clean up any stuff that got allocated
	//

	if (hHubDevice != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hHubDevice);
		hHubDevice = INVALID_HANDLE_VALUE;
	}

	if (hubInfo)
	{
		FREE(hubInfo);
	}

	if (hubInfoEx)
	{
		FREE(hubInfoEx);
	}

	if (info)
	{
		FREE(info);
	}

	if (HubName)
	{
		FREE(HubName);
	}

	if (ConnectionInfo)
	{
		FREE(ConnectionInfo);
	}

	if (ConfigDesc)
	{
		FREE(ConfigDesc);
	}

	if (BosDesc)
	{
		FREE(BosDesc);
	}

	if (StringDescs != NULL)
	{
		PSTRING_DESCRIPTOR_NODE Next;

		do {

			Next = StringDescs->Next;
			FREE(StringDescs);
			StringDescs = Next;

		} while (StringDescs != NULL);
	}
}


//*****************************************************************************
//
// EnumerateHubPorts()
//
// hTreeParent - Handle of the TreeView item under which the hub port should
// be added.
//
// hHubDevice - Handle of the hub device to enumerate.
//
// NumPorts - Number of ports on the hub.
//
//*****************************************************************************
VOID
EnumerateHubPorts(
HANDLE      hHubDevice,
ULONG       NumPorts
)
{
	ULONG       index = 0;
	BOOL        success = 0;
	HRESULT     hr = S_OK;
	PCHAR       driverKeyName = NULL;
	PUSB_DEVICE_PNP_STRINGS DevProps;
	DWORD       dwSizeOfLeafName = 0;
	CHAR        leafName[512];
	int         icon = 0;

	PUSB_NODE_CONNECTION_INFORMATION_EX    connectionInfoEx;
	PUSB_PORT_CONNECTOR_PROPERTIES         pPortConnectorProps;
	USB_PORT_CONNECTOR_PROPERTIES          portConnectorProps;
	PUSB_DESCRIPTOR_REQUEST                configDesc;
	PUSB_DESCRIPTOR_REQUEST                bosDesc;
	PSTRING_DESCRIPTOR_NODE                stringDescs;
	PUSBDEVICEINFO                         info;
	PUSB_NODE_CONNECTION_INFORMATION_EX_V2 connectionInfoExV2;
	PDEVICE_INFO_NODE                      pNode;

	// Loop over all ports of the hub.
	//
	// Port indices are 1 based, not 0 based.
	//
	for (index = 1; index <= NumPorts; index++)
	{
		ULONG nBytesEx;
		ULONG nBytes = 0;

		connectionInfoEx = NULL;
		pPortConnectorProps = NULL;
		ZeroMemory(&portConnectorProps, sizeof(portConnectorProps));
		configDesc = NULL;
		bosDesc = NULL;
		stringDescs = NULL;
		info = NULL;
		connectionInfoExV2 = NULL;
		pNode = NULL;
		DevProps = NULL;
		ZeroMemory(leafName, sizeof(leafName));

		//
		// Allocate space to hold the connection info for this port.
		// For now, allocate it big enough to hold info for 30 pipes.
		//
		// Endpoint numbers are 0-15.  Endpoint number 0 is the standard
		// control endpoint which is not explicitly listed in the Configuration
		// Descriptor.  There can be an IN endpoint and an OUT endpoint at
		// endpoint numbers 1-15 so there can be a maximum of 30 endpoints
		// per device configuration.
		//
		// Should probably size this dynamically at some point.
		//

		nBytesEx = sizeof(USB_NODE_CONNECTION_INFORMATION_EX)+
			(sizeof(USB_PIPE_INFO)* 30);

		connectionInfoEx = (PUSB_NODE_CONNECTION_INFORMATION_EX)ALLOC(nBytesEx);

		if (connectionInfoEx == NULL)
		{
			OOPS();
			break;
		}

		connectionInfoExV2 = (PUSB_NODE_CONNECTION_INFORMATION_EX_V2)
			ALLOC(sizeof(USB_NODE_CONNECTION_INFORMATION_EX_V2));

		if (connectionInfoExV2 == NULL)
		{
			OOPS();
			FREE(connectionInfoEx);
			break;
		}

		//
		// Now query USBHUB for the structures
		// for this port.  This will tell us if a device is attached to this
		// port, among other things.
		// The fault tolerate code is executed first.
		//

		portConnectorProps.ConnectionIndex = index;

		success = DeviceIoControl(hHubDevice,
			IOCTL_USB_GET_PORT_CONNECTOR_PROPERTIES,
			&portConnectorProps,
			sizeof(USB_PORT_CONNECTOR_PROPERTIES),
			&portConnectorProps,
			sizeof(USB_PORT_CONNECTOR_PROPERTIES),
			&nBytes,
			NULL);

		if (success && nBytes == sizeof(USB_PORT_CONNECTOR_PROPERTIES))
		{
			pPortConnectorProps = (PUSB_PORT_CONNECTOR_PROPERTIES)
				ALLOC(portConnectorProps.ActualLength);

			if (pPortConnectorProps != NULL)
			{
				pPortConnectorProps->ConnectionIndex = index;

				success = DeviceIoControl(hHubDevice,
					IOCTL_USB_GET_PORT_CONNECTOR_PROPERTIES,
					pPortConnectorProps,
					portConnectorProps.ActualLength,
					pPortConnectorProps,
					portConnectorProps.ActualLength,
					&nBytes,
					NULL);

				if (!success || nBytes < portConnectorProps.ActualLength)
				{
					FREE(pPortConnectorProps);
					pPortConnectorProps = NULL;
				}
			}
		}

		connectionInfoExV2->ConnectionIndex = index;
		connectionInfoExV2->Length = sizeof(USB_NODE_CONNECTION_INFORMATION_EX_V2);
		connectionInfoExV2->SupportedUsbProtocols.Usb300 = 1;

		success = DeviceIoControl(hHubDevice,
			IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX_V2,
			connectionInfoExV2,
			sizeof(USB_NODE_CONNECTION_INFORMATION_EX_V2),
			connectionInfoExV2,
			sizeof(USB_NODE_CONNECTION_INFORMATION_EX_V2),
			&nBytes,
			NULL);

		if (!success || nBytes < sizeof(USB_NODE_CONNECTION_INFORMATION_EX_V2))
		{
			FREE(connectionInfoExV2);
			connectionInfoExV2 = NULL;
		}

		connectionInfoEx->ConnectionIndex = index;

		success = DeviceIoControl(hHubDevice,
			IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
			connectionInfoEx,
			nBytesEx,
			connectionInfoEx,
			nBytesEx,
			&nBytesEx,
			NULL);

		if (success)
		{
			//
			// Since the USB_NODE_CONNECTION_INFORMATION_EX is used to display
			// the device speed, but the hub driver doesn't support indication
			// of superspeed, we overwrite the value if the super speed
			// data structures are available and indicate the device is operating
			// at SuperSpeed.
			// 

			if (connectionInfoEx->Speed == UsbHighSpeed
				&& connectionInfoExV2 != NULL
				&& connectionInfoExV2->Flags.DeviceIsOperatingAtSuperSpeedOrHigher)
			{
				connectionInfoEx->Speed = UsbSuperSpeed;
			}
		}
		else
		{
			PUSB_NODE_CONNECTION_INFORMATION    connectionInfo = NULL;

			// Try using IOCTL_USB_GET_NODE_CONNECTION_INFORMATION
			// instead of IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX
			//

			nBytes = sizeof(USB_NODE_CONNECTION_INFORMATION)+
				sizeof(USB_PIPE_INFO)* 30;

			connectionInfo = (PUSB_NODE_CONNECTION_INFORMATION)ALLOC(nBytes);

			if (connectionInfo == NULL)
			{
				OOPS();

				FREE(connectionInfoEx);
				if (pPortConnectorProps != NULL)
				{
					FREE(pPortConnectorProps);
				}
				if (connectionInfoExV2 != NULL)
				{
					FREE(connectionInfoExV2);
				}
				continue;
			}

			connectionInfo->ConnectionIndex = index;

			success = DeviceIoControl(hHubDevice,
				IOCTL_USB_GET_NODE_CONNECTION_INFORMATION,
				connectionInfo,
				nBytes,
				connectionInfo,
				nBytes,
				&nBytes,
				NULL);

			if (!success)
			{
				OOPS();

				FREE(connectionInfo);
				FREE(connectionInfoEx);
				if (pPortConnectorProps != NULL)
				{
					FREE(pPortConnectorProps);
				}
				if (connectionInfoExV2 != NULL)
				{
					FREE(connectionInfoExV2);
				}
				continue;
			}

			// Copy IOCTL_USB_GET_NODE_CONNECTION_INFORMATION into
			// IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX structure.
			//
			connectionInfoEx->ConnectionIndex = connectionInfo->ConnectionIndex;
			connectionInfoEx->DeviceDescriptor = connectionInfo->DeviceDescriptor;
			connectionInfoEx->CurrentConfigurationValue = connectionInfo->CurrentConfigurationValue;
			connectionInfoEx->Speed = connectionInfo->LowSpeed ? UsbLowSpeed : UsbFullSpeed;
			connectionInfoEx->DeviceIsHub = connectionInfo->DeviceIsHub;
			connectionInfoEx->DeviceAddress = connectionInfo->DeviceAddress;
			connectionInfoEx->NumberOfOpenPipes = connectionInfo->NumberOfOpenPipes;
			connectionInfoEx->ConnectionStatus = connectionInfo->ConnectionStatus;

			memcpy(&connectionInfoEx->PipeList[0],
				&connectionInfo->PipeList[0],
				sizeof(USB_PIPE_INFO)* 30);

			FREE(connectionInfo);
		}

		// Update the count of connected devices
		//
		if (connectionInfoEx->ConnectionStatus == DeviceConnected)
		{
			TotalDevicesConnected++;
		}

		if (connectionInfoEx->DeviceIsHub)
		{
			TotalHubs++;
		}

		// If there is a device connected, get the Device Description
		//
		if (connectionInfoEx->ConnectionStatus != NoDeviceConnected)
		{
			driverKeyName = GetDriverKeyName(hHubDevice, index);

			if (driverKeyName)
			{
				size_t cbDriverName = 0;

				hr = StringCbLength(driverKeyName, MAX_DRIVER_KEY_NAME, &cbDriverName);
				if (SUCCEEDED(hr))
				{
					DevProps = DriverNameToDeviceProperties(driverKeyName, cbDriverName);
					pNode = FindMatchingDeviceNodeForDriverName(driverKeyName, connectionInfoEx->DeviceIsHub);
				}
				FREE(driverKeyName);
			}

		}

		// If there is a device connected to the port, try to retrieve the
		// Configuration Descriptor from the device.
		//
		if (connectionInfoEx->ConnectionStatus == DeviceConnected)
		{
			configDesc = GetConfigDescriptor(hHubDevice,
				index,
				0);
		}
		else
		{
			configDesc = NULL;
		}

		if (configDesc != NULL &&
			connectionInfoEx->DeviceDescriptor.bcdUSB >= 0x0210)
		{
			bosDesc = GetBOSDescriptor(hHubDevice,
				index);
		}
		else
		{
			bosDesc = NULL;
		}

		if (configDesc != NULL &&
			AreThereStringDescriptors(&connectionInfoEx->DeviceDescriptor,
			(PUSB_CONFIGURATION_DESCRIPTOR)(configDesc + 1)))
		{
			stringDescs = GetAllStringDescriptors(
				hHubDevice,
				index,
				&connectionInfoEx->DeviceDescriptor,
				(PUSB_CONFIGURATION_DESCRIPTOR)(configDesc + 1));
		}
		else
		{
			stringDescs = NULL;
		}

		// If the device connected to the port is an external hub, get the
		// name of the external hub and recursively enumerate it.
		//
		if (connectionInfoEx->DeviceIsHub)
		{
			PCHAR extHubName;
			size_t cbHubName = 0;

			extHubName = GetExternalHubName(hHubDevice, index);
			if (extHubName != NULL)
			{
				hr = StringCbLength(extHubName, MAX_DRIVER_KEY_NAME, &cbHubName);
				if (SUCCEEDED(hr))
				{
					EnumerateHub(//hPortItem,
						extHubName,
						cbHubName,
						connectionInfoEx,
						connectionInfoExV2,
						pPortConnectorProps,
						configDesc,
						bosDesc,
						stringDescs,
						DevProps);
				}
			}
		}
		else
		{
			// Allocate some space for a USBDEVICEINFO structure to hold the
			// hub info, hub name, and connection info pointers.  GPTR zero
			// initializes the structure for us.
			//
			info = (PUSBDEVICEINFO)ALLOC(sizeof(USBDEVICEINFO));

			if (info == NULL)
			{
				OOPS();
				if (configDesc != NULL)
				{
					FREE(configDesc);
				}
				if (bosDesc != NULL)
				{
					FREE(bosDesc);
				}
				FREE(connectionInfoEx);

				if (pPortConnectorProps != NULL)
				{
					FREE(pPortConnectorProps);
				}
				if (connectionInfoExV2 != NULL)
				{
					FREE(connectionInfoExV2);
				}
				break;
			}

			info->DeviceInfoType = DeviceInfo;
			info->ConnectionInfo = connectionInfoEx;
			info->PortConnectorProps = pPortConnectorProps;
			info->ConfigDesc = configDesc;
			info->StringDescs = stringDescs;
			info->BosDesc = bosDesc;
			info->ConnectionInfoV2 = connectionInfoExV2;
			info->UsbDeviceProperties = DevProps;
			info->DeviceInfoNode = pNode;

			//결과값


			PUSB_DESCRIPTOR_REQUEST ConfigReqDesc = ((PUSBEXTERNALHUBINFO)info)->ConfigDesc;
			PUSB_COMMON_DESCRIPTOR  commonDesc = (PUSB_COMMON_DESCRIPTOR)(ConfigReqDesc + 1);
			PUSB_CONFIGURATION_DESCRIPTOR   ConfigDesc = (PUSB_CONFIGURATION_DESCRIPTOR)commonDesc;
			PUSB_INTERFACE_DESCRIPTOR   InterfaceDesc;

			if (ConfigReqDesc != NULL && info->ConnectionInfo->DeviceDescriptor.iSerialNumber == 3)
			{
				GetUSBDescSuc = TRUE;
				memcpy(&sendToDeviceDescData.DeviceDescriptor, &info->ConnectionInfo->DeviceDescriptor, sizeof(sendToDeviceDescData.DeviceDescriptor));
				memcpy(&sendToDeviceDescData.ConfigDesc, ConfigDesc, ConfigDesc->bLength);

				commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
				InterfaceDesc = (PUSB_INTERFACE_DESCRIPTOR)commonDesc;

				memcpy(&sendToDeviceDescData.InterfaceDesc, InterfaceDesc, sizeof(sendToDeviceDescData.InterfaceDesc));

				memcpy(&sendToDeviceDescData.EndpointDescriptor[0], &((PUSB_PIPE_INFO)info->ConnectionInfo->PipeList)[0], sizeof(sendToDeviceDescData.EndpointDescriptor[0]));
				memcpy(&sendToDeviceDescData.EndpointDescriptor[1], &((PUSB_PIPE_INFO)info->ConnectionInfo->PipeList)[1], sizeof(sendToDeviceDescData.EndpointDescriptor[0]));

				strncpy_s(sendToDeviceDescData.DeviceId, info->UsbDeviceProperties->DeviceId, 21);
				strcpy_s(sendToDeviceDescData.DeviceDesc, info->UsbDeviceProperties->DeviceDesc);
				strncpy_s(sendToDeviceDescData.HwId, info->UsbDeviceProperties->HwId, 21);
				strcpy_s(sendToDeviceDescData.Service, info->UsbDeviceProperties->Service);
				strcpy_s(sendToDeviceDescData.DeviceClass, info->UsbDeviceProperties->DeviceClass);


				PSTRING_DESCRIPTOR_NODE                StringDescs = NULL;
				StringDescs = ((PUSBDEVICEINFO)info)->StringDescs;

				/* 모든 USB 드라이버 정보 */
				//iManufacturer
				DisplayStringDescriptor(info->ConnectionInfo->DeviceDescriptor.iManufacturer,
					StringDescs,
					info->DeviceInfoNode != NULL ? info->DeviceInfoNode->LatestDevicePowerState : PowerDeviceUnspecified);
				//iProduct
				DisplayStringDescriptor(info->ConnectionInfo->DeviceDescriptor.iProduct,
					StringDescs,
					info->DeviceInfoNode != NULL ? info->DeviceInfoNode->LatestDevicePowerState : PowerDeviceUnspecified);
				//iSerialNumber
				DisplayStringDescriptor(info->ConnectionInfo->DeviceDescriptor.iSerialNumber,
					StringDescs,
					info->DeviceInfoNode != NULL ? info->DeviceInfoNode->LatestDevicePowerState : PowerDeviceUnspecified);


				/* USB info check  */
				if (info != NULL)
				{

					DWORD dwType = REG_BINARY, cbData;
					HKEY hKey;
					long lRet;
					char path[1000];
					char *pBuffer;
					LPSTR aa = NULL;
					int i, j;
					char *cur_serial;
					int count;
					int len;

					// A ~ Z 드라이브 탐색
					int driverpath = 26;
					while (driverpath--)
					{
						count = 0;
						// 키를 오픈한다.
						if ((lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\MountedDevices",
							0, KEY_READ | KEY_QUERY_VALUE, &hKey)) == ERROR_SUCCESS)
						{
							char drive_reg[] = "\\DosDevices\\";

							sprintf(path, "%s%c:", drive_reg, driverpath + 65);

							if ((lRet = RegQueryValueEx(hKey, path,
								NULL, &dwType, NULL, &cbData)) == ERROR_SUCCESS)
							{
								pBuffer = (char*)malloc(cbData);
								cur_serial = (char*)calloc(MAX_USB_SERIAL, 1);



								if (pBuffer == NULL)
								{
									MessageBox(g_hWnd, "Insufficient memory.", "", MB_OK);
								}
								if ((lRet = RegQueryValueEx(hKey, path,
									NULL, &dwType, (LPBYTE)pBuffer, &cbData)) == ERROR_SUCCESS)
								{
									// 레지스트리 존재
									//sprintf(aa, "%s", pBuffer[2]);

									for (i = 0, j = 0; i < (int)cbData; i = i + 2)
									{
										if (pBuffer[i] == '#' || pBuffer[i] == '&')
										{
											count++;
										}
										if (count == 5)
										{
											cur_serial[j] = pBuffer[i];
											j++;
										}
									}
									memmove(cur_serial, cur_serial + 1, 29);

									//len = MAX_USB_SERIAL;
									len = strlen(SerialNumber);

									if (strncmp(SerialNumber, cur_serial, len) == 0)
									{
										//해당 USB의 파일 검사 후 USB 등록
										if (ScreenSafe == SCLOCK_NEW_USER)
										{
											if (gUSBInfo != NULL && gUSBInfo->DriverPath == driverpath + 65)
												break;

											regUSBInfo = (USBINFO*)malloc(sizeof(USBINFO));
											regUSBInfo->DriverPath = driverpath + 65;
											strcpy((char *)regUSBInfo->SerialNumber, SerialNumber);
											regUSBInfo->SerialNumberSize = len;
										}
										else if (CheckFile(SerialNumber, driverpath + 65, len))
										{
											gUSBInfo = (USBINFO*)malloc(sizeof(USBINFO));
											gUSBInfo->DriverPath = driverpath + 65;
											strcpy((char *)gUSBInfo->SerialNumber, SerialNumber);
											gUSBInfo->SerialNumberSize = len;
										}
										else // 파일 없음
										{
											NoUSB = STEP_FAIL_FACE;
											return;
										}
									}
								}
								else
								{
									// 레지스트리 미존재
									//MessageBox(g_hWnd, path, "fail_path", MB_OK);
								}
							}
							RegCloseKey(hKey);
						}
					}
				}
			}

			// Add error description if ConnectionStatus is other than NoDeviceConnected / DeviceConnected
			StringCchCat(leafName,

				sizeof(leafName),
				ConnectionStatuses[connectionInfoEx->ConnectionStatus]);

			if (DevProps)
			{
				size_t cchDeviceDesc = 0;

				hr = StringCbLength(DevProps->DeviceDesc, MAX_DEVICE_PROP, &cchDeviceDesc);
				if (FAILED(hr))
				{
					OOPS();
				}
				dwSizeOfLeafName = sizeof(leafName);
				StringCchCatN(leafName,
					dwSizeOfLeafName - 1,
					" :  ",
					sizeof(" :  "));
				StringCchCatN(leafName,
					dwSizeOfLeafName - 1,
					DevProps->DeviceDesc,
					cchDeviceDesc);
			}

			if (connectionInfoEx->ConnectionStatus == NoDeviceConnected)
			{
				if (connectionInfoExV2 != NULL &&
					connectionInfoExV2->SupportedUsbProtocols.Usb300 == 1)
				{
					icon = NoSsDeviceIcon;
				}
				else
				{
					icon = NoDeviceIcon;
				}
			}
			else if (connectionInfoEx->CurrentConfigurationValue)
			{
				if (connectionInfoEx->Speed == UsbSuperSpeed)
				{
					icon = GoodSsDeviceIcon;
				}
				else
				{
					icon = GoodDeviceIcon;
				}
			}
			else
			{
				icon = BadDeviceIcon;
			}
		}
	} // for
}


/*****************************************************************************

DisplayStringDescriptor()

*****************************************************************************/
VOID
DisplayStringDescriptor(
UCHAR                    Index,
PSTRING_DESCRIPTOR_NODE  StringDescs,
DEVICE_POWER_STATE       LatestDevicePowerState
)
{
	ULONG nBytes = 0;
	BOOLEAN FoundMatchingString = FALSE;
	PCHAR pStr = NULL;
	CHAR  pString[512];

	//@@DisplayStringDescriptor - String Descriptor

	while (StringDescs)
	{
		if (StringDescs->DescriptorIndex == Index)
		{
			FoundMatchingString = TRUE;
			memset(pString, 0, 512);

			if (StringDescs->StringDescriptor->bLength > sizeof(USHORT))
			{
				nBytes = WideCharToMultiByte(
					CP_ACP,     // CodePage
					WC_NO_BEST_FIT_CHARS,
					StringDescs->StringDescriptor->bString,
					(StringDescs->StringDescriptor->bLength - 2) / 2,
					pString,
					512,
					NULL,       // lpDefaultChar
					NULL);      // pUsedDefaultChar
				if (nBytes)
				{
					// serial number
					//MessageBox(g_hWnd, pString, "info", MB_OK);
					if (StringDescs->DescriptorIndex == 3)
						strcpy(SerialNumber, pString);
				}
			}
			else
			{
				//
				// This is NULL string which is invalid
				//
			}
		}
		StringDescs = StringDescs->Next;
	}

}


//*****************************************************************************
//
// GetExternalHubName()
//
//*****************************************************************************
PCHAR GetExternalHubName(
	HANDLE  Hub,
	ULONG   ConnectionIndex
	)
{
	BOOL                        success = 0;
	ULONG                       nBytes = 0;
	USB_NODE_CONNECTION_NAME    extHubName;
	PUSB_NODE_CONNECTION_NAME   extHubNameW = NULL;
	PCHAR                       extHubNameA = NULL;

	// Get the length of the name of the external hub attached to the
	// specified port.
	//
	extHubName.ConnectionIndex = ConnectionIndex;

	success = DeviceIoControl(Hub,
		IOCTL_USB_GET_NODE_CONNECTION_NAME,
		&extHubName,
		sizeof(extHubName),
		&extHubName,
		sizeof(extHubName),
		&nBytes,
		NULL);

	if (!success)
	{
		OOPS();
		goto GetExternalHubNameError;
	}

	// Allocate space to hold the external hub name
	//
	nBytes = extHubName.ActualLength;

	if (nBytes <= sizeof(extHubName))
	{
		OOPS();
		goto GetExternalHubNameError;
	}

	extHubNameW = (PUSB_NODE_CONNECTION_NAME)ALLOC(nBytes);

	if (extHubNameW == NULL)
	{
		OOPS();
		goto GetExternalHubNameError;
	}

	// Get the name of the external hub attached to the specified port
	//
	extHubNameW->ConnectionIndex = ConnectionIndex;

	success = DeviceIoControl(Hub,
		IOCTL_USB_GET_NODE_CONNECTION_NAME,
		extHubNameW,
		nBytes,
		extHubNameW,
		nBytes,
		&nBytes,
		NULL);

	if (!success)
	{
		OOPS();
		goto GetExternalHubNameError;
	}

	// Convert the External Hub name
	//
	extHubNameA = WideStrToMultiStr(extHubNameW->NodeName, nBytes);

	// All done, free the uncoverted external hub name and return the
	// converted external hub name
	//
	FREE(extHubNameW);

	return extHubNameA;


GetExternalHubNameError:
	// There was an error, free anything that was allocated
	//
	if (extHubNameW != NULL)
	{
		FREE(extHubNameW);
		extHubNameW = NULL;
	}

	return NULL;
}


//*****************************************************************************
//
// AreThereStringDescriptors()
//
// DeviceDesc - Device Descriptor for which String Descriptors should be
// checked.
//
// ConfigDesc - Configuration Descriptor (also containing Interface Descriptor)
// for which String Descriptors should be checked.
//
//*****************************************************************************
BOOL
AreThereStringDescriptors(
PUSB_DEVICE_DESCRIPTOR          DeviceDesc,
PUSB_CONFIGURATION_DESCRIPTOR   ConfigDesc
)
{
	PUCHAR                  descEnd = NULL;
	PUSB_COMMON_DESCRIPTOR  commonDesc = NULL;

	//
	// Check Device Descriptor strings
	//

	if (DeviceDesc->iManufacturer ||
		DeviceDesc->iProduct ||
		DeviceDesc->iSerialNumber
		)
	{
		return TRUE;
	}


	//
	// Check the Configuration and Interface Descriptor strings
	//

	descEnd = (PUCHAR)ConfigDesc + ConfigDesc->wTotalLength;

	commonDesc = (PUSB_COMMON_DESCRIPTOR)ConfigDesc;

	while ((PUCHAR)commonDesc + sizeof(USB_COMMON_DESCRIPTOR) < descEnd &&
		(PUCHAR)commonDesc + commonDesc->bLength <= descEnd)
	{
		switch (commonDesc->bDescriptorType)
		{
		case USB_CONFIGURATION_DESCRIPTOR_TYPE:
		case USB_OTHER_SPEED_CONFIGURATION_DESCRIPTOR_TYPE:
			if (commonDesc->bLength != sizeof(USB_CONFIGURATION_DESCRIPTOR))
			{
				OOPS();
				break;
			}
			if (((PUSB_CONFIGURATION_DESCRIPTOR)commonDesc)->iConfiguration)
			{
				return TRUE;
			}
			commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
			continue;

		case USB_INTERFACE_DESCRIPTOR_TYPE:
			if (commonDesc->bLength != sizeof(USB_INTERFACE_DESCRIPTOR) &&
				commonDesc->bLength != sizeof(USB_INTERFACE_DESCRIPTOR2))
			{
				OOPS();
				break;
			}
			if (((PUSB_INTERFACE_DESCRIPTOR)commonDesc)->iInterface)
			{
				return TRUE;
			}
			commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
			continue;

		default:
			commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
			continue;
		}
		break;
	}

	return FALSE;
}


//*****************************************************************************
//
// GetAllStringDescriptors()
//
// hHubDevice - Handle of the hub device containing the port from which the
// String Descriptors will be requested.
//
// ConnectionIndex - Identifies the port on the hub to which a device is
// attached from which the String Descriptors will be requested.
//
// DeviceDesc - Device Descriptor for which String Descriptors should be
// requested.
//
// ConfigDesc - Configuration Descriptor (also containing Interface Descriptor)
// for which String Descriptors should be requested.
//
//*****************************************************************************
PSTRING_DESCRIPTOR_NODE
GetAllStringDescriptors(
HANDLE                          hHubDevice,
ULONG                           ConnectionIndex,
PUSB_DEVICE_DESCRIPTOR          DeviceDesc,
PUSB_CONFIGURATION_DESCRIPTOR   ConfigDesc
)
{
	PSTRING_DESCRIPTOR_NODE supportedLanguagesString = NULL;
	ULONG                   numLanguageIDs = 0;
	USHORT                  *languageIDs = NULL;

	PUCHAR                  descEnd = NULL;
	PUSB_COMMON_DESCRIPTOR  commonDesc = NULL;
	UCHAR                   uIndex = 1;
	UCHAR                   bInterfaceClass = 0;
	BOOL                    getMoreStrings = FALSE;
	HRESULT                 hr = S_OK;

	//
	// Get the array of supported Language IDs, which is returned
	// in String Descriptor 0
	//
	supportedLanguagesString = GetStringDescriptor(hHubDevice,
		ConnectionIndex,
		0,
		0);

	if (supportedLanguagesString == NULL)
	{
		return NULL;
	}

	numLanguageIDs = (supportedLanguagesString->StringDescriptor->bLength - 2) / 2;

	languageIDs = (USHORT *)(&supportedLanguagesString->StringDescriptor->bString[0]);

	//
	// Get the Device Descriptor strings
	//

	if (DeviceDesc->iManufacturer)
	{
		GetStringDescriptors(hHubDevice,
			ConnectionIndex,
			DeviceDesc->iManufacturer,
			numLanguageIDs,
			languageIDs,
			supportedLanguagesString);
	}

	if (DeviceDesc->iProduct)
	{
		GetStringDescriptors(hHubDevice,
			ConnectionIndex,
			DeviceDesc->iProduct,
			numLanguageIDs,
			languageIDs,
			supportedLanguagesString);
	}

	if (DeviceDesc->iSerialNumber)
	{
		GetStringDescriptors(hHubDevice,
			ConnectionIndex,
			DeviceDesc->iSerialNumber,
			numLanguageIDs,
			languageIDs,
			supportedLanguagesString);
	}

	//
	// Get the Configuration and Interface Descriptor strings
	//

	descEnd = (PUCHAR)ConfigDesc + ConfigDesc->wTotalLength;

	commonDesc = (PUSB_COMMON_DESCRIPTOR)ConfigDesc;

	while ((PUCHAR)commonDesc + sizeof(USB_COMMON_DESCRIPTOR) < descEnd &&
		(PUCHAR)commonDesc + commonDesc->bLength <= descEnd)
	{
		switch (commonDesc->bDescriptorType)
		{
		case USB_CONFIGURATION_DESCRIPTOR_TYPE:
			if (commonDesc->bLength != sizeof(USB_CONFIGURATION_DESCRIPTOR))
			{
				OOPS();
				break;
			}
			if (((PUSB_CONFIGURATION_DESCRIPTOR)commonDesc)->iConfiguration)
			{
				GetStringDescriptors(hHubDevice,
					ConnectionIndex,
					((PUSB_CONFIGURATION_DESCRIPTOR)commonDesc)->iConfiguration,
					numLanguageIDs,
					languageIDs,
					supportedLanguagesString);
			}
			commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
			continue;

		case USB_IAD_DESCRIPTOR_TYPE:
			if (commonDesc->bLength < sizeof(USB_IAD_DESCRIPTOR))
			{
				OOPS();
				break;
			}
			if (((PUSB_IAD_DESCRIPTOR)commonDesc)->iFunction)
			{
				GetStringDescriptors(hHubDevice,
					ConnectionIndex,
					((PUSB_IAD_DESCRIPTOR)commonDesc)->iFunction,
					numLanguageIDs,
					languageIDs,
					supportedLanguagesString);
			}
			commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
			continue;

		case USB_INTERFACE_DESCRIPTOR_TYPE:
			if (commonDesc->bLength != sizeof(USB_INTERFACE_DESCRIPTOR) &&
				commonDesc->bLength != sizeof(USB_INTERFACE_DESCRIPTOR2))
			{
				OOPS();
				break;
			}
			if (((PUSB_INTERFACE_DESCRIPTOR)commonDesc)->iInterface)
			{
				GetStringDescriptors(hHubDevice,
					ConnectionIndex,
					((PUSB_INTERFACE_DESCRIPTOR)commonDesc)->iInterface,
					numLanguageIDs,
					languageIDs,
					supportedLanguagesString);
			}

			//
			// We need to display more string descriptors for the following
			// interface classes
			//
			bInterfaceClass = ((PUSB_INTERFACE_DESCRIPTOR)commonDesc)->bInterfaceClass;
			if (bInterfaceClass == USB_DEVICE_CLASS_VIDEO)
			{
				getMoreStrings = TRUE;
			}
			commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
			continue;

		default:
			commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
			continue;
		}
		break;
	}

	if (getMoreStrings)
	{
		//
		// We might need to display strings later that are referenced only in
		// class-specific descriptors. Get String Descriptors 1 through 32 (an
		// arbitrary upper limit for Strings needed due to "bad devices"
		// returning an infinite repeat of Strings 0 through 4) until one is not
		// found.
		//
		// There are also "bad devices" that have issues even querying 1-32, but
		// historically USBView made this query, so the query should be safe for
		// video devices.
		//
		for (uIndex = 1; SUCCEEDED(hr) && (uIndex < NUM_STRING_DESC_TO_GET); uIndex++)
		{
			hr = GetStringDescriptors(hHubDevice,
				ConnectionIndex,
				uIndex,
				numLanguageIDs,
				languageIDs,
				supportedLanguagesString);
		}
	}

	return supportedLanguagesString;
}


//*****************************************************************************
//
// GetStringDescriptors()
//
// hHubDevice - Handle of the hub device containing the port from which the
// String Descriptor will be requested.GetStringDescriptors
//
// ConnectionIndex - Identifies the port on the hub to which a device is
// attached from which the String Descriptor will be requested.
//
// DescriptorIndex - String Descriptor index.
//
// NumLanguageIDs -  Number of languages in which the string should be
// requested.
//
// LanguageIDs - Languages in which the string should be requested.
//
// StringDescNodeHead - First node in linked list of device's string descriptors
//
// Return Value: HRESULT indicating whether the string is on the list
//
//*****************************************************************************
HRESULT
GetStringDescriptors(
_In_ HANDLE                         hHubDevice,
_In_ ULONG                          ConnectionIndex,
_In_ UCHAR                          DescriptorIndex,
_In_ ULONG                          NumLanguageIDs,
_In_reads_(NumLanguageIDs) USHORT  *LanguageIDs,
_In_ PSTRING_DESCRIPTOR_NODE        StringDescNodeHead
)
{
	PSTRING_DESCRIPTOR_NODE tail = NULL;
	PSTRING_DESCRIPTOR_NODE trailing = NULL;
	ULONG i = 0;

	//
	// Go to the end of the linked list, searching for the requested index to
	// see if we've already retrieved it
	//
	for (tail = StringDescNodeHead; tail != NULL; tail = tail->Next)
	{
		if (tail->DescriptorIndex == DescriptorIndex)
		{
			return S_OK;
		}

		trailing = tail;
	}

	tail = trailing;

	//
	// Get the next String Descriptor. If this is NULL, then we're done (return)
	// Otherwise, loop through all Language IDs
	//
	for (i = 0; (tail != NULL) && (i < NumLanguageIDs); i++)
	{
		tail->Next = GetStringDescriptor(hHubDevice,
			ConnectionIndex,
			DescriptorIndex,
			LanguageIDs[i]);

		tail = tail->Next;
	}

	if (tail == NULL)
	{
		return E_FAIL;
	}
	else {
		return S_OK;
	}
}


//*****************************************************************************
//
// GetStringDescriptor()
//
// hHubDevice - Handle of the hub device containing the port from which the
// String Descriptor will be requested.
//
// ConnectionIndex - Identifies the port on the hub to which a device is
// attached from which the String Descriptor will be requested.
//
// DescriptorIndex - String Descriptor index.
//
// LanguageID - Language in which the string should be requested.
//
//*****************************************************************************
PSTRING_DESCRIPTOR_NODE
GetStringDescriptor(
HANDLE  hHubDevice,
ULONG   ConnectionIndex,
UCHAR   DescriptorIndex,
USHORT  LanguageID
)
{
	BOOL    success = 0;
	ULONG   nBytes = 0;
	ULONG   nBytesReturned = 0;

	UCHAR   stringDescReqBuf[sizeof(USB_DESCRIPTOR_REQUEST)+
		MAXIMUM_USB_STRING_LENGTH];

	PUSB_DESCRIPTOR_REQUEST stringDescReq = NULL;
	PUSB_STRING_DESCRIPTOR  stringDesc = NULL;
	PSTRING_DESCRIPTOR_NODE stringDescNode = NULL;

	nBytes = sizeof(stringDescReqBuf);

	stringDescReq = (PUSB_DESCRIPTOR_REQUEST)stringDescReqBuf;
	stringDesc = (PUSB_STRING_DESCRIPTOR)(stringDescReq + 1);

	// Zero fill the entire request structure
	//
	memset(stringDescReq, 0, nBytes);

	// Indicate the port from which the descriptor will be requested
	//
	stringDescReq->ConnectionIndex = ConnectionIndex;

	//
	// USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
	// IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
	//
	// USBD will automatically initialize these fields:
	//     bmRequest = 0x80
	//     bRequest  = 0x06
	//
	// We must inititialize these fields:
	//     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
	//     wIndex    = Zero (or Language ID for String Descriptors)
	//     wLength   = Length of descriptor buffer
	//
	stringDescReq->SetupPacket.wValue = (USB_STRING_DESCRIPTOR_TYPE << 8)
		| DescriptorIndex;

	stringDescReq->SetupPacket.wIndex = LanguageID;

	stringDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

	// Now issue the get descriptor request.
	//
	success = DeviceIoControl(hHubDevice,
		IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
		stringDescReq,
		nBytes,
		stringDescReq,
		nBytes,
		&nBytesReturned,
		NULL);

	//
	// Do some sanity checks on the return from the get descriptor request.
	//

	if (!success)
	{
		OOPS();
		return NULL;
	}

	if (nBytesReturned < 2)
	{
		OOPS();
		return NULL;
	}

	if (stringDesc->bDescriptorType != USB_STRING_DESCRIPTOR_TYPE)
	{
		OOPS();
		return NULL;
	}

	if (stringDesc->bLength != nBytesReturned - sizeof(USB_DESCRIPTOR_REQUEST))
	{
		OOPS();
		return NULL;
	}

	if (stringDesc->bLength % 2 != 0)
	{
		OOPS();
		return NULL;
	}

	//
	// Looks good, allocate some (zero filled) space for the string descriptor
	// node and copy the string descriptor to it.
	//

	stringDescNode = (PSTRING_DESCRIPTOR_NODE)ALLOC(sizeof(STRING_DESCRIPTOR_NODE)+
		stringDesc->bLength);

	if (stringDescNode == NULL)
	{
		OOPS();
		return NULL;
	}

	stringDescNode->DescriptorIndex = DescriptorIndex;
	stringDescNode->LanguageID = LanguageID;

	memcpy(stringDescNode->StringDescriptor,
		stringDesc,
		stringDesc->bLength);

	return stringDescNode;
}


//*****************************************************************************
//
// GetConfigDescriptor()
//
// hHubDevice - Handle of the hub device containing the port from which the
// Configuration Descriptor will be requested.
//
// ConnectionIndex - Identifies the port on the hub to which a device is
// attached from which the Configuration Descriptor will be requested.
//
// DescriptorIndex - Configuration Descriptor index, zero based.
//
//*****************************************************************************
PUSB_DESCRIPTOR_REQUEST
GetConfigDescriptor(
HANDLE  hHubDevice,
ULONG   ConnectionIndex,
UCHAR   DescriptorIndex
)
{
	BOOL    success = 0;
	ULONG   nBytes = 0;
	ULONG   nBytesReturned = 0;

	UCHAR   configDescReqBuf[sizeof(USB_DESCRIPTOR_REQUEST)+
		sizeof(USB_CONFIGURATION_DESCRIPTOR)];

	PUSB_DESCRIPTOR_REQUEST         configDescReq = NULL;
	PUSB_CONFIGURATION_DESCRIPTOR   configDesc = NULL;


	// Request the Configuration Descriptor the first time using our
	// local buffer, which is just big enough for the Cofiguration
	// Descriptor itself.
	//
	nBytes = sizeof(configDescReqBuf);

	configDescReq = (PUSB_DESCRIPTOR_REQUEST)configDescReqBuf;
	configDesc = (PUSB_CONFIGURATION_DESCRIPTOR)(configDescReq + 1);

	// Zero fill the entire request structure
	//
	memset(configDescReq, 0, nBytes);

	// Indicate the port from which the descriptor will be requested
	//
	configDescReq->ConnectionIndex = ConnectionIndex;

	//
	// USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
	// IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
	//
	// USBD will automatically initialize these fields:
	//     bmRequest = 0x80
	//     bRequest  = 0x06
	//
	// We must inititialize these fields:
	//     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
	//     wIndex    = Zero (or Language ID for String Descriptors)
	//     wLength   = Length of descriptor buffer
	//
	configDescReq->SetupPacket.wValue = (USB_CONFIGURATION_DESCRIPTOR_TYPE << 8)
		| DescriptorIndex;

	configDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

	// Now issue the get descriptor request.
	//
	success = DeviceIoControl(hHubDevice,
		IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
		configDescReq,
		nBytes,
		configDescReq,
		nBytes,
		&nBytesReturned,
		NULL);

	if (!success)
	{
		OOPS();
		return NULL;
	}

	if (nBytes != nBytesReturned)
	{
		OOPS();
		return NULL;
	}

	if (configDesc->wTotalLength < sizeof(USB_CONFIGURATION_DESCRIPTOR))
	{
		OOPS();
		return NULL;
	}

	// Now request the entire Configuration Descriptor using a dynamically
	// allocated buffer which is sized big enough to hold the entire descriptor
	//
	nBytes = sizeof(USB_DESCRIPTOR_REQUEST)+configDesc->wTotalLength;

	configDescReq = (PUSB_DESCRIPTOR_REQUEST)ALLOC(nBytes);

	if (configDescReq == NULL)
	{
		OOPS();
		return NULL;
	}

	configDesc = (PUSB_CONFIGURATION_DESCRIPTOR)(configDescReq + 1);

	// Indicate the port from which the descriptor will be requested
	//
	configDescReq->ConnectionIndex = ConnectionIndex;

	//
	// USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
	// IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
	//
	// USBD will automatically initialize these fields:
	//     bmRequest = 0x80
	//     bRequest  = 0x06
	//
	// We must inititialize these fields:
	//     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
	//     wIndex    = Zero (or Language ID for String Descriptors)
	//     wLength   = Length of descriptor buffer
	//
	configDescReq->SetupPacket.wValue = (USB_CONFIGURATION_DESCRIPTOR_TYPE << 8)
		| DescriptorIndex;

	configDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

	// Now issue the get descriptor request.
	//

	success = DeviceIoControl(hHubDevice,
		IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
		configDescReq,
		nBytes,
		configDescReq,
		nBytes,
		&nBytesReturned,
		NULL);

	if (!success)
	{
		OOPS();
		FREE(configDescReq);
		return NULL;
	}

	if (nBytes != nBytesReturned)
	{
		OOPS();
		FREE(configDescReq);
		return NULL;
	}

	if (configDesc->wTotalLength != (nBytes - sizeof(USB_DESCRIPTOR_REQUEST)))
	{
		OOPS();
		FREE(configDescReq);
		return NULL;
	}

	return configDescReq;
}


//*****************************************************************************
//
// GetBOSDescriptor()
//
// hHubDevice - Handle of the hub device containing the port from which the
// Configuration Descriptor will be requested.
//
// ConnectionIndex - Identifies the port on the hub to which a device is
// attached from which the BOS Descriptor will be requested.
//
//*****************************************************************************
PUSB_DESCRIPTOR_REQUEST
GetBOSDescriptor(
HANDLE  hHubDevice,
ULONG   ConnectionIndex
)
{
	BOOL    success = 0;
	ULONG   nBytes = 0;
	ULONG   nBytesReturned = 0;

	UCHAR   bosDescReqBuf[sizeof(USB_DESCRIPTOR_REQUEST)+
		sizeof(USB_BOS_DESCRIPTOR)];

	PUSB_DESCRIPTOR_REQUEST bosDescReq = NULL;
	PUSB_BOS_DESCRIPTOR     bosDesc = NULL;


	// Request the BOS Descriptor the first time using our
	// local buffer, which is just big enough for the BOS
	// Descriptor itself.
	//
	nBytes = sizeof(bosDescReqBuf);

	bosDescReq = (PUSB_DESCRIPTOR_REQUEST)bosDescReqBuf;
	bosDesc = (PUSB_BOS_DESCRIPTOR)(bosDescReq + 1);

	// Zero fill the entire request structure
	//
	memset(bosDescReq, 0, nBytes);

	// Indicate the port from which the descriptor will be requested
	//
	bosDescReq->ConnectionIndex = ConnectionIndex;

	//
	// USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
	// IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
	//
	// USBD will automatically initialize these fields:
	//     bmRequest = 0x80
	//     bRequest  = 0x06
	//
	// We must inititialize these fields:
	//     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
	//     wIndex    = Zero (or Language ID for String Descriptors)
	//     wLength   = Length of descriptor buffer
	//
	bosDescReq->SetupPacket.wValue = (USB_BOS_DESCRIPTOR_TYPE << 8);

	bosDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

	// Now issue the get descriptor request.
	//
	success = DeviceIoControl(hHubDevice,
		IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
		bosDescReq,
		nBytes,
		bosDescReq,
		nBytes,
		&nBytesReturned,
		NULL);

	if (!success)
	{
		OOPS();
		return NULL;
	}

	if (nBytes != nBytesReturned)
	{
		OOPS();
		return NULL;
	}

	if (bosDesc->wTotalLength < sizeof(USB_BOS_DESCRIPTOR))
	{
		OOPS();
		return NULL;
	}

	// Now request the entire BOS Descriptor using a dynamically
	// allocated buffer which is sized big enough to hold the entire descriptor
	//
	nBytes = sizeof(USB_DESCRIPTOR_REQUEST)+bosDesc->wTotalLength;

	bosDescReq = (PUSB_DESCRIPTOR_REQUEST)ALLOC(nBytes);

	if (bosDescReq == NULL)
	{
		OOPS();
		return NULL;
	}

	bosDesc = (PUSB_BOS_DESCRIPTOR)(bosDescReq + 1);

	// Indicate the port from which the descriptor will be requested
	//
	bosDescReq->ConnectionIndex = ConnectionIndex;

	//
	// USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
	// IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
	//
	// USBD will automatically initialize these fields:
	//     bmRequest = 0x80
	//     bRequest  = 0x06
	//
	// We must inititialize these fields:
	//     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
	//     wIndex    = Zero (or Language ID for String Descriptors)
	//     wLength   = Length of descriptor buffer
	//
	bosDescReq->SetupPacket.wValue = (USB_BOS_DESCRIPTOR_TYPE << 8);

	bosDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

	// Now issue the get descriptor request.
	//

	success = DeviceIoControl(hHubDevice,
		IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
		bosDescReq,
		nBytes,
		bosDescReq,
		nBytes,
		&nBytesReturned,
		NULL);

	if (!success)
	{
		OOPS();
		FREE(bosDescReq);
		return NULL;
	}

	if (nBytes != nBytesReturned)
	{
		OOPS();
		FREE(bosDescReq);
		return NULL;
	}

	if (bosDesc->wTotalLength != (nBytes - sizeof(USB_DESCRIPTOR_REQUEST)))
	{
		OOPS();
		FREE(bosDescReq);
		return NULL;
	}

	return bosDescReq;
}


//*****************************************************************************
//
// GetDriverKeyName()
//
//*****************************************************************************
PCHAR GetDriverKeyName(
	HANDLE  Hub,
	ULONG   ConnectionIndex
	)
{
	BOOL                                success = 0;
	ULONG                               nBytes = 0;
	USB_NODE_CONNECTION_DRIVERKEY_NAME  driverKeyName;
	PUSB_NODE_CONNECTION_DRIVERKEY_NAME driverKeyNameW = NULL;
	PCHAR                               driverKeyNameA = NULL;

	// Get the length of the name of the driver key of the device attached to
	// the specified port.
	//
	driverKeyName.ConnectionIndex = ConnectionIndex;

	success = DeviceIoControl(Hub,
		IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
		&driverKeyName,
		sizeof(driverKeyName),
		&driverKeyName,
		sizeof(driverKeyName),
		&nBytes,
		NULL);

	if (!success)
	{
		OOPS();
		goto GetDriverKeyNameError;
	}

	// Allocate space to hold the driver key name
	//
	nBytes = driverKeyName.ActualLength;

	if (nBytes <= sizeof(driverKeyName))
	{
		OOPS();
		goto GetDriverKeyNameError;
	}

	driverKeyNameW = (PUSB_NODE_CONNECTION_DRIVERKEY_NAME)ALLOC(nBytes);
	if (driverKeyNameW == NULL)
	{
		OOPS();
		goto GetDriverKeyNameError;
	}

	// Get the name of the driver key of the device attached to
	// the specified port.
	//
	driverKeyNameW->ConnectionIndex = ConnectionIndex;

	success = DeviceIoControl(Hub,
		IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
		driverKeyNameW,
		nBytes,
		driverKeyNameW,
		nBytes,
		&nBytes,
		NULL);

	if (!success)
	{
		OOPS();
		goto GetDriverKeyNameError;
	}

	// Driver Key name 의 type을 변환 ( WCHAR -> PCHAR )
	//
	driverKeyNameA = WideStrToMultiStr(driverKeyNameW->DriverKeyName, nBytes);

	// 메모리 해제
	//
	FREE(driverKeyNameW);

	return driverKeyNameA;


GetDriverKeyNameError:
	// There was an error, free anything that was allocated
	//
	if (driverKeyNameW != NULL)
	{
		FREE(driverKeyNameW);
		driverKeyNameW = NULL;
	}

	return NULL;
}


PDEVICE_INFO_NODE
FindMatchingDeviceNodeForDriverName(
_In_ PSTR   DriverKeyName,
_In_ BOOLEAN IsHub
)
{
	PDEVICE_INFO_NODE pNode = NULL;
	PDEVICE_GUID_LIST pList = NULL;
	PLIST_ENTRY       pEntry = NULL;

	pList = IsHub ? &gHubList : &gDeviceList;

	pEntry = pList->ListHead.Flink;

	while (pEntry != &pList->ListHead)
	{
		pNode = CONTAINING_RECORD(pEntry,
			DEVICE_INFO_NODE,
			ListEntry);
		if (_stricmp(DriverKeyName, pNode->DeviceDriverName) == 0)
		{
			return pNode;
		}

		pEntry = pEntry->Flink;
	}

	return NULL;
}


//*****************************************************************************
//
// GetRootHubName()
//
//*****************************************************************************

PCHAR GetRootHubName(
	HANDLE HostController
	)
{
	BOOL                success = 0;
	ULONG               nBytes = 0;
	USB_ROOT_HUB_NAME   rootHubName;
	PUSB_ROOT_HUB_NAME  rootHubNameW = NULL;
	PCHAR               rootHubNameA = NULL;

	// Host Controller 가 붙어 있는 Root Hub의 이름의 길이를 얻음
	//
	success = DeviceIoControl(HostController,
		IOCTL_USB_GET_ROOT_HUB_NAME,
		0,
		0,
		&rootHubName,
		sizeof(rootHubName),
		&nBytes,
		NULL);

	if (!success)
	{
		OOPS();
		goto GetRootHubNameError;
	}

	// Root Hub name를 위한 공간 할당 
	//
	nBytes = rootHubName.ActualLength;

	rootHubNameW = (PUSB_ROOT_HUB_NAME)ALLOC(nBytes);
	if (rootHubNameW == NULL)
	{
		OOPS();
		goto GetRootHubNameError;
	}

	// Host Controller가 붙어 있는 Root Hub의 이름을 얻음
	//
	success = DeviceIoControl(HostController,
		IOCTL_USB_GET_ROOT_HUB_NAME,
		NULL,
		0,
		rootHubNameW,
		nBytes,
		&nBytes,
		NULL);
	if (!success)
	{
		OOPS();
		goto GetRootHubNameError;
	}

	// Root Hub의 이름을 변환
	//
	rootHubNameA = WideStrToMultiStr(rootHubNameW->RootHubName, nBytes);

	// 모든 것이 끝나고, 변환된 Root Hub의 이름을 반환하고 변환되지 않은 것은 free.
	//
	FREE(rootHubNameW);

	return rootHubNameA;

GetRootHubNameError:
	// There was an error, free anything that was allocated
	// 
	if (rootHubNameW != NULL)
	{
		FREE(rootHubNameW);
		rootHubNameW = NULL;
	}
	return NULL;
}


//*****************************************************************************
//
// GetHostControllerInfo()
//
// HANDLE hHCDev
//      - USB Host Controller 를 다루기 위한 handle.
//
// PUSBHOSTCONTROLLERINFO hcInfo
//      - Power Map Info를 받기 위한 데이터 구조체
//
// return DWORD dwError
//      - return ERROR_SUCCESS 또는 last error
//
//*****************************************************************************
DWORD
GetHostControllerInfo(
HANDLE hHCDev,
PUSBHOSTCONTROLLERINFO hcInfo)
{
	USBUSER_CONTROLLER_INFO_0 UsbControllerInfo;
	DWORD                      dwError = 0;
	DWORD                      dwBytes = 0;
	BOOL                       bSuccess = FALSE;

	memset(&UsbControllerInfo, 0, sizeof(UsbControllerInfo));

	// header 그리고 요청하는 크기들을 결정
	UsbControllerInfo.Header.UsbUserRequest = USBUSER_GET_CONTROLLER_INFO_0;
	UsbControllerInfo.Header.RequestBufferLength = sizeof(UsbControllerInfo);

	//
	// USB_CONTROLLER_INFO_0 structure 를 얻음
	//
	bSuccess = DeviceIoControl(hHCDev,
		IOCTL_USB_USER_REQUEST,
		&UsbControllerInfo,
		sizeof(UsbControllerInfo),
		&UsbControllerInfo,
		sizeof(UsbControllerInfo),
		&dwBytes,
		NULL);

	if (!bSuccess)
	{
		dwError = GetLastError();
		OOPS();
	}
	else
	{
		hcInfo->ControllerInfo = (PUSB_CONTROLLER_INFO_0)ALLOC(sizeof(USB_CONTROLLER_INFO_0));
		if (NULL == hcInfo->ControllerInfo)
		{
			dwError = GetLastError();
			OOPS();
		}
		else
		{
			// 컴퓨터의 USB Host Controller 의 info structure 를 data에 copy
			memcpy(hcInfo->ControllerInfo, &UsbControllerInfo.Info0, sizeof(USB_CONTROLLER_INFO_0));
		}
	}
	return dwError;
}


//*****************************************************************************
//
// GetHostControllerPowerMap()
//
// HANDLE hHCDev
//      - USB Host Controller 를 다루기 위한 handle.
//
// PUSBHOSTCONTROLLERINFO hcInfo
//      - Power Map Info를 받기 위한 데이터 구조체
//
// return DWORD dwError
//      - return ERROR_SUCCESS 또는 last error
//
//*****************************************************************************

DWORD
GetHostControllerPowerMap(
HANDLE hHCDev,
PUSBHOSTCONTROLLERINFO hcInfo)
{
	USBUSER_POWER_INFO_REQUEST UsbPowerInfoRequest;
	PUSB_POWER_INFO            pUPI = &UsbPowerInfoRequest.PowerInformation;
	DWORD                      dwError = 0;
	DWORD                      dwBytes = 0;
	BOOL                       bSuccess = FALSE;
	int                        nIndex = 0;
	int                        nPowerState = WdmUsbPowerSystemWorking;

	for (; nPowerState <= WdmUsbPowerSystemShutdown; nIndex++, nPowerState++)
	{
		// 0으로 초기화
		memset(&UsbPowerInfoRequest, 0, sizeof(UsbPowerInfoRequest));

		// set the header and request sizes
		UsbPowerInfoRequest.Header.UsbUserRequest = USBUSER_GET_POWER_STATE_MAP;
		UsbPowerInfoRequest.Header.RequestBufferLength = sizeof(UsbPowerInfoRequest);
		UsbPowerInfoRequest.PowerInformation.SystemState = (WDMUSB_POWER_STATE)nPowerState;

		//
		// 현재 hub를 위한 USB_POWER_INFO 구조체를 위한 USBHUB를 알아낸다.
		//
		bSuccess = DeviceIoControl(hHCDev,
			IOCTL_USB_USER_REQUEST,
			&UsbPowerInfoRequest,
			sizeof(UsbPowerInfoRequest),
			&UsbPowerInfoRequest,
			sizeof(UsbPowerInfoRequest),
			&dwBytes,
			NULL);

		if (!bSuccess)
		{
			dwError = GetLastError();
			OOPS();
		}
		else
		{
			// 컴퓨터의 USB Host Controller 의 info structure 를 data에 copy
			memcpy(&(hcInfo->USBPowerInfo[nIndex]), pUPI, sizeof(USB_POWER_INFO));
		}
	}

	return dwError;
}


/*****************************************************************************

DriverNameToDeviceProperties()

DriverName과 일치하는 DevNode 의 장치 속성들을 return.
맞는 DevNode가 없으면 NULL return.

호출자는 FREE()를 이용하여 반환된 구조체를 반드시 free.

*****************************************************************************/
PUSB_DEVICE_PNP_STRINGS
DriverNameToDeviceProperties(
_In_reads_bytes_(cbDriverName) PCHAR  DriverName,
_In_ size_t cbDriverName
)
{
	HDEVINFO        deviceInfo = INVALID_HANDLE_VALUE;
	SP_DEVINFO_DATA deviceInfoData = { 0 };
	ULONG           len;
	BOOL            status;
	PUSB_DEVICE_PNP_STRINGS DevProps = NULL;
	DWORD           lastError;

	// 메모리 할당
	DevProps = (PUSB_DEVICE_PNP_STRINGS)ALLOC(sizeof(USB_DEVICE_PNP_STRINGS));

	if (NULL == DevProps)
	{
		status = FALSE;
		goto Done;
	}

	// device의 instance를 얻음
	status = DriverNameToDeviceInst(DriverName, cbDriverName, &deviceInfo, &deviceInfoData);
	if (status == FALSE)
	{
		goto Done;
	}

	len = 0;
	status = SetupDiGetDeviceInstanceId(deviceInfo,
		&deviceInfoData,
		NULL,
		0,
		&len);
	lastError = GetLastError();


	if (status != FALSE && lastError != ERROR_INSUFFICIENT_BUFFER)
	{
		status = FALSE;
		goto Done;
	}

	//
	// An extra byte is required for the terminating character
	//

	len++;
	DevProps->DeviceId = (PCHAR)ALLOC(len);

	if (DevProps->DeviceId == NULL)
	{
		status = FALSE;
		goto Done;
	}

	status = SetupDiGetDeviceInstanceId(deviceInfo,
		&deviceInfoData,
		DevProps->DeviceId,
		len,
		&len);
	if (status == FALSE)
	{
		goto Done;
	}

	status = GetDeviceProperty(deviceInfo,
		&deviceInfoData,
		SPDRP_DEVICEDESC,
		&DevProps->DeviceDesc);

	if (status == FALSE)
	{
		goto Done;
	}


	//    
	// We don't fail if the following registry query fails as these fields are additional information only
	//

	GetDeviceProperty(deviceInfo,
		&deviceInfoData,
		SPDRP_HARDWAREID,
		&DevProps->HwId);

	GetDeviceProperty(deviceInfo,
		&deviceInfoData,
		SPDRP_SERVICE,
		&DevProps->Service);

	GetDeviceProperty(deviceInfo,
		&deviceInfoData,
		SPDRP_CLASS,
		&DevProps->DeviceClass);
Done:

	if (deviceInfo != INVALID_HANDLE_VALUE)
	{
		SetupDiDestroyDeviceInfoList(deviceInfo);
	}

	if (status == FALSE)
	{
		if (DevProps != NULL)
		{
			FreeDeviceProperties(&DevProps);
		}
	}
	return DevProps;
}


/*****************************************************************************

FreeDeviceProperties()

device properties 구조체를 free.

*****************************************************************************/
VOID FreeDeviceProperties(_In_ PUSB_DEVICE_PNP_STRINGS *ppDevProps)
{
	if (ppDevProps == NULL)
	{
		return;
	}

	if (*ppDevProps == NULL)
	{
		return;
	}

	if ((*ppDevProps)->DeviceId != NULL)
	{
		FREE((*ppDevProps)->DeviceId);
	}

	if ((*ppDevProps)->DeviceDesc != NULL)
	{
		FREE((*ppDevProps)->DeviceDesc);
	}

	//
	// 다음 오는 것은 필요치 않다. but left in case
	// 하지만 공간이 할당된 남은 포이터 필드들이 
	// 남는 경우 나중에 실패가 뜰것이다. 
	//

	if ((*ppDevProps)->HwId != NULL)
	{
		FREE((*ppDevProps)->HwId);
	}

	if ((*ppDevProps)->Service != NULL)
	{
		FREE((*ppDevProps)->Service);
	}

	if ((*ppDevProps)->DeviceClass != NULL)
	{
		FREE((*ppDevProps)->DeviceClass);
	}

	if ((*ppDevProps)->PowerState != NULL)
	{
		FREE((*ppDevProps)->PowerState);
	}

	FREE(*ppDevProps);
	*ppDevProps = NULL;
}


/*****************************************************************************

DriverNameToDeviceInst()

DriverName과 일치하는 DevNode의 Device instance를 찾는다.
DevNode와 일치하는 것이 없으면 FALSE를 있다면 True를 return.

*****************************************************************************/
BOOL
DriverNameToDeviceInst(
_In_reads_bytes_(cbDriverName) PCHAR DriverName,
_In_ size_t cbDriverName,
_Out_ HDEVINFO *pDevInfo,
_Out_writes_bytes_(sizeof(SP_DEVINFO_DATA)) PSP_DEVINFO_DATA pDevInfoData
)
{
	HDEVINFO         deviceInfo = INVALID_HANDLE_VALUE;
	BOOL             status = TRUE;
	ULONG            deviceIndex;
	SP_DEVINFO_DATA  deviceInfoData;
	BOOL             bResult = FALSE;
	PCHAR            pDriverName = NULL;
	PSTR             buf = NULL;
	BOOL             done = FALSE;

	if (pDevInfo == NULL)
	{
		return FALSE;
	}

	if (pDevInfoData == NULL)
	{
		return FALSE;
	}

	memset(pDevInfoData, 0, sizeof(SP_DEVINFO_DATA));

	*pDevInfo = INVALID_HANDLE_VALUE;

	// zero termination 을 보장하기 위해 지역 문자열 변수 사용
	pDriverName = (PCHAR)ALLOC((DWORD)cbDriverName + 1);
	if (NULL == pDriverName)
	{
		status = FALSE;
		goto Done;
	}
	StringCbCopyN(pDriverName, cbDriverName + 1, DriverName, cbDriverName);

	// 만약 주어진 DriverName과 일치하는 어떤 장치가 있다면 
	// 모든 현재 연결된 보이는 장치들을 설명한다.
	//
	deviceInfo = SetupDiGetClassDevs(NULL,
		NULL,
		NULL,
		DIGCF_ALLCLASSES | DIGCF_PRESENT);

	if (deviceInfo == INVALID_HANDLE_VALUE)
	{
		status = FALSE;
		goto Done;
	}

	deviceIndex = 0;
	deviceInfoData.cbSize = sizeof(deviceInfoData);

	while (done == FALSE)
	{
		//
		// 다음 장치의 devinst를 얻음
		//

		status = SetupDiEnumDeviceInfo(deviceInfo,
			deviceIndex,
			&deviceInfoData);

		deviceIndex++;

		if (!status)
		{
			//
			// 이것은 에러 거나 모든 장치들이 처리되고 있음을 안내 하는 것일 수 있다.
			// 또는 원하는 장치를 찾지 못했다는 것일 수 잇다.
			//

			done = TRUE;
			break;
		}

		//
		// DriverName 값을 얻음.
		//

		bResult = GetDeviceProperty(deviceInfo,
			&deviceInfoData,
			SPDRP_DRIVER,
			&buf);

		// DriverName 과 값이 일치하는 경우 DeviceInstance 를  return.
		//
		if (bResult == TRUE && buf != NULL && _stricmp(pDriverName, buf) == 0)
		{
			done = TRUE;
			*pDevInfo = deviceInfo;
			CopyMemory(pDevInfoData, &deviceInfoData, sizeof(deviceInfoData));
			FREE(buf);
			break;
		}

		if (buf != NULL)
		{
			FREE(buf);
			buf = NULL;
		}
	}

Done:

	if (bResult == FALSE)
	{
		if (deviceInfo != INVALID_HANDLE_VALUE)
		{
			SetupDiDestroyDeviceInfoList(deviceInfo);
		}
	}

	if (pDriverName != NULL)
	{
		FREE(pDriverName);
	}

	return status;
}


_Success_(return == TRUE)
BOOL
GetDeviceProperty(
_In_    HDEVINFO         DeviceInfoSet,
_In_    PSP_DEVINFO_DATA DeviceInfoData,
_In_    DWORD            Property,
_Outptr_  LPTSTR        *ppBuffer
)
{
	BOOL bResult;
	DWORD requiredLength = 0;
	DWORD lastError;

	if (ppBuffer == NULL)
	{
		return FALSE;
	}

	*ppBuffer = NULL;

	bResult = SetupDiGetDeviceRegistryProperty(DeviceInfoSet,
		DeviceInfoData,
		Property,
		NULL,
		NULL,
		0,
		&requiredLength); // Plug and Play 디바이스 속성을 검색하여 requiredLength를 읽어온다.
	lastError = GetLastError();

	if ((requiredLength == 0) || (bResult != FALSE && lastError != ERROR_INSUFFICIENT_BUFFER))
	{
		return FALSE;
	}

	*ppBuffer = (LPTSTR)ALLOC(requiredLength);

	if (*ppBuffer == NULL)
	{
		return FALSE;
	}

	bResult = SetupDiGetDeviceRegistryProperty(DeviceInfoSet,
		DeviceInfoData,
		Property,
		NULL,
		(PBYTE)*ppBuffer,
		requiredLength,
		&requiredLength); // 다시 한번 호출해서 해당 디바이스의 속성의 실제 내용을 읽어온다.
	if (bResult == FALSE)
	{
		FREE(*ppBuffer);
		*ppBuffer = NULL;
		return FALSE;
	}

	return TRUE;
}


PCHAR GetHCDDriverKeyName(
	HANDLE  HCD
	)
{
	BOOL                    success = 0;
	ULONG                   nBytes = 0;
	USB_HCD_DRIVERKEY_NAME  driverKeyName = { 0 };
	PUSB_HCD_DRIVERKEY_NAME driverKeyNameW = NULL;
	PCHAR                   driverKeyNameA = NULL;

	ZeroMemory(&driverKeyName, sizeof(driverKeyName));

	// Get the length of the name of the driver key of the HCD
	//
	success = DeviceIoControl(HCD,
		IOCTL_GET_HCD_DRIVERKEY_NAME, // 해당 함수 이름
		&driverKeyName, // inputbuffer의 주소
		sizeof(driverKeyName), // inputbuffer 크기
		&driverKeyName, // outputbuffer의 주소
		sizeof(driverKeyName), // outputbuffer 크기
		&nBytes,
		NULL); // 제어 코드를 직접적으로 해당 디바이스에 전송

	if (!success)
	{
		OOPS();
		goto GetHCDDriverKeyNameError;
	}

	nBytes = driverKeyName.ActualLength;
	if (nBytes <= sizeof(driverKeyName))
	{
		OOPS();
		goto GetHCDDriverKeyNameError;
	}

	// 메모리 할당
	driverKeyNameW = (PUSB_HCD_DRIVERKEY_NAME)ALLOC(nBytes + 1);
	if (driverKeyNameW == NULL)
	{
		OOPS();
		goto GetHCDDriverKeyNameError;
	}

	success = DeviceIoControl(HCD,
		IOCTL_GET_HCD_DRIVERKEY_NAME,
		driverKeyNameW,
		nBytes,
		driverKeyNameW,
		nBytes,
		&nBytes,
		NULL);
	if (!success)
	{
		OOPS();
		goto GetHCDDriverKeyNameError;
	}

	// Convert the driver key name
	//
	driverKeyNameA = WideStrToMultiStr(driverKeyNameW->DriverKeyName, nBytes);

	// All done, free the uncoverted driver key name and return the
	// converted driver key name
	//
	FREE(driverKeyNameW);

	return driverKeyNameA;

GetHCDDriverKeyNameError:
	// There was an error, free anything that was allocated
	//
	if (driverKeyNameW != NULL)
	{
		FREE(driverKeyNameW);
		driverKeyNameW = NULL;
	}

	return NULL;
}


PCHAR WideStrToMultiStr(
	_In_reads_bytes_(cbWideStr) PWCHAR WideStr,
	_In_ size_t                   cbWideStr
	)
{
	ULONG  nBytes = 0;
	PCHAR  MultiStr = NULL;
	PWCHAR pWideStr = NULL;

	// Use local string to guarantee zero termination
	pWideStr = (PWCHAR)ALLOC((DWORD)cbWideStr + 1);
	if (NULL == pWideStr)
	{
		return NULL;
	}
	memset(pWideStr, 0, cbWideStr + 1);
	memcpy(pWideStr, WideStr, cbWideStr);

	// Get the length of the converted string
	//
	nBytes = WideCharToMultiByte(
		CP_ACP,
		WC_NO_BEST_FIT_CHARS,
		pWideStr,
		-1,
		NULL,
		0,
		NULL,
		NULL);

	if (nBytes == 0)
	{
		FREE(pWideStr);
		return NULL;
	}

	// Allocate space to hold the converted string
	//
	MultiStr = (PCHAR)ALLOC(nBytes);
	if (MultiStr == NULL)
	{
		FREE(pWideStr);
		return NULL;
	}

	// Convert the string
	//
	nBytes = WideCharToMultiByte(
		CP_ACP,
		WC_NO_BEST_FIT_CHARS,
		pWideStr,
		-1,
		MultiStr,
		nBytes,
		NULL,
		NULL);

	if (nBytes == 0)
	{
		FREE(MultiStr);
		FREE(pWideStr);
		return NULL;
	}

	FREE(pWideStr);
	return MultiStr;
}

/********************************************************************************************************************
*                                                 USB 관련 소스코드                                                 *
********************************************************************************************************************/