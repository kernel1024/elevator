#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

int SetModulePath();
void addLogMessage(const wchar_t *msg, bool interactive = false);
void ServiceMain(int argc, char** argv);
int InstallElevService();
int RemoveElevService();
int StartElevService();
LPWSTR GetServiceName();
