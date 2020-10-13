// LocalProxy.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "HttpProxy.h"

//#pragma comment(linker,"/subsystem:\"Windows\" /entry:\"mainCRTStartup\"")

LPBYTE CString_To_LPBYTE(CString str)
{
	LPBYTE lpb = new BYTE[str.GetLength() + 1];
	for (int i = 0; i<str.GetLength(); i ++)
		lpb[i] = str[i];
	lpb[str.GetLength()] = 0;
	return lpb;
}

BOOL SetRegeditKey(const DWORD dwProxyEnable)
{
	HKEY hKey;
	LPCTSTR data_Set = "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings\\";
	long ret = ::RegOpenKeyEx(HKEY_CURRENT_USER, data_Set, 0, KEY_WRITE, &hKey);
	if (ret != ERROR_SUCCESS) {
		return FALSE;
	}

	DWORD dwType = REG_DWORD;
	ret = ::RegSetValueEx(hKey, "ProxyEnable", NULL, dwType, (LPBYTE)&dwProxyEnable, sizeof(DWORD));
	if (ret != ERROR_SUCCESS) {
		return FALSE;
	}

	if (dwProxyEnable == 1) {
		CString strProxyServ = TEXT("127.0.0.1:8888");
		LPBYTE proxyServSet = CString_To_LPBYTE(strProxyServ);
		dwType = REG_SZ;
		DWORD cbData = strProxyServ.GetLength() + 1;
		ret = ::RegSetValueEx(hKey, "ProxyServer", NULL, dwType, proxyServSet, cbData);
		if (ret != ERROR_SUCCESS) {
			return FALSE;
		}
	}

	::RegCloseKey(hKey);

	return TRUE;
}

BOOL WINAPI ConsoleHandler(DWORD msgType)
{
	if (msgType == CTRL_CLOSE_EVENT)
	{
		SetRegeditKey(0);
		
		return TRUE;
	}

	return FALSE;
}

BOOL Init()
{
	if (!SetRegeditKey(1))	return FALSE;

	SetConsoleCtrlHandler(ConsoleHandler, TRUE);

	CUnGZip::Init();

	return TRUE;
}

int _tmain(int argc, _TCHAR* argv[])
{
	if (!Init())	return 0;;

	CHttpProxy httpProxy;
	httpProxy.StartWork();

	getchar();
	return 0;
}

