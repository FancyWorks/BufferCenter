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
	DWORD m_dwStatus;	//��ǰ���ڵ�״̬
	char m_szIP[16];
	DWORD m_dwPort;
	byte m_bDeviceNumber;
	KData m_data;	//�洢Ҫ�ϱ�������, ���������İ�
	KData m_buffer;	//�洢��������,�������ʱ��,���з�������ȷ�İ��ŵ�m_data��
	struct sockaddr_in sin;
	CString m_errorMsg;
	DWORD m_dwCheckCount;//�����Ĵ���
	DWORD m_dwMsgCountToRecv; //����Ҫn����Ϣ��Ҫ����
	bool m_bReadyToDel;	//���������Ϊtrue, ����ζ����ɾ��
	HANDLE m_hMutex;
	BOOL m_bEnd;	//�Ƿ����,S_TIMEOUT,S_ERROR,S_DONE ��Ϊ����״̬

	struct event_base *m_base;
	struct bufferevent *m_bev;
};
