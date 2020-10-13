#include "stdafx.h"
#include "HttpProxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

using namespace std;
#pragma warning(disable:4996)
#pragma comment(lib,"ws2_32.lib")

#define FIND_KEY(str,strKey) (str.Find(strKey) >= 0)

#define HEADLEN					7 // "http://"  的长度
#define HTTPS_HEADLEN			8 // "https://" 的长度

#define DEFAULT_SERV_PORT	80
#define PROXY_PORT	8888
#define LISTEN_NUM	500
#define TIMEOUT			500
#define MAX_BUF_SIZE	8*1024

char connectedTip[] = "HTTP/1.0 200 Connection Established\r\n\r\n";
char connectedTipEnd[] = "";

CRITICAL_SECTION cs;
DWORD g_dwFirstReqTime = 0;

CHttpProxy::CHttpProxy()
{

}


CHttpProxy::~CHttpProxy()
{

}

void CHttpProxy::StartWork()
{
	SOCKET ProxyServer;
	if (!CreateProxyServer(ProxyServer)) {
		WSACleanup();
		return;
	}
	printf("HTTP代理启动成功!\n");

	InitializeCriticalSection(&cs);

	SOCKET ClientSocket = INVALID_SOCKET;
	SOCKET *sockets;
	
	while (TRUE) {
		Sleep(5);
		//接受客户端的连接请求
		ClientSocket = accept(ProxyServer, NULL, NULL);

		sockets = (SOCKET*)malloc(sizeof(SOCKET)* 2);
		if (ClientSocket == INVALID_SOCKET || !sockets) {
			continue;
		}
		sockets[0] = ClientSocket;
		_beginthreadex(NULL, NULL, ProxyWorkTreadex, sockets, 0, NULL);
	}

	WSACleanup();
	DeleteCriticalSection(&cs);
}

