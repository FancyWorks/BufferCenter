#pragma once

#include "KData.h"
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

const int RESPONSE_SIZE = 46;
const int REQUERST_SIZE = 12;

enum Status{
	S_READY,S_START,S_FIRST_RECV,S_SECOND_RECV,S_TIMEOUT,S_ERROR,S_DONE
};

extern char gStatusMsg[][20];

class KConnection
{
public:
	KConnection(void);
	KConnection(char *ip,DWORD port,byte deviceNumber);
	BOOL Init(char *ip,DWORD port,byte deviceNumber);
	~KConnection(void);
public:
	BOOL Start();
	static void FetchData(void *params);
	static void EndThread(void *params);
	static void ReadCB(struct bufferevent *bev, void *params);
	static void WriteLog(KConnection*conn, char *logStr,int len );
public:
	DWORD m_dwStatus;	//当前处于的状态
	char m_szIP[16];
	DWORD m_dwPort;
	byte m_bDeviceNumber;
	KData m_data;	//存储要上报的数据, 都是完整的包
	KData m_buffer;	//存储缓冲数据,接收完或超时后,从中分析出正确的包放到m_data中
	struct sockaddr_in sin;
	CString m_errorMsg;
	DWORD m_dwCheckCount;//被检查的次数
	DWORD m_dwMsgCountToRecv; //还需要n条消息需要接收
	bool m_bReadyToDel;	//如果此属性为true, 则意味将被删除
	HANDLE m_hMutex;
	BOOL m_bEnd;	//是否结束,S_TIMEOUT,S_ERROR,S_DONE 都为结束状态

	struct event_base *m_base;
	struct bufferevent *m_bev;
};
