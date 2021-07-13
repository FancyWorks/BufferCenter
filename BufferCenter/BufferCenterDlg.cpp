// BufferCenterDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "BufferCenter.h"
#include "BufferCenterDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

const int TIMER_UPDATE = 1;
const int TIMER_RUNTIME = 2;

//�ڶ��߳���ʹ��, �ڳ����ʼ��ʱ��ȡ������
DWORD gThreadTimeout = 1000;	//�̳߳�ʱ
DWORD gDelayAfterConnect = 0;	//���ӳɹ�����ʱ
DWORD gDelayAfterDisconnect = 0;	//�Ͽ����Ӻ���ʱ

#define ID_TASKBARICON	100
#define WM_ADDTRAYICON	(WM_USER + 101)

const char deviceConfigFileName[] = "device_config.ini";
const char exeConfigFileName[] = "exe_config.ini";


// CBufferCenterDlg �Ի���

CBufferCenterDlg::CBufferCenterDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CBufferCenterDlg::IDD, pParent)
	, m_dwMsgCount(0)
	, m_szRunTime(_T(""))
	, m_dwShouldRecvMsgCount(0)
	, m_dwLostMsgCount(0)
	, m_dwFetchedMsgCount(0)
{
	m_bPause = false;
	m_bShow  = false;
	m_dwRunTimeSecond = 0;
	m_hIcon = AfxGetApp()->LoadIcon(IDI_ICON_128);
}

CBufferCenterDlg::~CBufferCenterDlg()
{
	OutputDebugString("Dlg Destroy\n");
	vector<KConnection*>::iterator iter = connArray.begin();
	for(;iter!=connArray.end();iter++)
	{
		delete *iter;
	}
	WSACleanup();

	NOTIFYICONDATA notifyicondata;
	notifyicondata.cbSize = sizeof(NOTIFYICONDATA);
	notifyicondata.uFlags = 0;
	notifyicondata.hWnd = m_hWnd;
	notifyicondata.uID = ID_TASKBARICON;
	Shell_NotifyIcon(NIM_DELETE, &notifyicondata);//ж������ͼ��
}

void CBufferCenterDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_MSG_COUNT, m_dwMsgCount);
	DDX_Control(pDX, IDC_LIST_CONNECTION, m_connListCtrl);
	DDX_Text(pDX, IDC_TEXT_RUNTIME, m_szRunTime);
	DDX_Text(pDX, IDC_SHOULD_RECV_MSG_COUNT, m_dwShouldRecvMsgCount);
	DDX_Text(pDX, IDC_LOST_MSG_COUNT, m_dwLostMsgCount);
	DDX_Text(pDX, IDC_FETCHED_MSG_COUNT, m_dwFetchedMsgCount);
}

BEGIN_MESSAGE_MAP(CBufferCenterDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_WM_TIMER()
	ON_WM_WINDOWPOSCHANGING()
	ON_BN_CLICKED(IDC_BTN_PAUSE, &CBufferCenterDlg::OnBnClickedBtnPause)
	ON_BN_CLICKED(IDC_BTN_START, &CBufferCenterDlg::OnBnClickedBtnStart)
	ON_MESSAGE(WM_ADDTRAYICON, OnAddTrayIcon)//�����Ϣӳ��
	ON_BN_CLICKED(IDC_BTN_HIDE, &CBufferCenterDlg::OnBnClickedBtnHide)
	ON_BN_CLICKED(IDC_BUTTON_ADD_DEVICE, &CBufferCenterDlg::OnBnClickedButtonAddDevice)
	ON_BN_CLICKED(IDC_BUTTON_DEL_DEVICE, &CBufferCenterDlg::OnBnClickedButtonDelDevice)
END_MESSAGE_MAP()


BOOL CBufferCenterDlg::PreTranslateMessage( MSG* pMsg )
{
	switch(pMsg->message)
	{
	case WM_KEYDOWN:
		if(pMsg->wParam==VK_ESCAPE)	//����ESC�˳��Ի���
			return TRUE;
		break;
	}
	return CDialog::PreTranslateMessage(pMsg);
}


// CBufferCenterDlg ��Ϣ�������

BOOL CBufferCenterDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// ���ô˶Ի����ͼ�ꡣ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��

	// TODO: �ڴ���Ӷ���ĳ�ʼ������

	WSADATA wsa_data;
	int wasResult = WSAStartup(0x0201, &wsa_data);
	
	UpdateData(FALSE);

	if(!shareMem.IsCreated())
		shareMem.Create(NULL, "MiddlewareShareData", 6*1024*1024);//"c:\\hd_mw_data.tmp"

	if(!ReadConfig())
	{
		MessageBox("�������ļ�ʧ��\r\n\r\n�ļ���:\r\ndevice_config.ini\r\n��ʽ:(ÿ�ж���һ���豸)\r\nIP|�˿�|�豸��");
	}

	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = m_hWnd;
	nid.uID = ID_TASKBARICON;
	nid.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
	nid.uCallbackMessage = WM_ADDTRAYICON;
	nid.hIcon = AfxGetApp()->LoadIcon(IDI_ICON_16);
	strcpy(nid.szTip, "Buffer Center");
	Shell_NotifyIcon(NIM_ADD, &nid);

	InitConnectionListCtrl();

	//�Զ�����
	OnBnClickedBtnStart();
	SetTimer(TIMER_RUNTIME,1000,NULL);
	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
}

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CBufferCenterDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}


