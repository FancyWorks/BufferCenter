#include "StdAfx.h"
#include "KConnection.h"
#include <process.h>

#include "KRequest.h"
#include "KResponse.h"
#include "KStringUtil.h"

char gStatusMsg[][20] = {
	"׼��","��ʼ","��һ�ν���","�ڶ��ν���","��ʱ","����","���"
};

extern DWORD gDelayAfterConnect;	//���ӳɹ�����ʱ
extern DWORD gDelayAfterDisconnect;	//�Ͽ����Ӻ���ʱ
extern DWORD gThreadTimeout;		//�̳߳�ʱ

KConnection::KConnection(void)
{
}

KConnection::KConnection( char *ip,DWORD port,byte deviceNumber )
{
	Init(ip,port,deviceNumber);
}

BOOL KConnection::Init( char *ip,DWORD port,byte deviceNumber )
{
	m_bReadyToDel = false;
	m_bEnd = FALSE;
	m_dwMsgCountToRecv = 0;
	m_dwCheckCount = 0;
	m_dwStatus = S_READY;
	strcpy(m_szIP,ip);
	m_dwPort = port;
	m_bDeviceNumber = deviceNumber;
	return TRUE;
}

KConnection::~KConnection(void)
{
	OutputDebugString("Destroy KConnection\n");
}

BOOL KConnection::Start()
{
	HANDLE hThread = (HANDLE)_beginthread(KConnection::FetchData,0,(LPVOID)this);
	if(hThread==NULL) 
	{
		this->m_dwStatus = S_ERROR;
		this->m_errorMsg = "�����߳�ʧ��";
		return FALSE;
	}
	return TRUE;
}

