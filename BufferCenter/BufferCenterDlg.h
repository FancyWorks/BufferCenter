// BufferCenterDlg.h : ͷ�ļ�
//

#pragma once

#include "KConnection.h"
#include <vector>
#include "afxcmn.h"
#include "XShareMemory.h"
using namespace std;

// CBufferCenterDlg �Ի���
class CBufferCenterDlg : public CDialog
{
// ����
public:
	CBufferCenterDlg(CWnd* pParent = NULL);	// ��׼���캯��
	~CBufferCenterDlg();
// �Ի�������
	enum { IDD = IDD_BUFFERCENTER_DIALOG };
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnOK(){return;}
protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��
	vector<KConnection*> connArray;
	DWORD m_dwMsgCount;
	CString m_szRunTime;
	DWORD m_dwRunTimeSecond;
	XShareMemory shareMem;
	bool m_bPause;
	NOTIFYICONDATA nid;
	CListCtrl m_connListCtrl;
	bool m_bShow;
	DWORD m_dwShouldRecvMsgCount;	//�ܽ��հ���
	int m_dwLostMsgCount;			//������
	DWORD m_dwFetchedMsgCount;		// ��λ��ȡ�ߵ���Ϣ��
protected:
	HICON m_hIcon;

	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBnClickedBtnPause();
	afx_msg void OnBnClickedBtnStart();
	afx_msg void OnBnClickedBtnHide();
	afx_msg LRESULT OnAddTrayIcon(WPARAM wParam,LPARAM lParam);
	DECLARE_MESSAGE_MAP()
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnWindowPosChanging(LPWINDOWPOS lpWndPos);
private:
	BOOL ReadConfig();
	BOOL SaveConnArrayToConfig(); //��connArray save��config�ļ���
	void InitConnectionListCtrl();
	void StartConnection(KConnection *conn);
public:
	afx_msg void OnBnClickedButtonAddDevice();
	afx_msg void OnBnClickedButtonDelDevice();
};
