#pragma once
#include<tchar.h>
#include<Windows.h>
#include<iostream>
using namespace std;



typedef struct _PROCESS_PATH_REQUEST_
{
	HANDLE ProcessIdentity;
	WCHAR ProcessPath[MAX_PATH];
}PROCESS_PATH_REQUEST, * PPROCESS_PATH_REQUEST;

BOOL GetProcessPath();