// BufferCenter.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CBufferCenterApp:
// �йش����ʵ�֣������ BufferCenter.cpp
//

class CBufferCenterApp : public CWinApp
{
public:
	CBufferCenterApp();

// ��д
	public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern CBufferCenterApp theApp;