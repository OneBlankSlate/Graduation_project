#pragma once
#include<iostream>
#include<tchar.h>
#include<Windows.h>
#include<TlHelp32.h>
#include<vector> 
#include"PeHelper.h"
#include"ThreadHelper.h"
#include"Common.h"
#include"ProcessHelper.h"
#include"MemoryHelper.h"
#include"RegisterHelper.h"


/*
远程线程注入
APC注入
Hook Eip注入
injector注入
反射式注入reflective注入
SetWindowHookEx注入
*/
#define REMOTE_THREAD 0
#define APC_INJECOT   1
#define HOOK_EIP      2
#define SETWINDOWHOOKEX  3
#define REGISTER_INJECT  4
void inject(char* ImageName, char* flag, const wchar_t* DllPath);

void create_remote_thread(HANDLE ProcessHandle, DWORD ProcessIdentity, const wchar_t* DllPath);
void apc_inject(HANDLE ProcessHandle, DWORD ProcessIdentity, const wchar_t* DllPath);
void hook_eip_inject(HANDLE ProcessHandle, DWORD ProcessIdentity, const wchar_t* DllPath);
void set_window_hookex_inject(HANDLE ProcessHandle, DWORD ProcessIdentity, const wchar_t* DllPath);
BOOL register_inject(const wchar_t* DllPath);          //系统范围内的注入，无需进程句柄与ID

void release(PVOID lpAddress, HANDLE ThreadHandle, HANDLE ProcessHandle);












