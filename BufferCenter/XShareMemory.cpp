// XShareMemory.cpp: implementation of the XShareMemory class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "XShareMemory.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//缓冲区的前100个字节做为缓存区信息
/*
{
DWORD 缓存区大小
DWORD 上位机取走的条数
}
*/
#define BUF_OFFSET 100

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

XShareMemory::XShareMemory()
{
	Init();
//	Create(DEFAULT_FILENAME, DEFAULT_MAPNAME, DEFAULT_MAPSIZE);
}

XShareMemory::XShareMemory(char *szFileName, char *szMapName, DWORD dwSize)
{
	Init();
	Create(szFileName, szMapName, dwSize);
}

XShareMemory::~XShareMemory()
{
	Destory();
}

void XShareMemory::Init()
{
	m_hFile = NULL;
	m_hFileMap = NULL;
	m_hCreateMutex = NULL;
	m_hOpenMutex = NULL;
	m_lpFileMapBuffer = NULL;

	m_pFileName = NULL;
	m_pMapName = NULL;
	m_dwSize = 0;

	m_iCreateFlag = 0;
	m_iOpenFlag = 0;
}

void XShareMemory::Destory()
{
	if (m_lpFileMapBuffer)
	{
		UnmapViewOfFile(m_lpFileMapBuffer);
		m_lpFileMapBuffer = NULL;
	}
	
	if (m_hFileMap)
	{
		CloseHandle(m_hFileMap);
		m_hFileMap = NULL;
	}
	
	if (m_hFile && m_hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hFile);
		m_hFile = NULL;
	}

	if(m_hCreateMutex)
	{
		CloseHandle(m_hCreateMutex);
		m_hCreateMutex = NULL;
	}
	
	if(m_hOpenMutex)
	{
		CloseHandle(m_hOpenMutex);
		m_hOpenMutex = NULL;
	}

	if (m_pFileName)
	{
		free(m_pFileName);
		m_pFileName = NULL;
	}

	if (m_pMapName)
	{
		free(m_pMapName);
		m_pMapName = NULL;
	}

	Init();
}

void XShareMemory::Create(char *szFileName, char *szMapName, DWORD dwSize)
{
	ASSERT(m_iOpenFlag == 0);

	if (m_iCreateFlag)
		Destory();

	char szMutexName[1000];
	strcpy(szMutexName, szMapName);
	strcat(szMutexName, "_MUTEX");

	m_hCreateMutex = CreateMutex(NULL, FALSE, szMutexName);
	
	if (szFileName)
		m_pFileName = _strdup(szFileName);

	if (szMapName)
		m_pMapName = _strdup(szMapName);
	else m_pMapName = _strdup(DEFAULT_MAPNAME);

	if (dwSize > 0)
		m_dwSize = dwSize;
	else m_dwSize = DEFAULT_MAPSIZE;

	if (m_pFileName)
	{
		// file
		m_hFile = CreateFile(
			m_pFileName,
			GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ|FILE_SHARE_WRITE,
			NULL,
			OPEN_ALWAYS,//OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
			);
	}
	else
	{
		// system
		m_hFile = (HANDLE)0xFFFFFFFF;
	}

	if (m_hFile)
	{
		m_hFileMap = CreateFileMapping(
			m_hFile,
			NULL,
			PAGE_READWRITE,
			0,
			m_dwSize,
			m_pMapName
			);

		//使只有一个CSFMServer对象能操作内存对象
		//if (m_hFileMap != NULL && ERROR_ALREADY_EXISTS == GetLastError())
		//{
		//	CloseHandle(m_hFileMap);
		//	m_hFileMap = NULL;
		//}
	}

	if (m_hFileMap)
	{
		m_lpFileMapBuffer = MapViewOfFile(
			m_hFileMap,
			FILE_MAP_ALL_ACCESS,//FILE_MAP_WRITE|FILE_MAP_READ,
			0,
			0,
			m_dwSize
			);
	}
	//Zero config zone
	if(m_lpFileMapBuffer)
		ZeroMemory(m_lpFileMapBuffer, dwSize);

	m_iCreateFlag = 1;
}

bool XShareMemory::IsOpened()
{
	return (m_iOpenFlag == 1)? true : false;
}

bool XShareMemory::IsCreated()
{
	return (m_iCreateFlag == 1)? true : false;
}

