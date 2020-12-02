#include "StdAfx.h"
#include "KResponse.h"

KResponse::KResponse(void)
{
	msg.head = 0xFC;
	msg.deviceNumber = 0x0;
	ZeroMemory(msg.msgCount,3);
	ZeroMemory(msg.data,46-6);
	msg.tail = 0xFD;
}

KResponse::~KResponse(void)
{
}

DWORD KResponse::GetMsgCount()
{
	DWORD msgCount = 0;
	byte *p = (byte*)&msgCount;
	p[2] = msg.msgCount[0];
	p[1] = msg.msgCount[1];
	p[0] = msg.msgCount[2];
	return msgCount;
}

void KResponse::SetMsgCount(DWORD msgCount)
{
	byte *p = (byte*)&msgCount;
	msg.msgCount[0] = p[2];
	msg.msgCount[1] = p[1];
	msg.msgCount[2] = p[0];
}