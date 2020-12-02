// KData.h: interface for the KData class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_KDATA_H__6AFC3CD8_450E_48F1_A311_8AB599A5CDA0__INCLUDED_)
#define AFX_KDATA_H__6AFC3CD8_450E_48F1_A311_8AB599A5CDA0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#include <windows.h>

class KData  
{
public:
	KData();
	virtual ~KData();
	void Append(const char *_data);
	void Append(const char *_data, int _size);
	void Append(KData *_data);
	void Clear();
	char* GetData(){return data;}
	int GetSize(){return size;}

	operator char* (){return data;}
protected:
	char *data;
	int size;
	DWORD memsize;		//分配的内存空间, 任意时刻, size<=memsize
};

#endif // !defined(AFX_KDATA_H__6AFC3CD8_450E_48F1_A311_8AB599A5CDA0__INCLUDED_)