//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CBufferCenterDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CBufferCenterDlg::OnTimer(UINT_PTR nIDEvent)
{
	if(nIDEvent==TIMER_UPDATE)
	{
		int nNewMsgCount = 0;	//ÿ�δ���connArray�������ӵİ���

		vector<KConnection*>::iterator iter = connArray.begin();
		int index = 0;
		for(;iter!=connArray.end();)
		{
			KConnection *conn = *iter;
			m_connListCtrl.SetItemText(index, 4, "");
			switch(conn->m_dwStatus)
			{
			case S_READY:
				StartConnection(conn);
				break;
// 			case S_START:
// 				break;
// 			case S_FIRST_RECV:
// 				break;
// 			case S_SECOND_RECV:
// 				break;
			case S_ERROR:
				m_connListCtrl.SetItemText(index, 4, conn->m_errorMsg);
				StartConnection(conn);
				break;
			case S_TIMEOUT:
			case S_DONE:
				if(conn->m_data.GetSize()>0) {
					if(shareMem.Append(conn->m_data.GetData(),conn->m_data.GetSize())) {
						m_dwShouldRecvMsgCount += conn->m_dwMsgCountToRecv+1; //+1 Ϊ��һ������������
						//nNewMsgCount += conn->m_data.GetSize()/46;	//��ʵ�ʽ��յ�Ϊ׼,���������������

						conn->m_data.Clear();
						StartConnection(conn);
						m_connListCtrl.SetItemText(index, 4, "");
					}
					else	//���뻺����ʧ��, ����������6M
					{
						m_connListCtrl.SetItemText(index, 4, "�ﵽ��������, �ȴ���ȡ��");
					}
				}
				else	//�����ݷ���, ����ȡ
				{
					StartConnection(conn);
				}
				break;
			}
			m_connListCtrl.SetItemText(index, 3, gStatusMsg[conn->m_dwStatus]);
			conn->m_dwCheckCount++;
			//�˴���ɾ�� ���ܻᶪʧ���ڽ��յ�����, ���ƶ���ɾ����, �˴�������Ӧ�þͲ�������
			if( conn->m_bReadyToDel )
			{
				iter = connArray.erase(iter);
				m_connListCtrl.DeleteItem(index);
				SaveConnArrayToConfig();
			}
			else
			{
				iter++;
				index++;
			}
		}
		//�ܽ��հ��� = ������ + ��λ��ȡ���� + ������
		//��λ��ȡ���� = �ϴλ�����Ϣ�� + �¼��뻺�����Ϣ�� - ��ǰ������Ϣ��
		m_dwFetchedMsgCount = shareMem.GetFetchedMsgCount();
		m_dwMsgCount = shareMem.GetMsgCount();
		m_dwLostMsgCount = m_dwShouldRecvMsgCount - m_dwMsgCount - m_dwFetchedMsgCount;
	}
	else if(nIDEvent==TIMER_RUNTIME)
	{
		m_dwRunTimeSecond++;
		DWORD dwDays	= m_dwRunTimeSecond/(3600*24);
		DWORD dwHours	= (m_dwRunTimeSecond%(3600*24))/3600;
		DWORD dwMinutes	= (m_dwRunTimeSecond%3600)/60;
		DWORD dwSeconds	= m_dwRunTimeSecond%60;
		m_szRunTime.Format("%d�� %dʱ %d�� %d��",dwDays,dwHours,dwMinutes,dwSeconds);
	}
	UpdateData(FALSE);
	CDialog::OnTimer(nIDEvent);
}
//��ȡ�����ļ�
BOOL CBufferCenterDlg::ReadConfig()
{
	CStdioFile file;
	if(!file.Open(deviceConfigFileName,CFile::modeRead))
		return FALSE;

	CString lineString;
	while(file.ReadString(lineString))
	{
		//#ע����
		if(lineString.GetLength()>0 && lineString.GetAt(0)=='#')
			continue;
		int index1 = lineString.Find('|');
		int index2 = lineString.Find('|',index1+1);
		if(index1==-1 || index2==-1)
			continue;
		CString IP = lineString.Mid(index1+1,index2-index1-1);
		byte deviceNumber = (byte)atoi(lineString.Left(index1));
		DWORD port = (DWORD)atoi(lineString.Right(lineString.GetLength()-index2-1));

		//������ݺ���, ���½�connection
		if(port>0 && deviceNumber>0 && deviceNumber<=127)
		{
			connArray.push_back(new KConnection(IP.GetBuffer(0),port,deviceNumber));
		}
		else
		{
			OutputDebugString("analyse config error\n");
		}
	}
	file.Close();

	//����Ĭ��IP��ǰ׺
	if(connArray.size()>0)
	{
		CString IP = connArray[0]->m_szIP;
		int index = IP.ReverseFind('.');
		SetDlgItemText(IDC_IPADDRESS,IP.Left(index));
	}

	//����exe_config.ini
	exeConfigFileName;
	CString exeConfigFilePath = exeConfigFileName;
	exeConfigFilePath.Insert(0,"src/../");
	gDelayAfterConnect = GetPrivateProfileInt("������Ϣ","���ӳɹ�����ʱ",0,exeConfigFilePath.GetBuffer());
	gDelayAfterDisconnect = GetPrivateProfileInt("������Ϣ","�Ͽ����Ӻ���ʱ",0,exeConfigFilePath.GetBuffer());
	gThreadTimeout = GetPrivateProfileInt("������Ϣ","����ʱ",0,exeConfigFilePath.GetBuffer());
	if(gThreadTimeout<1000)	//����1000ms
		gThreadTimeout = 1000;
	return TRUE;
}

