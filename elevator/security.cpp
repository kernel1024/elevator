#include "stdafx.h"
#include "security.h"
#include "service.h"

#include <WtsApi32.h>

#pragma comment (lib, "wtsapi32.lib")

BOOL CheckCredentials() {
	HANDLE hProcessToken;
	if (!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hProcessToken)) {
		addLogMessage(L"OpenProcessToken failed.");
		CloseHandle(hProcessToken);
		return FALSE;
	}
	PTOKEN_USER ptg = NULL;
	DWORD ptlen = 0;
	GetTokenInformation(hProcessToken,TokenUser,(LPVOID)ptg,0,&ptlen);
	if (ptlen<=0) {
		addLogMessage(L"GetTokenInformation failed.");
		CloseHandle(hProcessToken);
		return FALSE;
	}
	ptg = (PTOKEN_USER)malloc(ptlen);
	if (!GetTokenInformation(hProcessToken,TokenUser,(LPVOID)ptg,ptlen,&ptlen)) {
		addLogMessage(L"GetTokenInformation failed 2.");
		CloseHandle(hProcessToken);
		return FALSE;
	}
	DWORD accLen = MAX_PATH;
	DWORD domLen = MAX_PATH;
	wchar_t account[MAX_PATH];
	wchar_t domain[MAX_PATH];
	SID_NAME_USE eUse;
	if (!LookupAccountSidW(NULL,ptg->User.Sid,account,(LPDWORD)&accLen,domain,(LPDWORD)&domLen,&eUse)) {
		addLogMessage(L"LookupAccountSid failed 2.");
		free(ptg);
		CloseHandle(hProcessToken);
		return FALSE;
	}
	BOOL res = ((_wcsicmp(domain,L"nt authority") == 0) &&
		(_wcsicmp(account,L"system")==0));
	return res;
}

BOOL AdjustPrivilege(LPCWSTR privilege, BOOL state) {
	HANDLE hProcessToken;
	if (!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES,&hProcessToken)) {
		addLogMessage(L"OpenProcessToken failed.");
		return FALSE;
	}
	TOKEN_PRIVILEGES tp;
	LUID luid;
	if (!LookupPrivilegeValueW(NULL,SE_TCB_NAME,&luid)) {
		addLogMessage(L"LookupPrivilegeValueW failed.");
		CloseHandle(hProcessToken);
		return FALSE;
	}
	tp.PrivilegeCount=1;
	tp.Privileges[0].Luid=luid;
	if (state==TRUE)
		tp.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;
	else
		tp.Privileges[0].Attributes=SE_PRIVILEGE_REMOVED;
	if (!AdjustTokenPrivileges(hProcessToken,FALSE,&tp,sizeof(TOKEN_PRIVILEGES),(PTOKEN_PRIVILEGES)NULL,(PDWORD)NULL)) {
		addLogMessage(L"AdjustTokenPrivileges failed.");
		CloseHandle(hProcessToken);
		return FALSE;
	}
	if (GetLastError()==ERROR_NOT_ALL_ASSIGNED) {
		addLogMessage(L"Not all privileges assigned.\n");
		CloseHandle(hProcessToken);
		return FALSE;
	}
	CloseHandle(hProcessToken);
	return TRUE;
}

int SecurityWork() {
	PWTS_SESSION_INFOW sessions;
	DWORD sessCnt;

	if (!AdjustPrivilege(SE_TCB_NAME,TRUE)) return -9;

	if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE,0,1,&sessions,&sessCnt)) {
		addLogMessage(L"WTSEnumerateSessions failed.\n");
		return -1;
	}
	DWORD i;
	const DWORD NO_SESSIONS = 0x8fffffff;
	DWORD sessionId = NO_SESSIONS;

	for (i=0;i<sessCnt;i++) {
		if (sessions->State == WTSActive) {
			HANDLE hUserToken;
			if (!WTSQueryUserToken(sessions->SessionId,&hUserToken)) {
				continue;
			}
			sessionId = sessions->SessionId;
			CloseHandle(hUserToken);
			break;
		}
		sessions++;
	}

	if (sessionId!=NO_SESSIONS) {
		HANDLE hSelfToken;
		if (!OpenProcessToken(GetCurrentProcess(),TOKEN_ALL_ACCESS_P | TOKEN_ADJUST_SESSIONID,&hSelfToken)) {
			addLogMessage(L"OpenProcessToken failed.\n");
			return -4;
		}

		HANDLE hDestToken;
		if (!DuplicateTokenEx(hSelfToken,MAXIMUM_ALLOWED,0,SecurityImpersonation,TokenPrimary,&hDestToken)) {
			addLogMessage(L"DuplicateTokenEx failed.\n");
			return -5;
		}

		CloseHandle(hSelfToken);

		if (!SetTokenInformation(hDestToken,TokenSessionId,&sessionId,sizeof(sessionId))) {
			addLogMessage(L"SetTokenInformation failed.\n");
			return -6;
		}

		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		memset(&si,0,sizeof(si));
		memset(&pi,0,sizeof(pi));

		si.cb = sizeof(si);
		si.lpDesktop = TEXT("WinSta0\\Default");

		if (!ImpersonateLoggedOnUser(hDestToken)) {
			addLogMessage(L"ImpersonateLoggedOnUser failed.\n");
			return -7;
		}

		wchar_t path[MAX_PATH] = { 0 };
		LPWSTR *ptr = NULL;
		if (!SearchPathW(NULL, L"cmd.exe", NULL, MAX_PATH, path, ptr)) {
			addLogMessage(L"SearchPath failed.\n");
			return -8;
		}

		if (!CreateProcessAsUserW(hDestToken,path,L"/K",NULL,NULL,FALSE,CREATE_NEW_CONSOLE,NULL,NULL,&si,&pi)) {
			addLogMessage(L"Failed to create process!\n");
			return -9;
		}
		CloseHandle(hDestToken);
	}
	return 0;
}
