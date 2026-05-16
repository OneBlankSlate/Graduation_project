#pragma once
#include<iostream>
#include<tchar.h>
#include<Windows.h>
#include<TlHelp32.h>
#include<vector> 

BOOL get_thread_id(DWORD ProcessIdentify, std::vector<DWORD>& ThreadIdentifyV);


namespace _THREAD_HELPER_
{
	BOOL get_thread_id(DWORD ProcessIdentify, std::vector<DWORD>& ThreadIdentifyV) {
		HANDLE thread_snap_handle = INVALID_HANDLE_VALUE;
		THREADENTRY32 te32;
		thread_snap_handle = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		if (thread_snap_handle == INVALID_HANDLE_VALUE)  return FALSE;
		te32.dwSize = sizeof(THREADENTRY32);
		if (!Thread32First(thread_snap_handle, &te32)) {
			CloseHandle(thread_snap_handle);
			return FALSE;
		}
		do {
			if (te32.th32OwnerProcessID == ProcessIdentify) {
				ThreadIdentifyV.push_back(te32.th32ThreadID);
			}
		} while (Thread32Next(thread_snap_handle, &te32));
		CloseHandle(thread_snap_handle);
		return TRUE;
	}

}