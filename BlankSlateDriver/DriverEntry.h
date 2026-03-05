#pragma once
#include<fltKernel.h>
#define MAX_PATH 260

extern LARGE_INTEGER g_liRegCookie;

typedef struct _INJECT_INFORMATION_
{
	HANDLE ProcessIdentity;
	WCHAR InjectDll[MAX_PATH];
}INJECT_INFORMATION, * PINJECT_INFORMATION;

typedef struct _DEVICE_EXTENSION {
	PDEVICE_OBJECT DeviceObject;
	UNICODE_STRING DeviceName;
	UNICODE_STRING SymbolicLink;
}DEVICE_EXTENSION, * PDEVICE_EXTENSION;

VOID DriverUnload(IN PDRIVER_OBJECT DriverObject);
NTSTATUS DispatchRoutine(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS IoControlRoutine(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);


