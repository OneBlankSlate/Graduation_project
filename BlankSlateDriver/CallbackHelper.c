#include"CallbackHelper.h"
#include"ProcessHelper.h"
#include"ObjectHelper.h"
PVOID __FileCallbackHandle = NULL;
PVOID __ProcessCallbackHandle = NULL;
void InitializeCallbackSource(PDRIVER_OBJECT DriverObject)
{
	PLDR_DATA_TABLE_ENTRY LdrDataTableEntry;
	LdrDataTableEntry = (PLDR_DATA_TABLE_ENTRY)DriverObject->DriverSection;
	LdrDataTableEntry->Flags |= 0x20;
	FileObjectCallback();
	ProcessObjectCallback();

}
NTSTATUS FileObjectCallback()
{
	NTSTATUS Status;
	OB_CALLBACK_REGISTRATION ObCallbackRegistration;
	OB_OPERATION_REGISTRATION ObOperationRegistration;
	//通过文件对象类型进行回调设置
	POBJECT_TYPE_1 v1 = (POBJECT_TYPE_1)(*IoFileObjectType);
	v1->TypeInfo.SupportsObjectCallbacks = 1;
	RtlZeroMemory(&ObCallbackRegistration, sizeof(OB_CALLBACK_REGISTRATION));
	ObCallbackRegistration.Version = ObGetFilterVersion();
	ObCallbackRegistration.OperationRegistrationCount = 1;
	ObCallbackRegistration.RegistrationContext = NULL;
	RtlInitUnicodeString(&ObCallbackRegistration.Altitude, L"File");
	RtlZeroMemory(&ObOperationRegistration,sizeof(OB_OPERATION_REGISTRATION));
	ObOperationRegistration.ObjectType = IoFileObjectType;
	ObOperationRegistration.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
	ObOperationRegistration.PreOperation = (POB_PRE_OPERATION_CALLBACK)&FileObjectPrevious;    //在这里注册一个回调函数指针
	ObOperationRegistration.PostOperation = NULL;
	//关联两个结构体
	ObCallbackRegistration.OperationRegistration = &ObOperationRegistration;  //注意
	Status = ObRegisterCallbacks(&ObCallbackRegistration, &__FileCallbackHandle);
	if (!NT_SUCCESS(Status))
	{
		Status = STATUS_UNSUCCESSFUL;
	}
	else
	{
		Status = STATUS_SUCCESS;
	}
	return Status;
}
NTSTATUS ProcessObjectCallback()
{
	NTSTATUS Status;
	OB_CALLBACK_REGISTRATION ObCallbackRegistration;   //回调注册结构
	OB_OPERATION_REGISTRATION ObOperationRegistration; //操作注册结构
	//通过文件对象类型进行回调设置
	POBJECT_TYPE_1 v1 = (POBJECT_TYPE_1)(*PsProcessType);   //进程对象类型
	v1->TypeInfo.SupportsObjectCallbacks = 1;   //将SupportsObjectCallbacks设置为1，这样允许对该类型的对象设置回调
	RtlZeroMemory(&ObCallbackRegistration,sizeof(OB_CALLBACK_REGISTRATION));
	ObCallbackRegistration.Version = ObGetFilterVersion();
	ObCallbackRegistration.OperationRegistrationCount = 1;
	ObCallbackRegistration.RegistrationContext = NULL;
	RtlInitUnicodeString(&ObCallbackRegistration.Altitude, L"Process");
	RtlZeroMemory(&ObOperationRegistration,sizeof(OB_OPERATION_REGISTRATION));
	ObOperationRegistration.ObjectType = PsProcessType;  //进程对象类型
	ObOperationRegistration.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
	ObOperationRegistration.PreOperation = (POB_PRE_OPERATION_CALLBACK)&ProcessObjectPrevious;    //在这里注册一个回调函数指针  前置操作
	ObOperationRegistration.PostOperation = NULL;
	//关联这两个结构体
	ObCallbackRegistration.OperationRegistration = &ObOperationRegistration;  //注意
	/*
	ObRegisterCallbacks用于注册全局回调，这里针对进程对象的句柄创建和复制操作，
	当有进程句柄被创建或复制时，回调函数会被调用，从而可以修改访问权限，特别是禁止终止进程的权限。
	*/
	Status = ObRegisterCallbacks(&ObCallbackRegistration, &__ProcessCallbackHandle);//需要注意的是，ObRegisterCallbacks返回的句柄需要保存，并在驱动卸载时调用ObUnRegisterCallbacks来注销回调，否则可能导致系统不稳定。
	if (!NT_SUCCESS(Status))
	{
		Status = STATUS_UNSUCCESSFUL;
	}
	else
	{
		Status = STATUS_SUCCESS;
	}
	return Status;

}

