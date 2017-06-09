#include "stdafx.h"
#include "service.h"
#include "network.h"

void addLogMessage(const wchar_t *msg, bool interactive) {
	if (interactive) {
		MessageBoxW(0,msg,L"Elevator service",MB_ICONWARNING|MB_OK);
		return;
	}

#ifdef _DEBUG
	HANDLE hEventSource = NULL;
	LPCWSTR lpszStrings[3] = { NULL, NULL, NULL };
	wchar_t msgBuffer[2048];

	hEventSource = RegisterEventSourceW(NULL, GetServiceName());
	if (hEventSource)
	{
		lpszStrings[0] = GetServiceName();
		lpszStrings[1] = msg;
		DWORD err = GetLastError();
		if (err!=0) {
			FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				0,
				err,
				MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
				msgBuffer,
				sizeof(msgBuffer),
				NULL);
			lpszStrings[2] = msgBuffer;
		}

		ReportEvent(hEventSource,  // Event log handle
			EVENTLOG_ERROR_TYPE,   // Event type
			0,                     // Event category
			0,                     // Event identifier
			NULL,                  // No security identifier
			3,                     // Size of lpszStrings array
			0,                     // No binary data
			lpszStrings,           // Array of strings
			NULL                   // No binary data
			);

		DeregisterEventSource(hEventSource);
	}
#endif
}

SERVICE_STATUS serviceStatus;
SERVICE_STATUS_HANDLE serviceStatusHandle;
const wchar_t serviceName[] = L"ElevatorSvc";
const wchar_t serviceDisplay[] = L"Elevator system service";
wchar_t modulePath[MAX_PATH];
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;

LPWSTR GetServiceName() {
	return (LPWSTR)serviceName;
}

int SetModulePath() {
	if (GetModuleFileNameW(NULL, modulePath, sizeof(modulePath)) == 0)
	{
		addLogMessage(L"GetModuleFileName failed.\n");
		return -1;
	}
	return 0;
}

int InstallElevService() {
	SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if(!hSCManager) {
		addLogMessage(L"Error: Can't open Service Control Manager",true);
		return -1;
	}

	SC_HANDLE hService = CreateServiceW(
		hSCManager,
		serviceName,
		serviceDisplay,
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START,
		SERVICE_ERROR_NORMAL,
		modulePath,
		NULL, NULL, NULL, NULL, NULL
		);

	if(!hService) {
		int err = GetLastError();
		switch(err) {
		case ERROR_ACCESS_DENIED:
			addLogMessage(L"Error: ERROR_ACCESS_DENIED",true);
			break;
		case ERROR_CIRCULAR_DEPENDENCY:
			addLogMessage(L"Error: ERROR_CIRCULAR_DEPENDENCY",true);
			break;
		case ERROR_DUPLICATE_SERVICE_NAME:
			addLogMessage(L"Error: ERROR_DUPLICATE_SERVICE_NAME",true);
			break;
		case ERROR_INVALID_HANDLE:
			addLogMessage(L"Error: ERROR_INVALID_HANDLE",true);
			break;
		case ERROR_INVALID_NAME:
			addLogMessage(L"Error: ERROR_INVALID_NAME",true);
			break;
		case ERROR_INVALID_PARAMETER:
			addLogMessage(L"Error: ERROR_INVALID_PARAMETER",true);
			break;
		case ERROR_INVALID_SERVICE_ACCOUNT:
			addLogMessage(L"Error: ERROR_INVALID_SERVICE_ACCOUNT",true);
			break;
		case ERROR_SERVICE_EXISTS:
			addLogMessage(L"Error: ERROR_SERVICE_EXISTS",true);
			break;
		default:
			addLogMessage(L"Error: Undefined",true);
		}
		CloseServiceHandle(hSCManager);
		return -1;
	}
	CloseServiceHandle(hService);

	CloseServiceHandle(hSCManager);
	addLogMessage(L"Success install service!",true);
	return 0;
}

