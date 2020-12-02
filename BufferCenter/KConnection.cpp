#include "StdAfx.h"
#include "KConnection.h"
#include <process.h>

#include "KRequest.h"
#include "KResponse.h"
#include "KStringUtil.h"

char gStatusMsg[][20] = {
	"准备","开始","第一次接收","第二次接收","超时","错误","完成"
};

extern DWORD gDelayAfterConnect;	//连接成功后延时
extern DWORD gDelayAfterDisconnect;	//断开连接后延时
extern DWORD gThreadTimeout;		//线程超时

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
		this->m_errorMsg = "创建线程失败";
		return FALSE;
	}
	return TRUE;
}

void KConnection::FetchData(void *params)
{
	KConnection *conn = (KConnection*)params;
	//清空数据
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
			conn->m_errorMsg = "初始化libevent失败";
			conn->m_bEnd = TRUE;
			_endthread();
			return;
		}
		conn->m_bev = bufferevent_socket_new(conn->m_base, -1, BEV_OPT_CLOSE_ON_FREE);
		if(conn->m_bev==NULL)
		{
			conn->m_dwStatus = S_ERROR;
			conn->m_errorMsg = "创建socket失败";
			conn->m_bEnd = TRUE;
			_endthread();
			return;
		}

		//刚开始设置水位为RESPONSE_SIZE, 用于接收消息数的包, 如果这个包都接不到,则shut down这次通讯
		bufferevent_setwatermark(conn->m_bev,EV_READ,RESPONSE_SIZE,0);
		bufferevent_setcb(conn->m_bev, KConnection::ReadCB, NULL, NULL, (LPVOID)conn);
		bufferevent_enable(conn->m_bev, EV_READ|EV_WRITE);

		int conn_ret = bufferevent_socket_connect(conn->m_bev,(struct sockaddr *)&conn->sin, sizeof(conn->sin));
		if(conn_ret<0)
		{
			conn->m_dwStatus = S_ERROR;
			conn->m_errorMsg = "连接失败";
			conn->m_bEnd = TRUE;
			_endthread();
			return;
		}
		//连接成功后等100ms, 否则太快了设置处理不过来 2014-06-13
		Sleep(gDelayAfterConnect);
		//第一次通讯, 取可用的消息条数
		KRequest fetchMsgCountMessage;
		fetchMsgCountMessage.SetDeviceNumber(conn->m_bDeviceNumber);
		bufferevent_write(conn->m_bev,(char*)&fetchMsgCountMessage.msg,REQUERST_SIZE);

		//超时, 正常会在超时内完成数据的抽取
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
		//状态以下三种情况, 则为超时
		if(conn->m_dwStatus==S_START || conn->m_dwStatus==S_FIRST_RECV || conn->m_dwStatus==S_SECOND_RECV)
		{
			conn->m_dwStatus = S_TIMEOUT;
			//超时时 处理缓存, 与下面的代码处理逻辑一样
			DWORD length = conn->m_buffer.GetSize();
			byte *p = (byte*)conn->m_buffer.GetData();
			for(int i=0;i<length;i++)
			{
				if(p[i]==0xFC && i+RESPONSE_SIZE<=length && p[i+RESPONSE_SIZE-1]==0xFD)
				{
					conn->m_data.Append((const char *)(p+i),RESPONSE_SIZE);
					i += RESPONSE_SIZE-1;	//i++
				}
				else if(i+RESPONSE_SIZE<=length) //正常应该是一个包挨着一个包的
				{
					conn->m_errorMsg.Format("拆包过程异常,index=%d, 需要接收的数据条数 %d\r\n",i,conn->m_dwMsgCountToRecv);
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

	//断开前等100ms, 否则太快了设置处理不过来 2014-06-13
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
			if(conn->m_dwStatus==S_START) //第一次接收数据
			{
				conn->m_dwStatus = S_FIRST_RECV;
				KResponse res;
				bufferevent_read(bev,&res.msg,RESPONSE_SIZE);	//由于水位的设置,第一次接收的数据一定是RESPONSE_SIZE个字节
				if(res.msg.head==0xFC && res.msg.tail==0xFD)
				{
					conn->m_data.Append((const char *)&res.msg,RESPONSE_SIZE);
					DWORD msgCount = res.GetMsgCount();
					if(msgCount>0) //第一次通讯的结果: 有msgCount条数据
					{
						CString tmp;
						tmp.Format("msgCount = %u\r\n",msgCount);
						OutputDebugString(tmp);

						conn->m_dwMsgCountToRecv = msgCount;
						//第二次通讯请求
						KRequest req;
						req.SetMsgCount(msgCount);
						req.SetDeviceNumber(conn->m_bDeviceNumber);
						bufferevent_write(bev,(char*)&req.msg,sizeof(KRequestMsg));
						//重设水位, 可以不断的接收分段的数据,然后整合起来
						bufferevent_setwatermark(bev,EV_READ,0,0);
					}
					else //第一次通讯的结果: 无数据 无需第二次通讯
					{
						OutputDebugString("第一次通讯的结果: 无数据 无需第二次通讯");
						conn->m_dwStatus = S_DONE;
						event_base_loopexit(conn->m_base, NULL);
					}
				}
				else
				{
					OutputDebugString("第一次通讯的返回包不正确,断开连接");
					conn->m_dwStatus = S_ERROR;
					conn->m_errorMsg = "第一次通讯的返回包不正确";
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
				tmp.Format("接收长度 = %u\r\n",len);
				OutputDebugString(tmp);

				if(conn->m_buffer.GetSize()==conn->m_dwMsgCountToRecv*RESPONSE_SIZE)
				{
					//已接收全部数据,分析包
					DWORD length = conn->m_buffer.GetSize();
					byte *p = (byte*)conn->m_buffer.GetData();
					for(int i=0;i<length;i++)
					{
						if(p[i]==0xFC && i+RESPONSE_SIZE<=length && p[i+RESPONSE_SIZE-1]==0xFD)
						{
							conn->m_data.Append((const char *)(p+i),RESPONSE_SIZE);
							i += RESPONSE_SIZE-1;	//i++
						}
						else if(i+RESPONSE_SIZE<=length) //正常应该是一个包挨着一个包的
						{
							conn->m_errorMsg.Format("拆包过程异常,index=%d, 需要接收的数据条数 %d\r\n",i,conn->m_dwMsgCountToRecv);
						}
					}
					OutputDebugString("已接收全部数据退出\r\n");
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
// 			if(conn->m_dwStatus==S_START) //第一次接收数据
// 			{
// 				conn->m_dwStatus = S_FIRST_RECV;
// 
// 				DWORD msgCount = res.GetMsgCount();
// 				if(msgCount>0) //第一次通讯的结果: 有msgCount条数据
// 				{
// 					conn->m_dwMsgCountToRecv = msgCount;
// // 					CString tmp;
// // 					tmp.Format("第一次通讯数据,需要接收的数据条数 %d\n",msgCount);
// // 					OutputDebugString(tmp);
// 					//第二次通讯请求
// 					KRequest req;
// 					req.SetMsgCount(msgCount);
// 					req.SetDeviceNumber(conn->m_bDeviceNumber);
// 					bufferevent_write(bev,(char*)&req.msg,sizeof(KRequestMsg));
// 					//重设水位, 当数据满时再一次性接收
// 					//bufferevent_setwatermark(bev,EV_READ,RESPONSE_SIZE*msgCount,0);
// 				}
// 				else //第一次通讯的结果: 无数据 无需第二次通讯
// 				{
// 					OutputDebugString("第一次通讯的结果: 无数据 无需第二次通讯");
// 					event_base_loopexit(conn->m_base, NULL);
// 				}
// 			}
// 			else if(conn->m_dwStatus==S_FIRST_RECV || conn->m_dwStatus==S_SECOND_RECV) //第二次接收数据
// 			{
// 				conn->m_dwMsgCountToRecv--;
// 				conn->m_dwStatus = S_SECOND_RECV;
// 				CString tmp;
// 				tmp.Format("第二次通讯数据,需要接收的数据条数 %d\n",conn->m_dwMsgCountToRecv);
// 				OutputDebugString(tmp);
// 				if(conn->m_dwMsgCountToRecv==0) //接收完毕, 退出
// 				{
// 					OutputDebugString("接收完毕, 退出");
// 					event_base_loopexit(conn->m_base, NULL);
// 				}
// 			}
// 		}
// 		else
// 		{
// 			//数据包头尾不对 丢弃此包
// 			conn->m_dwMsgCountToRecv--;
// 			char msg[46*2+1] = {0};
// 			Data2HexString(msg,(const char *)&res.msg,len);
// 			CString tmp;
// 			tmp.Format("数据包头尾不对 丢弃此包,需要接收的数据条数 %d, data=%s\r\n",conn->m_dwMsgCountToRecv,msg);
// 			WriteLog(conn,tmp.GetBuffer(),tmp.GetLength());
// // 			OutputDebugString(tmp);
// 		}
// 	}
// 	else if(len<RESPONSE_SIZE)
// 	{
// 		//数据长度<46字节 丢弃此包
// 		conn->m_dwMsgCountToRecv--;
// 		byte *pBuffer = new byte[len];
// 		bufferevent_read(bev,pBuffer,len);
// 		char msg[46*2] = {0};
// 		Data2HexString(msg,(const char *)pBuffer,len);
// 
// 		CString tmp;
// 		tmp.Format("数据长度<46字节 丢弃此包,需要接收的数据条数 %d, data=%s\r\n",conn->m_dwMsgCountToRecv,msg);
// 		WriteLog(conn,tmp.GetBuffer(),tmp.GetLength());
// 
// 		delete [] pBuffer;
// // 		OutputDebugString(tmp);
// 	}
// 	else if(len>RESPONSE_SIZE)
// 	{
// 		//数据长度>RESPONSE_SIZE, 需要拆包
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
// 				tmp.Format("拆包过程异常,index=%d, 需要接收的数据条数 %d\r\n",i,conn->m_dwMsgCountToRecv);
// 				WriteLog(conn,tmp.GetBuffer(),tmp.GetLength());
// 			}
// 		}
// 		if(conn->m_dwMsgCountToRecv==0) //接收完毕, 退出 PS:如果中间有数据包错误, 则m_dwMsgCountToRecv>0,只有等超时才能退出
// 		{
// 			OutputDebugString("多条消息接收完毕, 退出");
// 			event_base_loopexit(conn->m_base, NULL);
// 		}
// 		if(conn->m_dwMsgCountToRecv<0)
// 		{
// 			CString tmp;
// 			tmp.Format("丢包异常m_dwMsgCountToRecv<0, 需要接收的数据条数 %d\r\n",conn->m_dwMsgCountToRecv);
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
// 				conn->m_dwStatus = S_FIRST_RECV; //状态:第一次接收数据
// 
// 				DWORD msgCount = res.GetMsgCount();
// 				if(msgCount>0) //第一次通讯的结果: 有msgCount条数据
// 				{
// 					//第二次通讯请求
// 					KRequest req;
// 					req.SetMsgCount(msgCount);
// 					req.SetDeviceNumber(conn->m_bDeviceNumber);
// 					bufferevent_write(bev,(char*)&req.msg,sizeof(KRequestMsg));
// 					//重设水位, 当数据满时再一次性接收
// 					//bufferevent_setwatermark(bev,EV_READ,RESPONSE_SIZE*msgCount,0);
// 				}
// 				else //第一次通讯的结果: 无数据 无需第二次通讯
// 				{
// 					struct timeval delay = { 0, 0 };
// 					event_base_loopexit(conn->m_base, &delay);
// 				}
// 			}
// 			else if(conn->m_dwStatus==S_FIRST_RECV)
// 			{
// 				conn->m_dwStatus = S_SECOND_RECV; //状态: 第二次接收数据 (有一条数据,len==RESPONSE_SIZE)
// 				struct timeval delay = { 0, 0 };
// 				event_base_loopexit(conn->m_base, &delay);
// 			}
// 		}
// 		else	//响应包头尾不对
// 		{
// 			struct timeval delay = { 0, 0 };
// 			event_base_loopexit(conn->m_base, &delay);
// 		}
// 
// 	}
// 	else if(len>RESPONSE_SIZE) //第二次接收数据(有多条数据) 取全部消息
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
	//TODO:此功能暂不需要, 用超时取代
// 	KConnection *conn = (KConnection*)params;
// 	char str[100] = {0};
// 	sprintf(str,"%d,EndThread\n",conn->m_dwCheckCount);
// 	OutputDebugString(str);
// 	struct timeval delay = { 0, 0 };
// 	//event_base_loopexit(conn->m_base, &delay);
// 	event_base_loopbreak(conn->m_base);
}

