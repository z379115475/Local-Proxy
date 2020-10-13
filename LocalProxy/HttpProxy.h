#pragma once
#include "DataStream.h"

#define MAX_HOSTNAME	256

#define GET_METHOD			1
#define HEAD_METHOD			2
#define POST_METHOD			3
#define CONNECT_METHOD		4
#define FTP_METHOD			5
#define OTHER_METHOD		0

typedef struct sockInfo{
	SOCKET *sockets;
	int ReqFlag;
}_sock_info;

class CHttpProxy
{
public:
	CHttpProxy();
	~CHttpProxy();

public:
	void StartWork();

private:
	BOOL CreateProxyServer(SOCKET &ProxyServer);
	static int ParseRequest(char *recvBuf, int &methodLen);
	static void GetHostAndPort(char *recvBuf, int datalen, char *servHostName, UINT &servPort);
	static BOOL RelayConnect(SOCKET *serSocket, char *servHostName, const UINT servPort);
	static BOOL SendRequestToServer(SOCKET* sockets, char *sendBuf, char *recvBuf, int dataLen, char *servHostName);
	static int EditRequest(char *sendBuf, char *recvBuf, int dataLen, int methodLen);
	static char *GetURLRootPoint(char * recvBuf, int dataLen, int *hostNameLen);
	static BOOL ParseFirstDataPacket(char *recvBuf, int &recvLen, Modifier *pCurModifier);
	static char *HostToIp(char *hostName);
	static UINT WINAPI ServClientDataExchangeTreadex(LPVOID lpParameter);
	static UINT WINAPI ProxyWorkTreadex(LPVOID lpParameter);

	

};