UINT WINAPI CHttpProxy::ServClientDataExchangeTreadex(LPVOID lpParameter)
{
	_sock_info *sockInfo = static_cast<_sock_info *>(lpParameter);
	
	Modifier *pCurModifier = new Modifier;
	pCurModifier->m_vKeyWordsList.clear();
	pCurModifier->m_vKeyWordsList.push_back(TEXT("京东"));
	pCurModifier->m_vKeyWordsList.push_back(TEXT("58同城"));
	pCurModifier->m_vKeyWordsList.push_back(TEXT("直播"));
	BOOL bCanInsertJS = FALSE;
	//printf("交换数据\n");
	int nReqFlag = sockInfo->ReqFlag;
	SOCKET cliSocket = sockInfo->sockets[0];
	SOCKET serSocket = sockInfo->sockets[1];
	struct timeval timeset;
	//读写集合
	char servHostName[MAX_HOSTNAME];
	fd_set readfd, writefd;
	int result;
	char sendBuf[MAX_BUF_SIZE];	//send to server
	char read_in1[MAX_BUF_SIZE], send_out1[MAX_BUF_SIZE]; //read from client , send to serv
	char recvBuf[MAX_BUF_SIZE]; //recv from serv to client
	int read1 = 0, totalread1 = 0, send1 = 0;
	int recvActualLen = 0, send2 = 0;
	int sendcount1, sendcount2;
	int maxfd;

	maxfd = max(cliSocket, serSocket) + 1;
	memset(servHostName, 0, MAX_HOSTNAME);
	memset(sendBuf, 0, MAX_BUF_SIZE);
	memset(read_in1, 0, MAX_BUF_SIZE);
	memset(recvBuf, 0, MAX_BUF_SIZE);
	memset(send_out1, 0, MAX_BUF_SIZE);

	timeset.tv_sec = 3;
	timeset.tv_usec = 0;
	if (nReqFlag == GET_METHOD)
		Sleep(500);
	while (TRUE)
	{
		FD_ZERO(&readfd);
		FD_ZERO(&writefd);

		//将两个套接字均加入到读写集合中
		FD_SET((UINT)cliSocket, &readfd);
		FD_SET((UINT)cliSocket, &writefd);
		FD_SET((UINT)serSocket, &writefd);
		FD_SET((UINT)serSocket, &readfd);

		result = select(maxfd, &readfd, &writefd, NULL, &timeset);
		if ((result < 0) && (errno != EINTR))
		{
			printf("Select error.\r\n");
			break;
		}
		else if (result == 0)
		{
			printf("Socket time out.\r\n");
			break;
		}

		//服务器是否有数据返回
		if (FD_ISSET(serSocket, &readfd))
		{
			if (pCurModifier->m_Status == INIT_STATUS)
			{
				recvActualLen = recv(serSocket, recvBuf, (MAX_BUF_SIZE - 512), 0);
				if (nReqFlag == GET_METHOD) {
					bCanInsertJS = ParseFirstDataPacket(recvBuf, recvActualLen, pCurModifier);
				}

				pCurModifier->m_Status = TRANS_STATUS;
			}
			else
			{
				if (recvActualLen == 0)
				{
					if (bCanInsertJS)
					{
						if (pCurModifier->m_In.IsEmpty())
						{
							recvActualLen = recv(serSocket, pCurModifier->m_In.m_Buf, MAX_BUF_SIZE_IN, 0);
							pCurModifier->m_In.m_Offset = 0;
							pCurModifier->m_In.m_ValidLen = recvActualLen;
							if (pCurModifier->m_In.m_isChunked && recvActualLen > 0) {
								pCurModifier->ReparseChunkData();
							}
						}

						if (!pCurModifier->m_Out.IsEmpty())
						{
							pCurModifier->m_Out.OutputData(recvBuf, MAX_BUF_SIZE, &recvActualLen);
						}
						else
						{
							if (pCurModifier->m_In.IsEmpty()) {
								memcpy(recvBuf, "\r\n0\r\n\r\n", 7);
								recvActualLen = 7;
								pCurModifier->m_Status = END_STATUS;
							}
							else
							{
								if (pCurModifier->StreamToStream(pCurModifier->m_In, pCurModifier->m_Out)) {
									pCurModifier->m_Out.OutputData(recvBuf, MAX_BUF_SIZE, &recvActualLen);
								}
							}
						}
						
					}
					else
					{
						recvActualLen = recv(serSocket, recvBuf, MAX_BUF_SIZE, 0);
						if (recvActualLen == 0) break;	//数据接收结束
						if ((recvActualLen < 0) && (errno != EINTR))	//发生错误，但不是信号中断
						{
							printf("Read serSocket data error,ReqFlag = %d.\n",nReqFlag);
							break;
						}
					}
				}
			}
		}

		//是否可以将数据返回给客户端
		if (FD_ISSET(cliSocket, &writefd))
		{
			if (recvActualLen > 0)
			{
				int nErr = 0;
				sendcount2 = 0;
				while (recvActualLen > 0)
				{
					send2 = send(cliSocket, recvBuf + sendcount2, recvActualLen, 0);
					if (send2 == 0) break;	//发送结束，但数据不一定全部发生完毕，需要下一次继续发送
					if ((send2 < 0) && (errno != EINTR))
					{
						printf("Send to cliSocket unknow error,ReqFlag = %d.\r\n", nReqFlag);
						nErr = 1;
						break;
					}
					if (send2 >= 0)
					{
						sendcount2 += send2;
						recvActualLen -= send2;
						if (recvActualLen == 0)
						{
							break;
						}
					}
				}
				if (nErr == 1) break;
				if (recvActualLen > 0)
				{
					if (sendcount2 > 0)
					{
						memcpy(recvBuf, recvBuf + sendcount2, recvActualLen);
						memset(recvBuf + recvActualLen, 0, MAX_BUF_SIZE - recvActualLen);
					}
				}
				else   //此次接收的数据全部发送完毕
				{
					if (pCurModifier->m_Status == END_STATUS)
					{
						break;
					}
					memset(recvBuf, 0, MAX_BUF_SIZE);
				}

			}
		}



		// 是否有请求从客户端到来
		if (FD_ISSET(cliSocket, &readfd))
		{
			if (totalread1 < MAX_BUF_SIZE)
			{
				read1 = recv(cliSocket, read_in1, MAX_BUF_SIZE - totalread1, 0);
				if ((read1 == SOCKET_ERROR) || (read1 == 0))
				{
					break;
				}

				memcpy(send_out1 + totalread1, read_in1, read1);

				totalread1 += read1;
				memset(read_in1, 0, MAX_BUF_SIZE);
			}
			// 转发
			/*SOCKET *sockets1 = (SOCKET*)malloc(sizeof(SOCKET)* 2);
			if (!sockets) {
				continue;
			}
			sockets1[0] = cliSocket;
			if (SendRequestToServer(sockets1, sendBuf, send_out1, totalread1, servHostName))
				totalread1 = 0;*/
		}

		//是否可以将数据发送给服务端
		if (FD_ISSET(serSocket, &writefd))
		{
			int err = 0;
			sendcount1 = 0;
			while (totalread1 > 0)
			{
				send1 = send(serSocket, send_out1 + sendcount1, totalread1, 0);
				if (send1 == 0)break;
				if ((send1 < 0) && (errno != EINTR))
				{
					err = 1;
					break;
				}

				if ((send1 < 0) && (errno == ENOSPC)) break;
				sendcount1 += send1;
				totalread1 -= send1;

			}

			if (err == 1) break;
			if ((totalread1 > 0) && (sendcount1 > 0))
			{
				memcpy(send_out1, send_out1 + sendcount1, totalread1);
				memset(send_out1 + totalread1, 0, MAX_BUF_SIZE - totalread1);
			}
			else
				memset(send_out1, 0, MAX_BUF_SIZE);
		}
		Sleep(10);
	}

	closesocket(sockInfo->sockets[0]);
	closesocket(sockInfo->sockets[1]);
	free(sockInfo->sockets);
	delete sockInfo;
	delete pCurModifier;
	return 0;
}

