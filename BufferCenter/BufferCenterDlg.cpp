// BufferCenterDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "BufferCenter.h"
#include "BufferCenterDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

const int TIMER_UPDATE = 1;
const int TIMER_RUNTIME = 2;

//在多线程中使用, 在程序初始化时读取配置项
DWORD gThreadTimeout = 1000;	//线程超时
DWORD gDelayAfterConnect = 0;	//连接成功后延时
DWORD gDelayAfterDisconnect = 0;	//断开连接后延时

#define ID_TASKBARICON	100
#define WM_ADDTRAYICON	(WM_USER + 101)

const char deviceConfigFileName[] = "device_config.ini";
const char exeConfigFileName[] = "exe_config.ini";


// CBufferCenterDlg 对话框

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
	Shell_NotifyIcon(NIM_DELETE, &notifyicondata);//卸载托盘图标
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
	ON_MESSAGE(WM_ADDTRAYICON, OnAddTrayIcon)//添加消息映射
	ON_BN_CLICKED(IDC_BTN_HIDE, &CBufferCenterDlg::OnBnClickedBtnHide)
	ON_BN_CLICKED(IDC_BUTTON_ADD_DEVICE, &CBufferCenterDlg::OnBnClickedButtonAddDevice)
	ON_BN_CLICKED(IDC_BUTTON_DEL_DEVICE, &CBufferCenterDlg::OnBnClickedButtonDelDevice)
END_MESSAGE_MAP()


BOOL CBufferCenterDlg::PreTranslateMessage( MSG* pMsg )
{
	switch(pMsg->message)
	{
	case WM_KEYDOWN:
		if(pMsg->wParam==VK_ESCAPE)	//屏蔽ESC退出对话框
			return TRUE;
		break;
	}
	return CDialog::PreTranslateMessage(pMsg);
}


// CBufferCenterDlg 消息处理程序

BOOL CBufferCenterDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码

	WSADATA wsa_data;
	int wasResult = WSAStartup(0x0201, &wsa_data);
	
	UpdateData(FALSE);

	if(!shareMem.IsCreated())
		shareMem.Create(NULL, "MiddlewareShareData", 6*1024*1024);//"c:\\hd_mw_data.tmp"

	if(!ReadConfig())
	{
		MessageBox("读配置文件失败\r\n\r\n文件名:\r\ndevice_config.ini\r\n格式:(每行定义一个设备)\r\nIP|端口|设备号");
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

	//自动启动
	OnBnClickedBtnStart();
	SetTimer(TIMER_RUNTIME,1000,NULL);
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CBufferCenterDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}


