#pragma once
#include "windows.h"

struct KReponseMsg {
	byte head;
	byte deviceNumber;
	byte msgCount[3];
	byte data[46-6];
	byte tail;
};

class KResponse
{
public:
	KResponse(void);
	~KResponse(void);
	DWORD GetMsgCount();
	void SetMsgCount(DWORD msgCount);
public:
	KReponseMsg msg;
};