void XShareMemory::Open(DWORD dwAccess, char *szMapName)
{
	ASSERT(m_iCreateFlag == 0);
	
	if (m_iOpenFlag)
		Destory();

	char szMutexName[1000];
	strcpy(szMutexName, szMapName);
	strcat(szMutexName, "_MUTEX");

	m_hOpenMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, szMutexName);
	if(m_hOpenMutex == NULL)
		return;

	if (szMapName)
		m_pMapName = _strdup(szMapName);
	else m_pMapName = _strdup(DEFAULT_MAPNAME);

	m_hFileMap = OpenFileMapping(
		dwAccess,
		TRUE,
		m_pMapName
		);

	if (m_hFileMap)
	{
		m_lpFileMapBuffer = MapViewOfFile(
			m_hFileMap,
			dwAccess,
			0,
			0,
			0
			);

		m_iOpenFlag = 1;
	}
}

LPVOID XShareMemory::GetBuffer()
{
	return (m_lpFileMapBuffer)?(m_lpFileMapBuffer):(NULL);
}

DWORD XShareMemory::GetSize()
{
	return m_dwSize;
}

bool XShareMemory::Read(char *pData, DWORD dwSize, DWORD index)
{
	char *p = (char*)GetBuffer();
	if(!p)
		return false;
	
	HANDLE hMutex;
	if(m_iCreateFlag)
		hMutex = m_hCreateMutex;
	else
		hMutex = m_hOpenMutex;

	::WaitForSingleObject(hMutex, INFINITE);
	memcpy(pData, p+index, dwSize);
	ReleaseMutex(hMutex);

	return true;
}
//添加数据到缓存尾
bool XShareMemory::Append( const char *pData, DWORD dwSize )
{
	char *p = (char*)GetBuffer();
	if(p)
	{
		HANDLE hMutex;
		if(m_iCreateFlag)
			hMutex = m_hCreateMutex;
		else
			hMutex = m_hOpenMutex;
		::WaitForSingleObject(hMutex, INFINITE);
		DWORD bufSize = GetBufferSize();
		//超出缓存区大小
		if(dwSize+bufSize>m_dwSize)
		{
			ReleaseMutex(hMutex);
			return false;
		}
		else
		{
			memcpy(p+BUF_OFFSET+bufSize, pData, dwSize);
			bufSize += dwSize;
			memcpy(p,(char*)(&bufSize),4); //更新缓存区实际数据字节大小到缓存区前4字节
			ReleaseMutex(hMutex);
		}
		return true;
	}
	else
		return false;
}
//从缓存中取出dwMsgCount个message,并[擦除]取出的数据
bool XShareMemory::Fetch( char *pData, DWORD dwMsgCount )
{
	HANDLE hMutex;
	if(m_iCreateFlag)
		hMutex = m_hCreateMutex;
	else
		hMutex = m_hOpenMutex;

	::WaitForSingleObject(hMutex, INFINITE);
	char *p = (char*)GetBuffer();
	if(!p)
	{
		ReleaseMutex(hMutex);
		return false;
	}
	DWORD bufSize = GetBufferSize();
	//读取越界
	if(dwMsgCount>(bufSize/MSG_SIZE))
	{
		ReleaseMutex(hMutex);
		return false;
	}

	//取值
	memcpy(pData, p+BUF_OFFSET, dwMsgCount*MSG_SIZE);
	//擦除
	DWORD bufSizeAfterFetch = bufSize-dwMsgCount*MSG_SIZE;
	ASSERT(bufSizeAfterFetch>=0);
	memcpy(p+BUF_OFFSET, p+BUF_OFFSET+dwMsgCount*MSG_SIZE, bufSizeAfterFetch); //理论上不存在重叠区域

	DWORD *pMsgCount = (DWORD*)p;
	*pMsgCount = bufSizeAfterFetch;	//更新缓存区实际数据字节大小到缓存区前4字节
	//memcpy(p,(char*)(&bufSizeAfterFetch),sizeof(DWORD)); 

	DWORD *pFetchedMsgCount = (DWORD*)(p+sizeof(DWORD));
	*pFetchedMsgCount += dwMsgCount; //更新上位机取走的数据字节大小到缓存区4~7字节
	ReleaseMutex(hMutex);

	return true;
}
//获取可读的message count
DWORD XShareMemory::GetMsgCount()
{
	return GetBufferSize()/MSG_SIZE;
}

DWORD XShareMemory::GetBufferSize()
{
	DWORD bufSize;
	Read((char*)&bufSize,sizeof(DWORD));
	return bufSize;
}

DWORD XShareMemory::GetFetchedMsgCount()
{
	DWORD bufSize;
	Read((char*)&bufSize,sizeof(DWORD),sizeof(DWORD));
	return bufSize;
}
