#pragma once
#include<iostream>
#include<tchar.h>
#include<Windows.h>
#include<TlHelp32.h>
#include<vector> 

//DWORD get_processid_by_imagename(const TCHAR* ProcessImageName);


namespace _PROCESS_HELPER_
{

	DWORD get_processid_by_imagename(const TCHAR* ProcessImageName)
	{
		DWORD process_identity = 0;
		HANDLE snap_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snap_handle == INVALID_HANDLE_VALUE)
		{
			_tprintf(_T("CreateToolhelp Failed!ErrorCode:%s"), GetLastError());
			return 0;
		}
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);

		if (!Process32First(snap_handle, &pe32))
		{
			_tprintf(_T("Process32First Failed!ErrorCode:%s"), GetLastError());
			return 0;
		}
		do {
			if (_wcsicmp(pe32.szExeFile, ProcessImageName) == 0) {    //两个变量相等时_wcsicmp返回0
				process_identity = pe32.th32ProcessID;
				break;
			}
		} while (Process32Next(snap_handle, &pe32));

		CloseHandle(snap_handle);

		return process_identity;
	}



}