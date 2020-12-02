// KData.cpp: implementation of the KData class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "KData.h"
#include <stdio.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

KData::KData()
{
	size = 0;
	memsize = 1024;
	data = new char[memsize];
}

KData::~KData()
{
	delete [] data;
	data = NULL;
}

void KData::Append(const char *_data)
{
	if(_data!=NULL)
	{
		Append(_data, strlen(_data));
	}
	else
		;////KLog::error("KData-->Append-->param is NULL");
}

void KData::Append(const char *_data, int _size)
{
	if(_data!=NULL && _size>0)
	{
		char str[100]={0};
		sprintf(str,"Append %d,   %d = %d+%d\n",memsize,_size+size,size,_size);
		OutputDebugStringA(str);

		if(_size+size<memsize)
		{
			memcpy(data+size,_data,_size);
			size += _size;
		}
		else
		{
			char *oldData = data;				//临时指针,用于释放原数据
			memsize = (size + _size)*2;		//连接后的数据大小
			data = new char[memsize+1];			//申请新的空间
			memset(data,0,memsize+1);
			if(size>0)
				memcpy(data,oldData,size);			//复制数据
			memcpy(data+size,_data,_size);
			//data[size + _size] = '\0';
			size += _size;
			delete [] oldData;
			oldData = NULL;
		}
	}
	else
		;////KLog::warning("KData-->Append-->param is illegal");
}
void KData::Append(KData *_data)
{
	if(_data!=NULL)
	{
		Append(_data->GetData(), _data->GetSize());
	}
	else
		;////KLog::error("KData-->Append-->param is NULL");
}

void KData::Clear()
{
	ZeroMemory(data,memsize);
	size=0;
}
