#pragma once

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "39555"

int InitSocket();
void CleanupSocket();
void AcceptClientSocket();