//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CBufferCenterDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CBufferCenterDlg::OnTimer(UINT_PTR nIDEvent)
{
	if(nIDEvent==TIMER_UPDATE)
	{
		int nNewMsgCount = 0;	//每次处理connArray后新增加的包数

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
						m_dwShouldRecvMsgCount += conn->m_dwMsgCountToRecv+1; //+1 为第一次请求后的数据
						//nNewMsgCount += conn->m_data.GetSize()/46;	//以实际接收的为准,这样才能算出丢包

						conn->m_data.Clear();
						StartConnection(conn);
						m_connListCtrl.SetItemText(index, 4, "");
					}
					else	//加入缓存区失败, 超过上限了6M
					{
						m_connListCtrl.SetItemText(index, 4, "达到缓存上限, 等待被取走");
					}
				}
				else	//无数据返回, 继续取
				{
					StartConnection(conn);
				}
				break;
			}
			m_connListCtrl.SetItemText(index, 3, gStatusMsg[conn->m_dwStatus]);
			conn->m_dwCheckCount++;
			//此处的删除 可能会丢失还在接收的数据, 估计都想删除了, 此处的数据应该就不考虑了
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
		//总接收包数 = 缓存数 + 上位机取走数 + 丢包数
		//上位机取走数 = 上次缓存消息数 + 新加入缓存的消息数 - 当前缓存消息数
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
		m_szRunTime.Format("%d天 %d时 %d分 %d秒",dwDays,dwHours,dwMinutes,dwSeconds);
	}
	UpdateData(FALSE);
	CDialog::OnTimer(nIDEvent);
}
//读取配置文件
BOOL CBufferCenterDlg::ReadConfig()
{
	CStdioFile file;
	if(!file.Open(deviceConfigFileName,CFile::modeRead))
		return FALSE;

	CString lineString;
	while(file.ReadString(lineString))
	{
		//#注释行
		if(lineString.GetLength()>0 && lineString.GetAt(0)=='#')
			continue;
		int index1 = lineString.Find('|');
		int index2 = lineString.Find('|',index1+1);
		if(index1==-1 || index2==-1)
			continue;
		CString IP = lineString.Mid(index1+1,index2-index1-1);
		byte deviceNumber = (byte)atoi(lineString.Left(index1));
		DWORD port = (DWORD)atoi(lineString.Right(lineString.GetLength()-index2-1));

		//如果数据合理, 则新建connection
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

	//设置默认IP的前缀
	if(connArray.size()>0)
	{
		CString IP = connArray[0]->m_szIP;
		int index = IP.ReverseFind('.');
		SetDlgItemText(IDC_IPADDRESS,IP.Left(index));
	}

	//处理exe_config.ini
	exeConfigFileName;
	CString exeConfigFilePath = exeConfigFileName;
	exeConfigFilePath.Insert(0,"src/../");
	gDelayAfterConnect = GetPrivateProfileInt("基本信息","连接成功后延时",0,exeConfigFilePath.GetBuffer());
	gDelayAfterDisconnect = GetPrivateProfileInt("基本信息","断开连接后延时",0,exeConfigFilePath.GetBuffer());
	gThreadTimeout = GetPrivateProfileInt("基本信息","请求超时",0,exeConfigFilePath.GetBuffer());
	if(gThreadTimeout<1000)	//最少1000ms
		gThreadTimeout = 1000;
	return TRUE;
}

void CBufferCenterDlg::InitConnectionListCtrl()
{
	DWORD dwStyle = m_connListCtrl.GetExtendedStyle();
	dwStyle |= LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES;
	m_connListCtrl.SetExtendedStyle(dwStyle);

	//header
	m_connListCtrl.InsertColumn( 0, "设备号", LVCFMT_LEFT, 50 );
	m_connListCtrl.InsertColumn( 1, "IP", LVCFMT_LEFT, 130 );
	m_connListCtrl.InsertColumn( 2, "端口", LVCFMT_LEFT, 50 );
	m_connListCtrl.InsertColumn( 3, "状态", LVCFMT_LEFT, 100 );
	m_connListCtrl.InsertColumn( 4, "错误信息", LVCFMT_LEFT, 350 );

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
		GetDlgItem(IDC_BTN_PAUSE)->SetWindowText("继续") ;
	}
	else
	{
		GetDlgItem(IDC_BTN_PAUSE)->SetWindowText("暂停") ;
	}
}

void CBufferCenterDlg::OnBnClickedBtnStart()
{
	SetTimer(TIMER_UPDATE,500,NULL);
	GetDlgItem(IDC_BTN_START)->EnableWindow(FALSE);
}
LRESULT CBufferCenterDlg::OnAddTrayIcon(WPARAM wParam,LPARAM lParam)
{
	if (wParam == ID_TASKBARICON)//为创建的托盘图标
	{
		switch(lParam)//消息的类型
		{
		case WM_LBUTTONDBLCLK://双击左键
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
		//bEnd 标识着线程真正结束
		//[如果线程设置完S_ERROR状态后还要执行其他的代码,此时主线程已经重启了新线程,则会发生多线程改写相同内存的情况]
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
		//检查是否已存在 [规则:设备号不可重复, IP&Port不可同时相同]
		vector<KConnection*>::iterator iter = connArray.begin();
		for(;iter!=connArray.end();iter++)
		{
			KConnection* conn = *iter;
			if(deviceNumber==conn->m_bDeviceNumber)
			{
				MessageBox("设备号已存在","添加失败");
				return ;
			}
			else if(IP.Compare(conn->m_szIP)==0 && port==conn->m_dwPort)
			{
				MessageBox("IP与端口已存在","添加失败");
				return ;
			}
		}
		//push到vector中,将由Timer负责启动 [因为是push到队列后,所以对Timer中的遍历无影响]
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
		MessageBox("设备号或端口不正确. (端口>0, 127>=设备号>0)","添加失败");
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
		MessageBox("请先选中要删除的设备","删除失败");
	}
}

BOOL CBufferCenterDlg::SaveConnArrayToConfig()
{
	CStdioFile file;
	if(!file.Open(deviceConfigFileName,CFile::modeReadWrite))
		file.Open(deviceConfigFileName,CFile::modeCreate);	//如果打开失败, 则新建一个

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

