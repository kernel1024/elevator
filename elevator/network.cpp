#include "stdafx.h"
#include "network.h"
#include "service.h"
#include "security.h"

#include <winsock2.h>
#include <WS2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")

WSADATA wsaData;
SOCKET ListenSocket = INVALID_SOCKET;

DWORD WINAPI NetworkWorkerThread (LPVOID lpParam)
{
	int iResult,iSendResult;
	SOCKET ClientSocket = (SOCKET)lpParam;
	char recvbuf[DEFAULT_BUFLEN];
	char cmd[DEFAULT_BUFLEN*4];
	int recvbuflen = DEFAULT_BUFLEN;
	const char bingo[] = "bingo!";

	memset(cmd,0,sizeof(cmd));
	// Receive until the peer shuts down the connection
	do {
		memset(recvbuf,0,sizeof(recvbuf));
		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			char fb = recvbuf[0];
			if (((fb>=0x20) && (fb<=0x7f)) || (fb==0xd) || (fb==0xa)) {
				strncat_s(cmd,sizeof(cmd),recvbuf,iResult);
				// Echo the buffer back to the sender
				iSendResult = send( ClientSocket, recvbuf, iResult, 0 );
				if (iSendResult == SOCKET_ERROR) {
					closesocket(ClientSocket);
					return 1;
				}
				if (strchr(cmd,'\r')!=NULL) break;
				if (strchr(cmd,'\n')!=NULL) break;
			}
		}
		else if (iResult == 0) {
			// Connection closing...
			closesocket(ClientSocket);
			return 1;
		} else {
			// recv failed with error
			closesocket(ClientSocket);
			return 1;
		}
		if (strncmp(cmd,bingo,strlen(bingo)) == 0) {
			SecurityWork();
			closesocket(ClientSocket);
			break;
		}
	} while (iResult > 0);

	// shutdown the connection since we're done
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		closesocket(ClientSocket);
		return 1;
	}
	closesocket(ClientSocket);
	return 0;
}

int InitSocket() {
	int iResult;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) {
		addLogMessage(L"WSAStartup failed with error.\n");
		return -1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo("localhost", DEFAULT_PORT, &hints, &result);
	if ( iResult != 0 ) {
		addLogMessage(L"getaddrinfo failed with error.\n");
		WSACleanup();
		return -2;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		addLogMessage(L"socket failed with error.\n");
		freeaddrinfo(result);
		WSACleanup();
		return -3;
	}

	// Set the mode of the socket to be nonblocking
/*	u_long iMode = 1;
	iResult = ioctlsocket(ListenSocket, FIONBIO, &iMode);

	if (iResult == SOCKET_ERROR) {
		addLogMessage(L"ioctlsocket failed with error.\n");
		closesocket(ListenSocket);
		WSACleanup();
		return -4;
	}*/


	// Setup the TCP listening socket
	iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		addLogMessage(L"bind failed with error.\n");
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return -5;
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		addLogMessage(L"listen failed with error.\n");
		closesocket(ListenSocket);
		WSACleanup();
		return -6;
	}
	return 0;
}

void CleanupSocket() {
	closesocket(ListenSocket);
	WSACleanup();
}

void AcceptClientSocket() {
	// Accept a client socket
	SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
	if (ClientSocket != INVALID_SOCKET) {
		//disable nagle on the client's socket
		char value = 1;
		setsockopt( ClientSocket, IPPROTO_TCP, TCP_NODELAY, &value, sizeof( value ) );

		HANDLE hThread = CreateThread( NULL, 0, NetworkWorkerThread,(LPVOID)ClientSocket, 0, NULL);
	}
}