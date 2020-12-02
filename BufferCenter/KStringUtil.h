#ifndef KSTRINGUTIL__H
#define KSTRINGUTIL__H

void Data2HexString(char *dest,const char*src,int length)
{
	static const char *hex = "0123456789ABCDEF";
	for(int i=0;i<length;i++)
	{
		dest[i*2] = hex[((unsigned char)src[i])>>4];
		dest[i*2+1] = hex[src[i] & 0x0F];
	}
}

#endif