void KConnection::FetchData(void *params)
{
	KConnection *conn = (KConnection*)params;
	//�������
	conn->m_bEnd = FALSE;
	conn->m_dwStatus = S_START;
	conn->m_dwCheckCount = 0;
	conn->m_dwMsgCountToRecv = 0;
	conn->m_errorMsg = "";
	conn->m_data.Clear();
	conn->m_buffer.Clear();
	//conn->m_hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, "buffer_center_log");
	ZeroMemory(&conn->sin,sizeof(conn->sin));

	conn->sin.sin_family = AF_INET;
	conn->sin.sin_addr.S_un.S_addr = inet_addr(conn->m_szIP);
	conn->sin.sin_port = htons(conn->m_dwPort);

	try {
		conn->m_base = event_base_new();
		if(conn->m_base==NULL)
		{
			conn->m_dwStatus = S_ERROR;
			conn->m_errorMsg = "��ʼ��libeventʧ��";
			conn->m_bEnd = TRUE;
			_endthread();
			return;
		}
		conn->m_bev = bufferevent_socket_new(conn->m_base, -1, BEV_OPT_CLOSE_ON_FREE);
		if(conn->m_bev==NULL)
		{
			conn->m_dwStatus = S_ERROR;
			conn->m_errorMsg = "����socketʧ��";
			conn->m_bEnd = TRUE;
			_endthread();
			return;
		}

		//�տ�ʼ����ˮλΪRESPONSE_SIZE, ���ڽ�����Ϣ���İ�, �����������Ӳ���,��shut down���ͨѶ
		bufferevent_setwatermark(conn->m_bev,EV_READ,RESPONSE_SIZE,0);
		bufferevent_setcb(conn->m_bev, KConnection::ReadCB, NULL, NULL, (LPVOID)conn);
		bufferevent_enable(conn->m_bev, EV_READ|EV_WRITE);

		int conn_ret = bufferevent_socket_connect(conn->m_bev,(struct sockaddr *)&conn->sin, sizeof(conn->sin));
		if(conn_ret<0)
		{
			conn->m_dwStatus = S_ERROR;
			conn->m_errorMsg = "����ʧ��";
			conn->m_bEnd = TRUE;
			_endthread();
			return;
		}
		//���ӳɹ����100ms, ����̫�������ô������� 2014-06-13
		Sleep(gDelayAfterConnect);
		//��һ��ͨѶ, ȡ���õ���Ϣ����
		KRequest fetchMsgCountMessage;
		fetchMsgCountMessage.SetDeviceNumber(conn->m_bDeviceNumber);
		bufferevent_write(conn->m_bev,(char*)&fetchMsgCountMessage.msg,REQUERST_SIZE);

		//��ʱ, �������ڳ�ʱ��������ݵĳ�ȡ
		struct timeval delay = { gThreadTimeout/1000, gThreadTimeout%1000 };
		event_base_loopexit(conn->m_base, &delay);

		event_base_dispatch(conn->m_base);

		if(conn->m_bev != NULL)
		{
			bufferevent_free(conn->m_bev);
			conn->m_bev = NULL;
		}
		if(conn->m_base != NULL)
		{
			event_base_free(conn->m_base);
			conn->m_base = NULL;
		}
		//״̬�����������, ��Ϊ��ʱ
		if(conn->m_dwStatus==S_START || conn->m_dwStatus==S_FIRST_RECV || conn->m_dwStatus==S_SECOND_RECV)
		{
			conn->m_dwStatus = S_TIMEOUT;
			//��ʱʱ ������, ������Ĵ��봦���߼�һ��
			DWORD length = conn->m_buffer.GetSize();
			byte *p = (byte*)conn->m_buffer.GetData();
			for(int i=0;i<length;i++)
			{
				if(p[i]==0xFC && i+RESPONSE_SIZE<=length && p[i+RESPONSE_SIZE-1]==0xFD)
				{
					conn->m_data.Append((const char *)(p+i),RESPONSE_SIZE);
					i += RESPONSE_SIZE-1;	//i++
				}
				else if(i+RESPONSE_SIZE<=length) //����Ӧ����һ��������һ������
				{
					conn->m_errorMsg.Format("��������쳣,index=%d, ��Ҫ���յ��������� %d\r\n",i,conn->m_dwMsgCountToRecv);
				}
			}
		}
	}
	catch (...)
	{
		conn->m_bEnd = TRUE;
		OutputDebugString("***FetchData catch***");
	}
	OutputDebugString("---END---");

	//�Ͽ�ǰ��100ms, ����̫�������ô������� 2014-06-13
	Sleep(gDelayAfterDisconnect);
	conn->m_bEnd = TRUE;
	_endthread();
}