int RemoveElevService() {
	SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(!hSCManager) {
		addLogMessage(L"Error: Can't open Service Control Manager",true);
		return -1;
	}
	SC_HANDLE hService = OpenService(hSCManager, serviceName, SERVICE_STOP | DELETE);
	if(!hService) {
		addLogMessage(L"Error: Can't remove service",true);
		CloseServiceHandle(hSCManager);
		return -1;
	}

	DeleteService(hService);
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCManager);
	addLogMessage(L"Success remove service!",true);
	return 0;
}

int StartElevService() {
	SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	SC_HANDLE hService = OpenService(hSCManager, serviceName, SERVICE_START);
	if(!StartService(hService, 0, NULL)) {
		CloseServiceHandle(hSCManager);
		addLogMessage(L"Error: Can't start service");
		return -1;
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCManager);
	return 0;
}

void ControlHandler(DWORD request) {
	switch(request)
	{
	case SERVICE_CONTROL_STOP:
		serviceStatus.dwWin32ExitCode = 0;
		serviceStatus.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus (serviceStatusHandle, &serviceStatus);
		return;

	case SERVICE_CONTROL_SHUTDOWN:
		serviceStatus.dwWin32ExitCode = 0;
		serviceStatus.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus (serviceStatusHandle, &serviceStatus);
		return;

	default:
		break;
	}

	SetServiceStatus (serviceStatusHandle, &serviceStatus);

	return;
} 

DWORD WINAPI ServiceWorkerThread (LPVOID lpParam)
{
	//  Periodically check if the service has been requested to stop
	while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
		AcceptClientSocket();
		Sleep(500);
	}


	return 0;
}

void ServiceMain(int argc, char** argv) {
	int error;
	int i = 0;

	serviceStatus.dwServiceType    = SERVICE_WIN32_OWN_PROCESS;
	serviceStatus.dwCurrentState    = SERVICE_START_PENDING;
	serviceStatus.dwControlsAccepted  = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	serviceStatus.dwWin32ExitCode   = 0;
	serviceStatus.dwServiceSpecificExitCode = 0;
	serviceStatus.dwCheckPoint     = 0;
	serviceStatus.dwWaitHint      = 0;

	serviceStatusHandle = RegisterServiceCtrlHandler(serviceName, (LPHANDLER_FUNCTION)ControlHandler);
	if (serviceStatusHandle == (SERVICE_STATUS_HANDLE)0) {
		return;
	} 

	error = InitSocket();
	if (error) {
		serviceStatus.dwCurrentState    = SERVICE_STOPPED;
		serviceStatus.dwWin32ExitCode   = -1;
		SetServiceStatus(serviceStatusHandle, &serviceStatus);
		return;
	}

	g_ServiceStopEvent = CreateEvent (NULL, TRUE, FALSE, NULL);
	if (g_ServiceStopEvent == NULL) 
	{   
		// Error creating event
		// Tell service controller we are stopped and exit
		serviceStatus.dwControlsAccepted = 0;
		serviceStatus.dwCurrentState = SERVICE_STOPPED;
		serviceStatus.dwWin32ExitCode = GetLastError();
		serviceStatus.dwCheckPoint = 1;
		SetServiceStatus (serviceStatusHandle, &serviceStatus);
		return;
	}

	serviceStatus.dwCurrentState = SERVICE_RUNNING;
	serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	serviceStatus.dwWin32ExitCode = 0;
	serviceStatus.dwCheckPoint = 0;
	SetServiceStatus (serviceStatusHandle, &serviceStatus);

	HANDLE hThread = CreateThread (NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

	WaitForSingleObject (hThread, INFINITE);

	//cleanup
	CloseHandle (g_ServiceStopEvent);

	CleanupSocket();

	serviceStatus.dwControlsAccepted = 0;
	serviceStatus.dwCurrentState = SERVICE_STOPPED;
	serviceStatus.dwWin32ExitCode = 0;
	serviceStatus.dwCheckPoint = 3;

	SetServiceStatus (serviceStatusHandle, &serviceStatus);
	return;
}