BOOL CHttpProxy::ParseFirstDataPacket(char *recvBuf, int &recvLen, Modifier *pCurModifier)
{
	if (memcmp(recvBuf, "HTTP/1.1 200 OK", 13) == 0) {
		CHAR* p = strstr(recvBuf, "\r\n\r\n");
		if (p == NULL) {
			return FALSE;
		}
		*p = '\0';
		CString strHead = recvBuf;
		*p = '\r';
		CString strVal;
		if (pCurModifier->GetHeadValue(strHead, "Content-Type:", strVal)) {
			if (strVal.Find("text/html") == -1) {
				return FALSE;
			}
		}
		printf("\r\n===========%d=========\r\n", strHead.GetLength());

		/*printf("\r\n===============1==============\r\n");
		printf(strHead);
		printf("\r\n===============1==============\r\n");

		DWORD dCurTime = GetTickCount();
		DWORD dTime = 0;
		EnterCriticalSection(&cs);
		dTime = dCurTime - g_dwFirstReqTime;
		if (dTime > 3000) {
			g_dwFirstReqTime = dCurTime;
		}
		LeaveCriticalSection(&cs);

		if (dTime <= 3000) {
			return FALSE;
		}

		printf("\r\n=============2================\r\n");
		printf(strHead);
		printf("\r\n=============2================\r\n");*/

		if (pCurModifier->GetHeadValue(strHead, "Content-Encoding:", strVal)) {
			if (strVal.Find("gzip") != -1) {
				pCurModifier->m_In.m_isGZip = TRUE;
			}
		}

		if (pCurModifier->GetHeadValue(strHead, "Transfer-Encoding:", strVal)) {
			if (strVal.Find("chunked") != -1) {
				pCurModifier->m_In.m_isChunked = TRUE;
			}
		}

		if (pCurModifier->m_In.m_isGZip) {
			// 将压缩的数据复制到输入流，调整到body的开始
			p += 4;

			pCurModifier->m_In.m_ValidLen = recvLen - (p - recvBuf);
			memcpy(pCurModifier->m_In.m_Buf, p, pCurModifier->m_In.m_ValidLen);

			if (pCurModifier->m_In.m_isChunked) {
				pCurModifier->ReparseChunkData();
			}

			// 解压第一个数据包
			LONG lInUsed;
			LONG lOutUsed;
			CUnGZip UnGZip;
			pCurModifier->m_UnGZip.xDecompress((BYTE*)pCurModifier->m_In.m_Buf, pCurModifier->m_In.m_ValidLen, (BYTE*)pCurModifier->m_Out.m_Buf, (pCurModifier->m_Out.m_BufLen - 512), &lInUsed, &lOutUsed);

			pCurModifier->m_In.m_ValidLen -= lInUsed;
			pCurModifier->m_Out.m_ValidLen = lOutUsed;

			if (!pCurModifier->CheckKeyWords())
			{
				return FALSE;
			}

			if (pCurModifier->GetHeadValue(strHead, "Cache-Control:", strVal)) {		//设置无缓存
				if (strVal != "max-age=0" && strVal != "no-cache") {
					strHead.Replace(strVal, "max-age=0");
				}
			}
			else {
				pCurModifier->AddHttpHead(strHead, "Cache-Control: max-age=0");
			}

			// 删除Content-Length:
			pCurModifier->DeleteHttpHead(strHead, "Content-Length:");
			pCurModifier->DeleteHttpHead(strHead, "Content-Encoding: gzip");
			// 添加Transfer-Encoding: chunked
			if (!pCurModifier->m_In.m_isChunked)
			{
				pCurModifier->AddHttpHead(strHead, "Transfer-Encoding: chunked");
			}

			// 复制Http头
			strHead += TEXT("\r\n");
			memcpy(recvBuf, strHead, strHead.GetLength());
			p = recvBuf + strHead.GetLength();

			//有可能一共就一个包，直接insert广告
			pCurModifier->InsertJS(pCurModifier->m_Out);

			int	lOut;
			pCurModifier->m_Out.OutputData(p, 8192 - (p - recvBuf), &lOut);
			recvLen = lOut + strHead.GetLength();

			return TRUE;
		}
		else {
			// 没有压缩，修改Content-Length:
			return FALSE;
		}
	}

	return FALSE;
}

