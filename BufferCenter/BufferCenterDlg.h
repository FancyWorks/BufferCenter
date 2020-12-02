// BufferCenterDlg.h : 头文件
//

#pragma once

#include "KConnection.h"
#include <vector>
#include "afxcmn.h"
#include "XShareMemory.h"
using namespace std;

// CBufferCenterDlg 对话框
class CBufferCenterDlg : public CDialog
{
// 构造
public:
	CBufferCenterDlg(CWnd* pParent = NULL);	// 标准构造函数
	~CBufferCenterDlg();
// 对话框数据
	enum { IDD = IDD_BUFFERCENTER_DIALOG };
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnOK(){return;}
protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持
	vector<KConnection*> connArray;
	DWORD m_dwMsgCount;
	CString m_szRunTime;
	DWORD m_dwRunTimeSecond;
	XShareMemory shareMem;
	bool m_bPause;
	NOTIFYICONDATA nid;
	CListCtrl m_connListCtrl;
	bool m_bShow;
	DWORD m_dwShouldRecvMsgCount;	//总接收包数
	int m_dwLostMsgCount;			//丢包数
	DWORD m_dwFetchedMsgCount;		// 上位机取走的消息数
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
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
	BOOL SaveConnArrayToConfig(); //将connArray save到config文件中
	void InitConnectionListCtrl();
	void StartConnection(KConnection *conn);
public:
	afx_msg void OnBnClickedButtonAddDevice();
	afx_msg void OnBnClickedButtonDelDevice();
};