void CBufferCenterDlg::InitConnectionListCtrl()
{
	DWORD dwStyle = m_connListCtrl.GetExtendedStyle();
	dwStyle |= LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES;
	m_connListCtrl.SetExtendedStyle(dwStyle);

	//header
	m_connListCtrl.InsertColumn( 0, "�豸��", LVCFMT_LEFT, 50 );
	m_connListCtrl.InsertColumn( 1, "IP", LVCFMT_LEFT, 130 );
	m_connListCtrl.InsertColumn( 2, "�˿�", LVCFMT_LEFT, 50 );
	m_connListCtrl.InsertColumn( 3, "״̬", LVCFMT_LEFT, 100 );
	m_connListCtrl.InsertColumn( 4, "������Ϣ", LVCFMT_LEFT, 350 );

	//rows
	CString tmp;
	vector<KConnection*>::iterator iter = connArray.begin();
	int insertIndex = 0;
	for(;iter!=connArray.end();iter++)
	{
		int nRow = m_connListCtrl.InsertItem(insertIndex++, NULL);
		tmp.Format("%u",(*iter)->m_bDeviceNumber);
		m_connListCtrl.SetItemText(nRow, 0, tmp);
		m_connListCtrl.SetItemText(nRow, 1, (*iter)->m_szIP);
		tmp.Format("%u",(*iter)->m_dwPort);
		m_connListCtrl.SetItemText(nRow, 2, tmp);
		m_connListCtrl.SetItemText(nRow, 3, gStatusMsg[(*iter)->m_dwStatus]);
		m_connListCtrl.SetItemText(nRow, 4, "");
	}
}

void CBufferCenterDlg::OnBnClickedBtnPause()
{
	m_bPause = !m_bPause;
	if(m_bPause)
	{
		GetDlgItem(IDC_BTN_PAUSE)->SetWindowText("����") ;
	}
	else
	{
		GetDlgItem(IDC_BTN_PAUSE)->SetWindowText("��ͣ") ;
	}
}

void CBufferCenterDlg::OnBnClickedBtnStart()
{
	SetTimer(TIMER_UPDATE,500,NULL);
	GetDlgItem(IDC_BTN_START)->EnableWindow(FALSE);
}
LRESULT CBufferCenterDlg::OnAddTrayIcon(WPARAM wParam,LPARAM lParam)
{
	if (wParam == ID_TASKBARICON)//Ϊ����������ͼ��
	{
		switch(lParam)//��Ϣ������
		{
		case WM_LBUTTONDBLCLK://˫�����
			m_bShow = true;
			ShowWindow(SW_SHOWNORMAL);
			break;
		}
	}
	return 1;
}

