#pragma once
#include<fltKernel.h>
#include"DriverEntry.h"


void InitializeCallbackSource(PDRIVER_OBJECT DriverObject);

OB_PREOP_CALLBACK_STATUS FileObjectPrevious(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION ObPreviousOperationInfo);
OB_PREOP_CALLBACK_STATUS ProcessObjectPrevious(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION ObPreviousOperationInfo);
void UninitializeCallbackSource();
NTSTATUS FileObjectCallback();
NTSTATUS ProcessObjectCallback();

//CmRegisterCallback回调通信
typedef struct My_Data  //通信测试数据结构
{
	int key;

}My_Data, * PMy_Data;
NTSTATUS RegistryCallback(__in PVOID  CallbackContext, __in_opt PVOID  Argument1, __in_opt PVOID  Argument2);
void RegistryCallbackUnload();