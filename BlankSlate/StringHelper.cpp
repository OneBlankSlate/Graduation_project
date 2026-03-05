#include"StringHelper.h"


char* Wchar2Char(WCHAR* wstr)
{
	/*
	CP_ACP：使用系统默认的ANSI编码。
	CP_UTF8：UTF-8编码。
	CP_OEMCP：OEM字符集。
	*/
	char* str = NULL;
	ULONG Length = 0;
	if (wstr != NULL)
	{
		Length = WideCharToMultiByte(CP_ACP, NULL, wstr, -1, NULL, 0, NULL, FALSE);
		str = (char*)malloc(Length + 1);
		if (str == NULL)
		{
			return NULL;
		}
		memset(str, 0, Length + 1);
		WideCharToMultiByte(CP_OEMCP, NULL, wstr, -1, str, Length, NULL, FALSE);
	}
	return str;
}
char* Char2Char(CHAR* SourceStr)
{
	char* str = NULL;
	ULONG Length = 0;
	if (SourceStr != NULL)
	{
		Length = strlen(SourceStr);
		str = (char*)malloc(Length + 1);
		if (str == NULL)
		{
			return NULL;
		}
		memset(str, 0, Length + 1);
		memcpy(str, SourceStr, Length);
	}
	return str;
}
wchar_t* Char2Wchar(CHAR* str)
{
	wchar_t* wstr = NULL;
	ULONG Length = 0;

	if (str != NULL)
	{
		Length = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
		wstr = (wchar_t*)malloc((Length + 1) * sizeof(wchar_t));
		if (wstr == NULL)
		{
			return NULL;
		}
		// 执行转换
		int result = MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, Length);
		if (result == 0)
		{
			return NULL;
		}
	}
	return wstr;


}