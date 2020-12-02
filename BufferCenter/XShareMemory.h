// XShareMemory.h: interface for the XShareMemory class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_XSHAREMEMORY_H__32BEA564_49E7_4756_994E_AFC067505D25__INCLUDED_)
#define AFX_XSHAREMEMORY_H__32BEA564_49E7_4756_994E_AFC067505D25__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define	DEFAULT_FILENAME	NULL
#define	DEFAULT_MAPNAME		"_SFM_OBJ_"
#define	DEFAULT_MAPSIZE		(0xFFFF + 1)
#define MUTEX_NAME "HD_MW_MUTEX"
#define MSG_SIZE 46

// ���̼乲���ڴ�

class XShareMemory  
{
public:
	bool Read(char *pData, DWORD dwSize, DWORD index=0);
//	bool Write(const char *pData, DWORD dwSize);
	bool Append(const char *pData, DWORD dwSize);
	bool Fetch(char *pData, DWORD dwMsgCount);
	DWORD GetMsgCount(); // (buffer size) / (msg size)
	DWORD GetBufferSize();
	DWORD GetFetchedMsgCount();

	XShareMemory();
	XShareMemory(char *szFileName, char *szMapName, DWORD dwSize);
	virtual ~XShareMemory();

	void Create(char *szFileName, char *szMapName, DWORD dwSize);	// ����ˣ����������ڴ�
	void Open(DWORD dwAccess, char *szMapName);		// �ͻ��ˣ��򿪹����ڴ�
	LPVOID GetBuffer();
	DWORD GetSize();
	bool IsOpened();
	bool IsCreated();
private:
	void Destory();
	void Init();
protected:
	HANDLE	m_hFile;
	HANDLE	m_hFileMap;
	HANDLE  m_hCreateMutex;		// ���Ᵽ�������ڴ�
	HANDLE  m_hOpenMutex;		// ���Ᵽ�������ڴ�
	LPVOID	m_lpFileMapBuffer;

	char	*m_pFileName;
	char	*m_pMapName;
	DWORD	m_dwSize;

	int		m_iCreateFlag;		// ������־����������
	int		m_iOpenFlag;		// �򿪱�־���ͻ���
};

#endif // !defined(AFX_XSHAREMEMORY_H__32BEA564_49E7_4756_994E_AFC067505D25__INCLUDED_)