OB_PREOP_CALLBACK_STATUS FileObjectPrevious(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION ObPreviousOperationInfo)
{
	UNICODE_STRING v1;
	POBJECT_NAME_INFORMATION ObjectNameInfo;
	PFILE_OBJECT FileObject = (PFILE_OBJECT)ObPreviousOperationInfo->Object;
	HANDLE ProcessIdentity = PsGetCurrentProcessId();
	if (ObPreviousOperationInfo->ObjectType != *IoFileObjectType)
	{
		return OB_PREOP_SUCCESS;
	}
	//过滤无效指针
	if (FileObject->FileName.Buffer == NULL || !MmIsAddressValid(FileObject->FileName.Buffer) || FileObject->DeviceObject == NULL || !MmIsAddressValid(FileObject->DeviceObject))
	{
		return OB_PREOP_SUCCESS;
	}
	if (!NT_SUCCESS(IoQueryFileDosDeviceName((PFILE_OBJECT)FileObject, &ObjectNameInfo)))
	{
		return OB_PREOP_SUCCESS;
	}
	if (ObjectNameInfo->Name.Buffer == NULL || ObjectNameInfo->Name.Length == 0)
	{
		ExFreePool(ObjectNameInfo);
		return OB_PREOP_SUCCESS;
	}
	if (wcsstr(ObjectNameInfo->Name.Buffer, L"D:\\Shine.txt") || wcsstr(ObjectNameInfo->Name.Buffer, L"C:\\Shine.txt"))
	{
		if (FileObject->DeleteAccess == TRUE || FileObject->WriteAccess == TRUE)
		{
			if (ObPreviousOperationInfo->Operation == OB_OPERATION_HANDLE_CREATE)
			{
				ObPreviousOperationInfo->Parameters->CreateHandleInformation.DesiredAccess = 0;
			}
			if (ObPreviousOperationInfo->Operation == OB_OPERATION_HANDLE_DUPLICATE)
			{
				ObPreviousOperationInfo->Parameters->DuplicateHandleInformation.DesiredAccess = 0;
			}
		}
	}
	RtlVolumeDeviceToDosName(FileObject->DeviceObject, &v1);
	DbgPrint("ProcessIdentity:%d  File:%wZ  %wZ\r\n", ProcessIdentity, &v1, &ObjectNameInfo->Name);
	ExFreePool(ObjectNameInfo);
	return OB_PREOP_SUCCESS;

}

OB_PREOP_CALLBACK_STATUS ProcessObjectPrevious(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION ObPreviousOperationInfo)
{
	if (ObPreviousOperationInfo->KernelHandle)  //如果是内核句柄，则直接返回成功
	{
		return OB_PREOP_SUCCESS;
	}
	PEPROCESS EProcess = (PEPROCESS)ObPreviousOperationInfo->Object;
	HANDLE ProcessIdentity = PsGetProcessId(EProcess);
	if (IsProcessIdentityExist(ProcessIdentity))
	{
		//found in list,remove terminate access
		ObPreviousOperationInfo->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;  //移除PROCESS_TERMINATE的访问权限，这样该进程将无法被终止

	}
	return OB_PREOP_SUCCESS;
}

void UninitializeCallbackSource()
{
	if (__FileCallbackHandle != NULL)
	{
		ObUnRegisterCallbacks(__FileCallbackHandle);
	}
	if (__ProcessCallbackHandle != NULL)
	{
		ObUnRegisterCallbacks(__ProcessCallbackHandle);
	}
}

//CmRegisterCallback回调通信
NTSTATUS RegistryCallback(__in PVOID  CallbackContext, __in_opt PVOID  Argument1, __in_opt PVOID  Argument2)
{

	NTSTATUS status = STATUS_SUCCESS;
	UNICODE_STRING uStrRegPath = { 0 };    //保存注册表完整路径
	// 保存操作码的类型 
	long uOpCode = (long)Argument1;
	PREG_SET_VALUE_KEY_INFORMATION SetKeyInfo = (PREG_SET_VALUE_KEY_INFORMATION)Argument2;
	switch (uOpCode)
	{
		// 打开注册表之前
	case RegNtSetValueKey:
	{
		if (SetKeyInfo->Type == 31101 && SetKeyInfo->DataSize == sizeof(My_Data))
		{
			PMy_Data data = (PMy_Data)SetKeyInfo->Data;
			DbgPrint("[wdk]回调通信成功");
			DbgPrint("[wdk]%d", data->key);
			return STATUS_SUCCESS;
		}
		break;
	}

	default:
	{
		break;
	}
	}

	return status;
}
void RegistryCallbackUnload()
{
	NTSTATUS Status = STATUS_SUCCESS;
	if (g_liRegCookie.QuadPart > 0)
	{
		Status = CmUnRegisterCallback(g_liRegCookie);
		if (!NT_SUCCESS(Status))
		{
			DbgPrint("删除回调函数失败0x%X\r\n", Status);
		}
	}
	else
	{
		DbgPrint("删除回调函数成功\r\n");
	}
	DbgPrint("驱动卸载完成\r\n");
}