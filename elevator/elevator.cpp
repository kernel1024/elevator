#include "stdafx.h"

#include "network.h"
#include "service.h"
#include "security.h"

int _tmain(int argc, _TCHAR* argv[])
{
	if (SetModulePath()<0) return -1;

	if((argc - 1 == 0) && CheckCredentials()) {
		SERVICE_TABLE_ENTRY ServiceTable[2];
		memset(ServiceTable,0,sizeof(ServiceTable));
		ServiceTable[0].lpServiceName = GetServiceName();
		ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;

		if(!StartServiceCtrlDispatcher(ServiceTable)) {
			addLogMessage(L"Error: StartServiceCtrlDispatcher",true);
		}
	} else if( wcscmp(argv[argc-1], _T("install")) == 0) {
		InstallElevService();
	} else if( wcscmp(argv[argc-1], _T("remove")) == 0) {
		RemoveElevService();
	} else if( wcscmp(argv[argc-1], _T("start")) == 0 ){
		StartElevService();
	} else {
		MessageBoxW(0,L"Elevator system service. \n"
			L"Usage: elevator.exe [install|remove|start] for serivce (de)installation.",L"Elevator",MB_ICONASTERISK|MB_OK);
	}
	return 0;
}