UINT WINAPI CHttpProxy::ProxyWorkTreadex(LPVOID lpParameter)
{
	SOCKET *sockets = static_cast<SOCKET *>(lpParameter);

	char *servHostName = (char*)malloc(sizeof(char)*MAX_HOSTNAME);
	char *recvBuf = (char*)malloc(sizeof(char)*MAX_BUF_SIZE);
	char *sendBuf = (char*)malloc(sizeof(char)*MAX_BUF_SIZE);

	memset(servHostName, 0, MAX_HOSTNAME);
	memset(recvBuf, 0, MAX_BUF_SIZE);
	memset(sendBuf, 0, MAX_BUF_SIZE);

	int dataLen = 0;
	dataLen = recv(sockets[0], recvBuf, MAX_BUF_SIZE, 0);	//recv from client
	if (strstr(recvBuf, "GET http://www.jd.com/ HTTP/1.1") || strstr(recvBuf, "GET http://dl.58.com/ HTTP/1.1") ||
		strstr(recvBuf, "GET http://v.17173.com/show HTTP/1.1"))
	{
		printf("Insert JS at This Socket!\r\n");
	}
	if (dataLen == SOCKET_ERROR || dataLen == 0)
		goto exit;
	
	if (!SendRequestToServer(sockets, sendBuf, recvBuf, dataLen, servHostName)) {
		goto exit;
	}

exit:
	free(servHostName);
	free(sendBuf);
	free(recvBuf);
	return 0;
}

BOOL CHttpProxy::RelayConnect(SOCKET *serSocket, char *servHostName, const UINT servPort)
{
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(servPort);

	if (inet_addr(servHostName) != INADDR_NONE) {
		servaddr.sin_addr.s_addr = inet_addr(servHostName);
	} else {
		if (HostToIp(servHostName) != NULL)
			servaddr.sin_addr.s_addr = inet_addr(HostToIp(servHostName));
		else
			return FALSE;
	}

	*serSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (*serSocket == INVALID_SOCKET)
		return FALSE;
	UINT timeOut = TIMEOUT;

	if (0 == setsockopt(*serSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeOut, sizeof(int))) {
		if (connect(*serSocket, (const SOCKADDR *)&servaddr, sizeof(servaddr)) == SOCKET_ERROR) {
			return FALSE;
		}
	}

	return TRUE;
}

BOOL CHttpProxy::SendRequestToServer(SOCKET* sockets, char *sendBuf, char *recvBuf, int dataLen, char *servHostName)
{
	UINT servPort = 0;
	int ReqFlag = 0, methodLen = 0, sendLen = 0;
	ReqFlag = ParseRequest(recvBuf, methodLen);

	if (ReqFlag == OTHER_METHOD)
		return FALSE;

	if (ReqFlag == GET_METHOD || ReqFlag == HEAD_METHOD || ReqFlag == POST_METHOD) {
		sendLen = EditRequest(sendBuf, recvBuf, dataLen, methodLen);
		if (0 == sendLen)	return FALSE;

		GetHostAndPort(recvBuf + methodLen + HEADLEN, dataLen - methodLen - HEADLEN, servHostName, servPort);

		if (!RelayConnect(&sockets[1], servHostName, servPort))
			return FALSE;

		if (send(sockets[1], sendBuf, sendLen, 0) == SOCKET_ERROR)
			return FALSE;
	}
	else if (ReqFlag == CONNECT_METHOD) {
		GetHostAndPort(recvBuf + methodLen, dataLen - methodLen, servHostName, servPort);

		if (!RelayConnect(&sockets[1], servHostName, servPort))
			return FALSE;

		send(sockets[0], connectedTipEnd, strlen(connectedTipEnd), 0);
		Sleep(20);
		send(sockets[0], connectedTip, strlen(connectedTip), 0);
	}
	else if (ReqFlag == FTP_METHOD) {
		return FALSE;
	}

	_sock_info *sockInfo = new _sock_info;
	sockInfo->sockets = sockets;
	sockInfo->ReqFlag = ReqFlag;

	if (sockets[0] && sockets[1]) {
		_beginthreadex(NULL, NULL, ServClientDataExchangeTreadex, (LPVOID)sockInfo, 0, NULL);
	}

	

	return TRUE;
}