void KConnection::ReadCB( struct bufferevent *bev, void *params )
{
	try {
		KConnection *conn = (KConnection*)params;
		struct evbuffer *input = bufferevent_get_input(bev);
		size_t len = evbuffer_get_length(input);

		if(len>0)
		{
			if(conn->m_dwStatus==S_START) //��һ�ν�������
			{
				conn->m_dwStatus = S_FIRST_RECV;
				KResponse res;
				bufferevent_read(bev,&res.msg,RESPONSE_SIZE);	//����ˮλ������,��һ�ν��յ�����һ����RESPONSE_SIZE���ֽ�
				if(res.msg.head==0xFC && res.msg.tail==0xFD)
				{
					conn->m_data.Append((const char *)&res.msg,RESPONSE_SIZE);
					DWORD msgCount = res.GetMsgCount();
					if(msgCount>0) //��һ��ͨѶ�Ľ��: ��msgCount������
					{
						CString tmp;
						tmp.Format("msgCount = %u\r\n",msgCount);
						OutputDebugString(tmp);

						conn->m_dwMsgCountToRecv = msgCount;
						//�ڶ���ͨѶ����
						KRequest req;
						req.SetMsgCount(msgCount);
						req.SetDeviceNumber(conn->m_bDeviceNumber);
						bufferevent_write(bev,(char*)&req.msg,sizeof(KRequestMsg));
						//����ˮλ, ���Բ��ϵĽ��շֶε�����,Ȼ����������
						bufferevent_setwatermark(bev,EV_READ,0,0);
					}
					else //��һ��ͨѶ�Ľ��: ������ ����ڶ���ͨѶ
					{
						OutputDebugString("��һ��ͨѶ�Ľ��: ������ ����ڶ���ͨѶ");
						conn->m_dwStatus = S_DONE;
						event_base_loopexit(conn->m_base, NULL);
					}
				}
				else
				{
					OutputDebugString("��һ��ͨѶ�ķ��ذ�����ȷ,�Ͽ�����");
					conn->m_dwStatus = S_ERROR;
					conn->m_errorMsg = "��һ��ͨѶ�ķ��ذ�����ȷ";
					event_base_loopexit(conn->m_base, NULL);
				}
			}
			else if(conn->m_dwStatus==S_FIRST_RECV || conn->m_dwStatus==S_SECOND_RECV)
			{
				conn->m_dwStatus = S_SECOND_RECV;
				char *pBuffer = new char[len];
				bufferevent_read(bev,pBuffer,len);
				conn->m_buffer.Append(pBuffer,len);
				delete [] pBuffer;

				CString tmp;
				tmp.Format("���ճ��� = %u\r\n",len);
				OutputDebugString(tmp);

				if(conn->m_buffer.GetSize()==conn->m_dwMsgCountToRecv*RESPONSE_SIZE)
				{
					//�ѽ���ȫ������,������
					DWORD length = conn->m_buffer.GetSize();
					byte *p = (byte*)conn->m_buffer.GetData();
					for(int i=0;i<length;i++)
					{
						if(p[i]==0xFC && i+RESPONSE_SIZE<=length && p[i+RESPONSE_SIZE-1]==0xFD)
						{
							conn->m_data.Append((const char *)(p+i),RESPONSE_SIZE);
							i += RESPONSE_SIZE-1;	//i++
						}
						else if(i+RESPONSE_SIZE<=length) //����Ӧ����һ��������һ������
						{
							conn->m_errorMsg.Format("��������쳣,index=%d, ��Ҫ���յ��������� %d\r\n",i,conn->m_dwMsgCountToRecv);
						}
					}
					OutputDebugString("�ѽ���ȫ�������˳�\r\n");
					conn->m_dwStatus = S_DONE;
					event_base_loopexit(conn->m_base, NULL);
				}
			}
		}
	}
	catch(...)
	{
		OutputDebugString("***ReadCB catch***");
	}
}
// void KConnection::ReadCB( struct bufferevent *bev, void *params )
// {
// 	KConnection *conn = (KConnection*)params;
// 	struct evbuffer *input = bufferevent_get_input(bev);
// 	size_t len = evbuffer_get_length(input);
// 	extern CFile logFile;
// 
// 	if(len==RESPONSE_SIZE) {
// 		KResponse res;
// 		bufferevent_read(bev,&res.msg,RESPONSE_SIZE);
// 		if(res.msg.head==0xFC && res.msg.tail==0xFD)
// 		{
// 			conn->m_data.Append((const char *)&res.msg,RESPONSE_SIZE);
// 
// 			if(conn->m_dwStatus==S_START) //��һ�ν�������
// 			{
// 				conn->m_dwStatus = S_FIRST_RECV;
// 
// 				DWORD msgCount = res.GetMsgCount();
// 				if(msgCount>0) //��һ��ͨѶ�Ľ��: ��msgCount������
// 				{
// 					conn->m_dwMsgCountToRecv = msgCount;
// // 					CString tmp;
// // 					tmp.Format("��һ��ͨѶ����,��Ҫ���յ��������� %d\n",msgCount);
// // 					OutputDebugString(tmp);
// 					//�ڶ���ͨѶ����
// 					KRequest req;
// 					req.SetMsgCount(msgCount);
// 					req.SetDeviceNumber(conn->m_bDeviceNumber);
// 					bufferevent_write(bev,(char*)&req.msg,sizeof(KRequestMsg));
// 					//����ˮλ, ��������ʱ��һ���Խ���
// 					//bufferevent_setwatermark(bev,EV_READ,RESPONSE_SIZE*msgCount,0);
// 				}
// 				else //��һ��ͨѶ�Ľ��: ������ ����ڶ���ͨѶ
// 				{
// 					OutputDebugString("��һ��ͨѶ�Ľ��: ������ ����ڶ���ͨѶ");
// 					event_base_loopexit(conn->m_base, NULL);
// 				}
// 			}
// 			else if(conn->m_dwStatus==S_FIRST_RECV || conn->m_dwStatus==S_SECOND_RECV) //�ڶ��ν�������
// 			{
// 				conn->m_dwMsgCountToRecv--;
// 				conn->m_dwStatus = S_SECOND_RECV;
// 				CString tmp;
// 				tmp.Format("�ڶ���ͨѶ����,��Ҫ���յ��������� %d\n",conn->m_dwMsgCountToRecv);
// 				OutputDebugString(tmp);
// 				if(conn->m_dwMsgCountToRecv==0) //�������, �˳�
// 				{
// 					OutputDebugString("�������, �˳�");
// 					event_base_loopexit(conn->m_base, NULL);
// 				}
// 			}
// 		}
// 		else
// 		{
// 			//���ݰ�ͷβ���� �����˰�
// 			conn->m_dwMsgCountToRecv--;
// 			char msg[46*2+1] = {0};
// 			Data2HexString(msg,(const char *)&res.msg,len);
// 			CString tmp;
// 			tmp.Format("���ݰ�ͷβ���� �����˰�,��Ҫ���յ��������� %d, data=%s\r\n",conn->m_dwMsgCountToRecv,msg);
// 			WriteLog(conn,tmp.GetBuffer(),tmp.GetLength());
// // 			OutputDebugString(tmp);
// 		}
// 	}
// 	else if(len<RESPONSE_SIZE)
// 	{
// 		//���ݳ���<46�ֽ� �����˰�
// 		conn->m_dwMsgCountToRecv--;
// 		byte *pBuffer = new byte[len];
// 		bufferevent_read(bev,pBuffer,len);
// 		char msg[46*2] = {0};
// 		Data2HexString(msg,(const char *)pBuffer,len);
// 
// 		CString tmp;
// 		tmp.Format("���ݳ���<46�ֽ� �����˰�,��Ҫ���յ��������� %d, data=%s\r\n",conn->m_dwMsgCountToRecv,msg);
// 		WriteLog(conn,tmp.GetBuffer(),tmp.GetLength());
// 
// 		delete [] pBuffer;
// // 		OutputDebugString(tmp);
// 	}
// 	else if(len>RESPONSE_SIZE)
// 	{
// 		//���ݳ���>RESPONSE_SIZE, ��Ҫ���
// 		byte *pBuffer = new byte[len];
// 		bufferevent_read(bev,pBuffer,len);
// 		for(int i=0;i<len;i++)
// 		{
// 			if(pBuffer[i]==0xFC && i+RESPONSE_SIZE<=len && pBuffer[i+RESPONSE_SIZE-1]==0xFD)
// 			{
// 				conn->m_data.Append((const char *)(pBuffer+i),RESPONSE_SIZE);
// 				conn->m_dwMsgCountToRecv--;
// 				i += RESPONSE_SIZE-1;	//i++
// 			}
// 			else if(i+RESPONSE_SIZE<=len)
// 			{
// 				CString tmp;
// 				tmp.Format("��������쳣,index=%d, ��Ҫ���յ��������� %d\r\n",i,conn->m_dwMsgCountToRecv);
// 				WriteLog(conn,tmp.GetBuffer(),tmp.GetLength());
// 			}
// 		}
// 		if(conn->m_dwMsgCountToRecv==0) //�������, �˳� PS:����м������ݰ�����, ��m_dwMsgCountToRecv>0,ֻ�еȳ�ʱ�����˳�
// 		{
// 			OutputDebugString("������Ϣ�������, �˳�");
// 			event_base_loopexit(conn->m_base, NULL);
// 		}
// 		if(conn->m_dwMsgCountToRecv<0)
// 		{
// 			CString tmp;
// 			tmp.Format("�����쳣m_dwMsgCountToRecv<0, ��Ҫ���յ��������� %d\r\n",conn->m_dwMsgCountToRecv);
// 			WriteLog(conn,tmp.GetBuffer(),tmp.GetLength());
// 		}
// 		delete [] pBuffer;
// 	}
// }
// void KConnection::ReadCB( struct bufferevent *bev, void *params )
// {
// 	KConnection *conn = (KConnection*)params;
// 	struct evbuffer *input = bufferevent_get_input(bev);
// 	size_t len = evbuffer_get_length(input);
// 
// 	if(len==RESPONSE_SIZE) {
// 		KResponse res;
// 		bufferevent_read(bev,&res.msg,RESPONSE_SIZE);
// 		if(res.msg.head==0xFC && res.msg.tail==0xFD)
// 		{
// 			conn->m_data.Append((const char *)&res.msg,RESPONSE_SIZE);
// 
// 			if(conn->m_dwStatus==S_START)
// 			{
// 				conn->m_dwStatus = S_FIRST_RECV; //״̬:��һ�ν�������
// 
// 				DWORD msgCount = res.GetMsgCount();
// 				if(msgCount>0) //��һ��ͨѶ�Ľ��: ��msgCount������
// 				{
// 					//�ڶ���ͨѶ����
// 					KRequest req;
// 					req.SetMsgCount(msgCount);
// 					req.SetDeviceNumber(conn->m_bDeviceNumber);
// 					bufferevent_write(bev,(char*)&req.msg,sizeof(KRequestMsg));
// 					//����ˮλ, ��������ʱ��һ���Խ���
// 					//bufferevent_setwatermark(bev,EV_READ,RESPONSE_SIZE*msgCount,0);
// 				}
// 				else //��һ��ͨѶ�Ľ��: ������ ����ڶ���ͨѶ
// 				{
// 					struct timeval delay = { 0, 0 };
// 					event_base_loopexit(conn->m_base, &delay);
// 				}
// 			}
// 			else if(conn->m_dwStatus==S_FIRST_RECV)
// 			{
// 				conn->m_dwStatus = S_SECOND_RECV; //״̬: �ڶ��ν������� (��һ������,len==RESPONSE_SIZE)
// 				struct timeval delay = { 0, 0 };
// 				event_base_loopexit(conn->m_base, &delay);
// 			}
// 		}
// 		else	//��Ӧ��ͷβ����
// 		{
// 			struct timeval delay = { 0, 0 };
// 			event_base_loopexit(conn->m_base, &delay);
// 		}
// 
// 	}
// 	else if(len>RESPONSE_SIZE) //�ڶ��ν�������(�ж�������) ȡȫ����Ϣ
// 	{
// 		conn->m_dwStatus = S_SECOND_RECV;
// 		char *buffer = new char[len+1];
// 		bufferevent_read(bev,buffer,len);
// 		conn->m_data.Append(buffer,len);
// 
// 		delete buffer;
// 		buffer = NULL;
// 
// 		struct timeval delay = { 0, 0 };
// 		event_base_loopexit(conn->m_base, &delay);
// 	}
// }

void KConnection::EndThread( void *params )
{
	//TODO:�˹����ݲ���Ҫ, �ó�ʱȡ��
// 	KConnection *conn = (KConnection*)params;
// 	char str[100] = {0};
// 	sprintf(str,"%d,EndThread\n",conn->m_dwCheckCount);
// 	OutputDebugString(str);
// 	struct timeval delay = { 0, 0 };
// 	//event_base_loopexit(conn->m_base, &delay);
// 	event_base_loopbreak(conn->m_base);
}

