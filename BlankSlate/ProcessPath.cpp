#include"ProcessPath.h"
#include"ProcessHelper.h"
#include"IoControlHelper.h"
BOOL GetProcessPath()
{

	PROCESS_PATH_REQUEST ProcessPathRequest = { 0 };
	ProcessPathRequest.ProcessIdentity = GetProcessIdentity(_T("Dbgview.exe"));
	//通过设备链接名打开设备对象 获得设备对象句柄
	HANDLE DeviceHandle = CreateFile(SYMBOLIC_LINK,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (DeviceHandle == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	DWORD ReturnLength = 0;
	BOOL IsOk = DeviceIoControl(DeviceHandle,CTL_PROCESS_PATH,
		&ProcessPathRequest,
		sizeof(PROCESS_PATH_REQUEST),
		&ProcessPathRequest,
		sizeof(PROCESS_PATH_REQUEST),
		&ReturnLength,
		NULL);
	if (IsOk == FALSE)
	{
		int LastError = GetLastError();
		_tprintf(_T("LastError=%d\r\n"), LastError);
		return FALSE;
	}
	else if (IsOk == TRUE)
	{
		wprintf(L"完整路径：%s\r\n", ProcessPathRequest.ProcessPath);
		return TRUE;
	}
	if (DeviceHandle != NULL)
	{
		CloseHandle(DeviceHandle);
		DeviceHandle = NULL;
	}

}
