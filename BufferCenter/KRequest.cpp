#include "StdAfx.h"
#include "KRequest.h"


KRequest::KRequest(void)
{
	msg.head = 0xFC;
	msg.deviceNumber = 0x0;
	ZeroMemory(msg.msgCount,3);
	ZeroMemory(msg.positionCode,3);
	ZeroMemory(msg.data,3);
	msg.tail = 0xFD;
}

KRequest::~KRequest(void)
{
}

DWORD KRequest::GetMsgCount()
{
	DWORD msgCount = 0;
	byte *p = (byte*)&msgCount;
	p[2] = msg.msgCount[0];
	p[1] = msg.msgCount[1];
	p[0] = msg.msgCount[2];
	return msgCount;
}

void KRequest::SetMsgCount(DWORD msgCount)
{
	byte *p = (byte*)&msgCount;
	msg.msgCount[0] = p[2];
	msg.msgCount[1] = p[1];
	msg.msgCount[2] = p[0];
}

void KRequest::SetDeviceNumber( byte deviceNumber )
{
	this->msg.deviceNumber = deviceNumber;
}
