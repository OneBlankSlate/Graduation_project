#include"DriverEntry.h"
#include"IoControlHelper.h"
#include"SystemHelper.h"
#include"CallbackHelper.h"
#include"SystemModule.h"
#include"ProcessHelper.h"
//注册表回调使用的Cookie
LARGE_INTEGER g_liRegCookie;
//   bu BlankSlateDriver!DriverEntry
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
	__debugbreak();
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS Status = STATUS_SUCCESS;
	PDEVICE_OBJECT DeviceObject;
	PDEVICE_EXTENSION DeviceExtension = NULL;
	UNICODE_STRING DeviceName;
	UNICODE_STRING SymbolicLink;
	//设置卸载函数
	DriverObject->DriverUnload = DriverUnload;

	RtlInitUnicodeString(&DeviceName, DEVICE_NAME);
	RtlInitUnicodeString(&SymbolicLink, SYMBOLIC_LINK);
	//创建设备对象
	Status = IoCreateDevice(DriverObject,  //驱动对象
		sizeof(DEVICE_EXTENSION),          //设备扩展
		&DeviceName,                       //设备名称
		FILE_DEVICE_UNKNOWN,
		0,
		TRUE,
		&DeviceObject);
	if (!NT_SUCCESS(Status))
	{
		return Status;
	}

	DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	DeviceExtension->DeviceObject = DeviceObject;
	DeviceExtension->DeviceName = DeviceName;
	DeviceExtension->SymbolicLink = SymbolicLink;

	Status = IoCreateSymbolicLink(&SymbolicLink, &DeviceName);
	if (!NT_SUCCESS(Status))
	{
		IoDeleteDevice(DeviceObject);
		return Status;
	}
	for (int i = 0; i < ARRAYSIZE(DriverObject->MajorFunction); ++i)
	{
		DriverObject->MajorFunction[i] = DispatchRoutine;
	}
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IoControlRoutine;
	//必要的初始化行为
	InitializeSystemSource();
	InitializeCallbackSource(DriverObject);
	GetDriverObject(DriverObject);
	DbgPrint("[wdk]DriverEntry");

	return STATUS_SUCCESS;
}
VOID DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
	PDEVICE_OBJECT DeviceObject = NULL;
	PDEVICE_EXTENSION DeviceExtension = NULL;
	DeviceObject = DriverObject->DeviceObject;
	while (DeviceObject != NULL)
	{
		DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
		//删除符号链接
		UNICODE_STRING SymbolicLink = DeviceExtension->SymbolicLink;
		IoDeleteSymbolicLink(&SymbolicLink);
		DeviceObject = DeviceObject->NextDevice;
		IoDeleteDevice(DeviceExtension->DeviceObject);

	}
	//必要的Uninitialize
	UninitializeSystemSource();
	UninitializeCallbackSource();


}
NTSTATUS DispatchRoutine(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	//KdPrint(("Enter HelloDDKDispatchRoutine\n"));
	PIO_STACK_LOCATION IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
	//建立一个字符串数组与IRP类型对应起来
	static char* v1[] =
	{
		"IRP_MJ_CREATE",
		"IRP_MJ_CREATE_NAMED_PIPE",
		"IRP_MJ_CLOSE",
		"IRP_MJ_READ",
		"IRP_MJ_WRITE",
		"IRP_MJ_QUERY_INFORMATION",
		"IRP_MJ_SET_INFORMATION",
		"IRP_MJ_QUERY_EA",
		"IRP_MJ_SET_EA",
		"IRP_MJ_FLUSH_BUFFERS",
		"IRP_MJ_QUERY_VOLUME_INFORMATION",
		"IRP_MJ_SET_VOLUME_INFORMATION",
		"IRP_MJ_DIRECTORY_CONTROL",
		"IRP_MJ_FILE_SYSTEM_CONTROL",
		"IRP_MJ_DEVICE_CONTROL",
		"IRP_MJ_INTERNAL_DEVICE_CONTROL",
		"IRP_MJ_SHUTDOWN",
		"IRP_MJ_LOCK_CONTROL",
		"IRP_MJ_CLEANUP",
		"IRP_MJ_CREATE_MAILSLOT",
		"IRP_MJ_QUERY_SECURITY",
		"IRP_MJ_SET_SECURITY",
		"IRP_MJ_POWER",
		"IRP_MJ_SYSTEM_CONTROL",
		"IRP_MJ_DEVICE_CHANGE",
		"IRP_MJ_QUERY_QUOTA",
		"IRP_MJ_SET_QUOTA",
		"IRP_MJ_PNP",
	};
	UCHAR MajorFunction = IoStackLocation->MajorFunction;
	if (MajorFunction >= ARRAYSIZE(v1))
	{

	}
	else
	{

	}
	NTSTATUS Status = STATUS_SUCCESS;
	//完成IRP
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Status;

}
NTSTATUS IoControlRoutine(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	PDEVICE_EXTENSION DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	NTSTATUS Status = STATUS_SUCCESS;
	PIO_STACK_LOCATION IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
	//得到输入缓冲区
	PVOID InputBuffer = Irp->AssociatedIrp.SystemBuffer;
	ULONG InputBufferLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
	//得到输出缓冲区
	PVOID OutputBuffer = Irp->AssociatedIrp.SystemBuffer;
	ULONG OutputBufferLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
	//得到IOCTL码
	ULONG Code = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

	ULONG Information = 0;
	switch (Code)
	{
	case MY_CTL_CODE:
	{
		PVOID OutputBuffer = Irp->UserBuffer;
		ULONG OutputBufferLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
		PVOID InputBuffer = IoStackLocation->Parameters.DeviceIoControl.Type3InputBuffer;
		ULONG InputBufferLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
		ULONG ReturnValue = 0;
		Status = CommunicateNeitherControl(InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength, &ReturnValue);
		Information = ReturnValue;
		break;
	}

	default:
	{
		Irp->IoStatus.Information = 0;
		break;
	}

	}
	//完成Irp
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = Information;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Status;
}