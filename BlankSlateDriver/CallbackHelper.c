#include"CallbackHelper.h"
#include"ProcessHelper.h"
#include"ObjectHelper.h"
PVOID __ProcessCallbackHandle = NULL;
PEPROCESS __ProtectedProcess = NULL;
OB_OPERATION_REGISTRATION __OperationRegistrations = { 0 };
UNICODE_STRING __Altitude = { 0 };
OB_CALLBACK_REGISTRATION  __ObRegistration = { 0 };
TD_CALLBACK_REGISTRATION __CallbackRegistration = { 0 };
void InitializeCallbackSource(PDRIVER_OBJECT DriverObject)
{
	// 以下代码放在DriverEntry中  用来绕过 调用ObRegisterCallback时 进行的签名校验
	ULONG_PTR pDrvSection = (ULONG_PTR)DriverObject->DriverSection;
	*(PULONG)(pDrvSection + 0x68) |= 0x20;
}
NTSTATUS PsProtectProcess(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PCOMMUNICATE_PROTECT_PROCESS v1 = (PCOMMUNICATE_PROTECT_PROCESS)InputBuffer;
	HANDLE ProcessIdentity = v1->ProcessIdentitys;
	PsLookupProcessByProcessId(ProcessIdentity, &__ProtectedProcess);
	//KeAcquireGuardedMutex(&__CallbacksMutex);

	// Setup the Ob Registration calls

	__OperationRegistrations.ObjectType = PsProcessType;
	__OperationRegistrations.Operations |= OB_OPERATION_HANDLE_CREATE;
	__OperationRegistrations.Operations |= OB_OPERATION_HANDLE_DUPLICATE;
	__OperationRegistrations.PreOperation = PreOperationCallback;
	__OperationRegistrations.PostOperation = NULL;



	RtlInitUnicodeString(&__Altitude, L"1000");

	__ObRegistration.Version = OB_FLT_REGISTRATION_VERSION;
	__ObRegistration.OperationRegistrationCount = 1;
	__ObRegistration.Altitude = __Altitude;
	__ObRegistration.RegistrationContext = &__CallbackRegistration;
	__ObRegistration.OperationRegistration = &__OperationRegistrations;

	// save the registration handle to remove callbacks later
	Status = ObRegisterCallbacks(&__ObRegistration, &__ProcessCallbackHandle);

	if (!NT_SUCCESS(Status))
	{
		DbgPrint("Fail:%d", Status);
		//KeReleaseGuardedMutex(&__CallbacksMutex); // Release the lock before exit
		return STATUS_UNSUCCESSFUL;
	}
	DbgPrint("Success:%d", Status);
	//KeReleaseGuardedMutex(&__CallbacksMutex);
	return Status;
}
OB_PREOP_CALLBACK_STATUS PreOperationCallback(_In_ PVOID RegistrationContext, _Inout_ POB_PRE_OPERATION_INFORMATION PreInfo)
{
	PTD_CALLBACK_REGISTRATION CallbackRegistration;
	ACCESS_MASK AccessBitsToClear = 0;
	ACCESS_MASK AccessBitsToSet = 0;
	ACCESS_MASK InitialDesiredAccess = 0;
	ACCESS_MASK OriginalDesiredAccess = 0;
	PACCESS_MASK DesiredAccess = NULL;
	LPCWSTR ObjectTypeName = NULL;
	LPCWSTR OperationName = NULL;

	// Not using driver specific values at this time
	CallbackRegistration = (PTD_CALLBACK_REGISTRATION)RegistrationContext;



	// Only want to filter attempts to access protected process
	// all other processes are left untouched

	if (PreInfo->ObjectType == *PsProcessType) {
		//
		// Ignore requests for processes other than our target process.
		//

		// if (TdProtectedTargetProcess != NULL &&
		//    TdProtectedTargetProcess != PreInfo->Object)
		if (__ProtectedProcess != PreInfo->Object)   // 检查是否为非保护进程操作
		{
			goto Exit;
		}

		//
		// Also ignore requests that are trying to open/duplicate the current
		// process.
		//

		if (PreInfo->Object == PsGetCurrentProcess()) {   //检查是否为进程自身的操作，禁止当前进程通过自身操作绕过保护。
			DbgPrintEx(
				DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL,
				"ObCallbackTest: CBTdPreOperationCallback: ignore process open/duplicate from the protected process itself\n");
			goto Exit;
		}

		ObjectTypeName = L"PsProcessType";
		AccessBitsToClear = CB_PROCESS_TERMINATE;  //清除进程的终止权限
		AccessBitsToSet = 0;
	}
	else {
		DbgPrintEx(
			DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
			"ObCallbackTest: CBTdPreOperationCallback: unexpected object type\n");
		goto Exit;
	}

	switch (PreInfo->Operation) {
	case OB_OPERATION_HANDLE_CREATE:
		DesiredAccess = &PreInfo->Parameters->CreateHandleInformation.DesiredAccess;
		OriginalDesiredAccess = PreInfo->Parameters->CreateHandleInformation.OriginalDesiredAccess;

		OperationName = L"OB_OPERATION_HANDLE_CREATE";
		break;

	case OB_OPERATION_HANDLE_DUPLICATE:
		DesiredAccess = &PreInfo->Parameters->DuplicateHandleInformation.DesiredAccess;
		OriginalDesiredAccess = PreInfo->Parameters->DuplicateHandleInformation.OriginalDesiredAccess;

		OperationName = L"OB_OPERATION_HANDLE_DUPLICATE";
		break;

	default:
		TD_ASSERT(FALSE);
		break;
	}

	InitialDesiredAccess = *DesiredAccess;

	// Filter only if request made outside of the kernel
	if (PreInfo->KernelHandle != 1) {     // 仅处理非内核句柄操作
		*DesiredAccess &= ~AccessBitsToClear;   // 清除危险权限位（如终止权限）
		//*DesiredAccess &= ~0x20;                //清除内存写的权限
		//*DesiredAccess &= ~0x10;                //清除内促读权限
		//*DesiredAccess &= ~0x80;                //清除进程创建权限
		*DesiredAccess |= AccessBitsToSet;     // 可选的权限添加（此处未使用）
	}
	//可以通过此方法去除下方任意进程句柄权限！
	//#define PROCESS_TERMINATE                  (0x0001)  终止权限
	//#define PROCESS_CREATE_THREAD              (0x0002)  创建线程权限
	//#define PROCESS_SET_SESSIONID              (0x0004)  
	//#define PROCESS_VM_OPERATION               (0x0008)  虚拟内存操作权限
	//#define PROCESS_VM_READ                    (0x0010)  虚拟内存读权限
	//#define PROCESS_VM_WRITE                   (0x0020)  虚拟内存写权限
	//#define PROCESS_DUP_HANDLE                 (0x0040)  复制句柄权限
	//#define PROCESS_CREATE_PROCESS             (0x0080)  创建进程权限
	//#define PROCESS_SET_QUOTA                  (0x0100)  
	//#define PROCESS_SET_INFORMATION            (0x0200)  
	//#define PROCESS_QUERY_INFORMATION          (0x0400)  QueryInformation权限
	//#define PROCESS_SUSPEND_RESUME             (0x0800)  
	//#define PROCESS_QUERY_LIMITED_INFORMATION  (0x1000)  
	//#define PROCESS_SET_LIMITED_INFORMATION    (0x2000) 

	//
	// Set call context.
	//
	TdSetCallContext(PreInfo, CallbackRegistration);


Exit:

	return OB_PREOP_SUCCESS;
}
NTSTATUS PsUnprotectProcess(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	return status;
}
void TdSetCallContext(_Inout_ POB_PRE_OPERATION_INFORMATION PreInfo, _In_ PTD_CALLBACK_REGISTRATION CallbackRegistration)
{
	PTD_CALL_CONTEXT CallContext;

	CallContext = (PTD_CALL_CONTEXT)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(TD_CALL_CONTEXT), TD_CALL_CONTEXT_TAG);

	if (CallContext == NULL)
	{
		return;
	}

	CallContext->CallbackRegistration = CallbackRegistration;
	CallContext->Operation = PreInfo->Operation;
	CallContext->Object = PreInfo->Object;
	CallContext->ObjectType = PreInfo->ObjectType;

	PreInfo->CallContext = CallContext;
}
void UninitializeCallbackSource()
{
	if (__ProcessCallbackHandle != NULL)
	{
		ObUnRegisterCallbacks(__ProcessCallbackHandle);
	}
}
