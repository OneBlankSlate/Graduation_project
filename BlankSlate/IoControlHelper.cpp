#include"IoControlHelper.h"

BOOL CommunicateDevice(PVOID InputBuffer, DWORD InputBufferSize, PVOID OutputBuffer, DWORD OutputBufferSize, DWORD* ReturnValue)
{
	BOOL IsOk = FALSE;
	DWORD ReturnLength = 0;
	HANDLE DeviceObject = CreateFile(SYMBOLIC_LINK, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
	if (DeviceObject != INVALID_HANDLE_VALUE)
	{
		if (DeviceIoControl(DeviceObject, MY_CTL_CODE, InputBuffer, InputBufferSize, OutputBuffer, OutputBufferSize, &ReturnLength, NULL))
		{
			IsOk = TRUE;
		}
		CloseHandle(DeviceObject);
		DeviceObject = INVALID_HANDLE_VALUE;
	}
	return IsOk;
}