int CHttpProxy::EditRequest(char *sendBuf, char *recvBuf, int dataLen, int methodLen)
{
	strncpy(sendBuf, recvBuf, methodLen);

	int hedLen = 0;

	//确定是否为"http://"协议
	if (strncmp(recvBuf + methodLen, "http://", HEADLEN))
		return 0;


	char * getRootfp = GetURLRootPoint(recvBuf + methodLen + HEADLEN, dataLen - methodLen - HEADLEN, &hedLen);
	if (getRootfp == NULL)
		return 0;

	memcpy(sendBuf + methodLen, getRootfp, dataLen - methodLen - HEADLEN - hedLen);

	return dataLen - HEADLEN - hedLen;
}

char * CHttpProxy::GetURLRootPoint(char * recvBuf, int dataLen, int *hostNameLen)
{
	for (int i = 0; i < dataLen; i++)
	{
		if (recvBuf[i] == '/')
		{
			*hostNameLen = i;
			return &recvBuf[i];
		}
	}
	return NULL;
}

//域名解析
char *CHttpProxy::HostToIp(char *hostName)
{
	HOSTENT *hostent = NULL;
	IN_ADDR iaddr;
	hostent = gethostbyname(hostName);
	if (hostent == NULL)
	{
		return NULL;
	}
	iaddr = *((LPIN_ADDR)*hostent->h_addr_list);
	return inet_ntoa(iaddr);
}

int CHttpProxy::ParseRequest(char *recvBuf, int &methodLen)
{
	if (!_strnicmp(recvBuf, "GET ", 4) && _strnicmp(recvBuf + 4, "ftp:", 4))
	{
		methodLen = 4;
		return GET_METHOD;
	}
	else if (!_strnicmp(recvBuf, "HEAD ", 5))
	{
		methodLen = 5;
		return HEAD_METHOD;
	}
	else if (!_strnicmp(recvBuf, "POST ", 5))
	{
		methodLen = 5;
		return POST_METHOD;
	}
	else if (!_strnicmp(recvBuf, "CONNECT ", 8))
	{
		methodLen = 8;
		return CONNECT_METHOD;
	}
	if (!_strnicmp(recvBuf, "GET ", 4) && !_strnicmp(recvBuf + 4, "ftp:", 4))
	{
		methodLen = 4;
		return FTP_METHOD;
	}
	else
	{
		return OTHER_METHOD;
	}
}

void CHttpProxy::GetHostAndPort(char *recvBuf, int datalen, char *servHostName, UINT &servPort)
{
	memset(servHostName, 0, MAX_HOSTNAME);
	char *headBuf = recvBuf;
	int i = 0;

	for (; i < datalen && *headBuf != ':' && *headBuf != '\0' && *headBuf != '\r' && *headBuf != '/'; i++)
	{
		servHostName[i] = *headBuf++;
		if (*headBuf == ':')
			servPort = atoi(headBuf + 1);
		else servPort = DEFAULT_SERV_PORT;
	}
}

BOOL CHttpProxy::CreateProxyServer(SOCKET &ProxyServer)
{
	//套接字准备:WSAStartup,socket,bind,listen
	WSADATA WSAData;
	if (WSAStartup(MAKEWORD(2, 2), &WSAData)) {
		return FALSE;
	}

	//创建套接字
	ProxyServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ProxyServer == SOCKET_ERROR) {
		return FALSE;
	}


	struct sockaddr_in servaddr = { 0 };
	unsigned int nProxyPort = PROXY_PORT;

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(nProxyPort);
	servaddr.sin_addr.S_un.S_addr = INADDR_ANY;

	int tryCount = 0;
	while (::bind(ProxyServer, (LPSOCKADDR)&servaddr, sizeof(servaddr)) == SOCKET_ERROR) {
		tryCount++;
		nProxyPort++;
		RtlZeroMemory(&servaddr, sizeof(sockaddr_in));

		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(nProxyPort);
		servaddr.sin_addr.S_un.S_addr = INADDR_ANY;

		Sleep(20);

		//printf("端口绑定失败，%d\n", nProxyPort);
		if (tryCount >= 500) {
			return FALSE;
		}
	}


	if (listen(ProxyServer, LISTEN_NUM) == SOCKET_ERROR) {
		return FALSE;
	}

	return TRUE;
}