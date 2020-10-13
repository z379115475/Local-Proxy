// md5.h
#include "stdafx.h"
#include "Utility.h"
#include <tchar.h>
#include <wincrypt.h>

// ����Hash���ɹ�����0��ʧ�ܷ���GetLastError()
//  CONST BYTE *pbData,   // �������� 
//  DWORD dwDataLen,      // ���������ֽڳ��� 
//  ALG_ID algId          // Hash �㷨��CALG_MD5,CALG_SHA
//  LPTSTR pszHash,       // ���16����Hash�ַ�����MD5����Ϊ32+1, SHA����Ϊ40+1
// 

DWORD GetHash(CONST BYTE *pbData, DWORD dwDataLen, ALG_ID algId, LPTSTR pszHash)
{
	DWORD dwReturn = 0;
	HCRYPTPROV hProv;
	if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
		return (dwReturn = GetLastError());

	HCRYPTHASH hHash;
	//Alg Id:CALG_MD5,CALG_SHA
	if (!CryptCreateHash(hProv, algId, 0, 0, &hHash))
	{
		dwReturn = GetLastError();
		CryptReleaseContext(hProv, 0);
		return dwReturn;
	}

	if (!CryptHashData(hHash, pbData, dwDataLen, 0)) {
		dwReturn = GetLastError();
		CryptDestroyHash(hHash);
		CryptReleaseContext(hProv, 0);
		return dwReturn;
	}

	DWORD dwSize;
	DWORD dwLen = sizeof(dwSize);
	CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)(&dwSize), &dwLen, 0);

	BYTE* pHash = new BYTE[dwSize];
	dwLen = dwSize;
	CryptGetHashParam(hHash, HP_HASHVAL, pHash, &dwLen, 0);

	lstrcpy(pszHash, _T(""));
	TCHAR szTemp[3];
	for (DWORD i = 0; i < dwLen; ++i)
	{
		//wsprintf(szTemp, _T("%X%X"), pHash[i] >> 4, pHash[i] & 0xf);
		wsprintf(szTemp, _T("%02X"), pHash[i]);
		lstrcat(pszHash, szTemp);
	}
	delete[]pHash;

	CryptDestroyHash(hHash);
	CryptReleaseContext(hProv, 0);

	return dwReturn;
}

BOOL GetFileMd5(LPCTSTR lpFileName, LPTSTR pszHash)
{
	HANDLE hFile = CreateFile(lpFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return FALSE;
	}
	CHandleRelease resh(hFile);
	DWORD dwFileSize = GetFileSize(hFile, 0);    //��ȡ�ļ��Ĵ�С
	if (dwFileSize == 0xFFFFFFFF){
		return FALSE;
	}

	BYTE* lpReadFileBuffer = new BYTE[dwFileSize];
	CObjRelease<BYTE> resb(lpReadFileBuffer);
	DWORD lpReadNumberOfBytes;
	if (ReadFile(hFile, lpReadFileBuffer, dwFileSize, &lpReadNumberOfBytes, NULL) == 0){
		return FALSE;
	}

	if (GetHash(lpReadFileBuffer, dwFileSize, CALG_MD5, pszHash)) {
		return FALSE;
	}

	return TRUE;
}


BOOL GetStringMd5(TCHAR* pszStr, LPTSTR pszHash)
{
	if (GetHash((BYTE*)pszStr, _tcslen(pszStr), CALG_MD5, pszHash)) {
		return FALSE;
	}

	return TRUE;
}