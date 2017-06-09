#pragma once

#include <Windows.h>

BOOL CheckCredentials();
BOOL AdjustPrivilege(LPCWSTR privilege, BOOL state);
int SecurityWork();