void CBufferCenterDlg::OnBnClickedBtnHide()
{
	ShowWindow(SW_HIDE);
}

void CBufferCenterDlg::StartConnection( KConnection *conn )
{
	if(!m_bPause && conn)
	{
		//bEnd ��ʶ���߳���������
		//[����߳�������S_ERROR״̬��Ҫִ�������Ĵ���,��ʱ���߳��Ѿ����������߳�,��ᷢ�����̸߳�д��ͬ�ڴ�����]
		if(conn->m_dwStatus==S_READY || conn->m_bEnd)
			conn->Start();
	}
}

void CBufferCenterDlg::OnWindowPosChanging( LPWINDOWPOS lpWndPos )
{
	if(!m_bShow)
		lpWndPos->flags &= ~SWP_SHOWWINDOW;
	CDialog::OnWindowPosChanging(lpWndPos);
}

void CBufferCenterDlg::OnBnClickedButtonAddDevice()
{
	byte deviceNumber = (byte)GetDlgItemInt(IDC_EDIT_DEVICE_NUMBER);
	CString IP;
	GetDlgItemText(IDC_IPADDRESS,IP);
	DWORD port = GetDlgItemInt(IDC_EDIT_PORT);

	if(port>0 && deviceNumber>0 && deviceNumber<=127)
	{
		//����Ƿ��Ѵ��� [����:�豸�Ų����ظ�, IP&Port����ͬʱ��ͬ]
		vector<KConnection*>::iterator iter = connArray.begin();
		for(;iter!=connArray.end();iter++)
		{
			KConnection* conn = *iter;
			if(deviceNumber==conn->m_bDeviceNumber)
			{
				MessageBox("�豸���Ѵ���","���ʧ��");
				return ;
			}
			else if(IP.Compare(conn->m_szIP)==0 && port==conn->m_dwPort)
			{
				MessageBox("IP��˿��Ѵ���","���ʧ��");
				return ;
			}
		}
		//push��vector��,����Timer�������� [��Ϊ��push�����к�,���Զ�Timer�еı�����Ӱ��]
		int insertIndex = m_connListCtrl.GetItemCount();
		CString tmp;
		int nRow = m_connListCtrl.InsertItem(insertIndex, NULL);
		tmp.Format("%u",deviceNumber);
		m_connListCtrl.SetItemText(nRow, 0, tmp);
		m_connListCtrl.SetItemText(nRow, 1, IP);
		tmp.Format("%u",port);
		m_connListCtrl.SetItemText(nRow, 2, tmp);
		m_connListCtrl.SetItemText(nRow, 3, gStatusMsg[S_READY]);
		m_connListCtrl.SetItemText(nRow, 4, "");

		connArray.push_back(new KConnection(IP.GetBuffer(0),port,deviceNumber));
		SaveConnArrayToConfig();
	}
	else 
	{
		MessageBox("�豸�Ż�˿ڲ���ȷ. (�˿�>0, 127>=�豸��>0)","���ʧ��");
	}
}

void CBufferCenterDlg::OnBnClickedButtonDelDevice()
{
	bool bFound = false;
	for(int i=0; i<m_connListCtrl.GetItemCount(); i++)
	{
		if( m_connListCtrl.GetItemState(i, LVIS_SELECTED) == LVIS_SELECTED )
		{
			connArray[i]->m_bReadyToDel = true;
			bFound = true;
		}
	}
	if(!bFound)
	{
		MessageBox("����ѡ��Ҫɾ�����豸","ɾ��ʧ��");
	}
}

BOOL CBufferCenterDlg::SaveConnArrayToConfig()
{
	CStdioFile file;
	if(!file.Open(deviceConfigFileName,CFile::modeReadWrite))
		file.Open(deviceConfigFileName,CFile::modeCreate);	//�����ʧ��, ���½�һ��

	CString lineString;
	vector<CString> commentArray;
	while(file.ReadString(lineString))
	{
		if(lineString.Find('#')==0)
		{
			commentArray.push_back(lineString);
		}
	}
	file.SetLength(0);//empty file
	vector<KConnection*>::iterator iter = connArray.begin();
	CString connString;
	for(;iter!=connArray.end();iter++)
	{
		KConnection* conn = *iter;
		connString.Format("%u|%s|%u\r\n",conn->m_bDeviceNumber,conn->m_szIP,conn->m_dwPort);
		file.WriteString(connString);
	}
	vector<CString>::iterator iterComment = commentArray.begin();
	for(;iterComment!=commentArray.end();iterComment++)
	{
		file.WriteString(*iterComment);
		file.WriteString("\r\n");
	}
	file.Close();
	return TRUE;
}

