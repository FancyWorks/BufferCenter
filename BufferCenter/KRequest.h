#pragma once
#include "windows.h"

struct KRequestMsg 
{
	byte head;
	byte deviceNumber;
	byte msgCount[3];
	byte positionCode[3];
	byte data[3];
	byte tail;
};
class KRequest
{
public:
	KRequest(void);
	~KRequest(void);
	DWORD GetMsgCount();
	void SetMsgCount(DWORD msgCount);
	void SetDeviceNumber(byte deviceNumber);
public:
	KRequestMsg msg;
};
