#include"ProcessModule.h"
#include"ProcessHelper.h"
#include"MemoryHelper.h"
#include"StringHelper.h"
#include"SystemHelper.h"
#include"ProcessThread.h"
LPFN_NTUNMAPVIEWOFSECTION __NtUnmapViewOfSection = NULL;
LPFN_NTCLOSE __NtClose = NULL;
PVOID GetUserModuleHandle(IN PEPROCESS EProcess, IN PUNICODE_STRING ModuleName, ULONG* SizeOfImage)
{
	if (EProcess == NULL)
		return NULL;
	__try
	{
		LARGE_INTEGER TimeOut = { 0 };
		TimeOut.QuadPart = -25011 * 10 * 1000;   //250ms
		PPEB Peb = PsGetProcessPeb(EProcess);
		if (!Peb)
		{
			return NULL;
		}
		for (INT i = 0; !Peb->Ldr && i < 10; i++)
		{
			KeDelayExecutionThread(KernelMode, TRUE, &TimeOut);
		}
		if (!Peb->Ldr)
		{
			return NULL;
		}
		//遍历链表
		for (PLIST_ENTRY ListEntry = Peb->Ldr->InLoadOrderModuleList.Flink; ListEntry != &Peb->Ldr->InLoadOrderModuleList; ListEntry = ListEntry->Flink)
		{
			PLDR_DATA_TABLE_ENTRY LdrDataTableEntry = CONTAINING_RECORD(ListEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
			if (RtlCompareUnicodeString(&LdrDataTableEntry->ModuleName, ModuleName, TRUE) == 0)  //第三个参数为TRUE，则比较时忽略大小写，返回0表示相等
			{
				if (SizeOfImage != NULL)
				{
					SizeOfImage = LdrDataTableEntry->ModuleSize;
				}
				return LdrDataTableEntry->ModuleBaseAddress;
			}

		}

	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{

	}
	return NULL;
}

NTSTATUS PsEnumProcessModules(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
	NTSTATUS Status1 = STATUS_UNSUCCESSFUL, Status2 = STATUS_UNSUCCESSFUL;
	PCOMMUNICATE_PROCESS_MODULE v1 = (PCOMMUNICATE_PROCESS_MODULE)InputBuffer;
	ULONG_PTR ProcessIdentity = v1->ProcessIdentity;
	PEPROCESS EProcess = NULL;
	ULONG NumberOfModule = (OutputBufferLength - sizeof(MODULES_INFORMATION)) / sizeof(MODULE_INFORMATION_ENTRY);
	//参数检查
	if (!InputBuffer || InputBufferLength != sizeof(COMMUNICATE_PROCESS_MODULE) || !OutputBuffer || OutputBufferLength < sizeof(MODULES_INFORMATION))
	{
		return STATUS_INVALID_PARAMETER;
	}
	if (ProcessIdentity)
	{
		Status2 = PsLookupProcessByProcessId((HANDLE)ProcessIdentity, &EProcess);
	}
	if (!EProcess)
	{
		return STATUS_UNSUCCESSFUL;
	}
	if (PsIsRealProcess(EProcess))
	{
		PMODULES_INFORMATION v5 = (PMODULES_INFORMATION)AllocatePoolWithTag(PagedPool, OutputBufferLength);
		if (v5)
		{
			memset(v5, 0, OutputBufferLength);
			Status1 = EnumProcessModulesByPeb(EProcess, v5, NumberOfModule);
			if (NumberOfModule >= v5->NumberOfModules)
			{
				RtlCopyMemory(OutputBuffer, v5, OutputBufferLength);
				Status1 = STATUS_SUCCESS;
			}
			else
			{
				Status1 = STATUS_BUFFER_TOO_SMALL;
			}

			FreePoolWithTag(v5, 0);
			v5 = NULL;
		}
	}
	if (NT_SUCCESS(Status2))
	{
		ObfDereferenceObject(EProcess);
	}
	return Status1;
}
NTSTATUS EnumProcessModulesByPeb(PEPROCESS EProcess, PMODULES_INFORMATION ModulesInfo, ULONG NumberOfModule)
{
	BOOLEAN IsAttach = FALSE;
	KAPC_STATE ApcState;
	NTSTATUS Status = STATUS_UNSUCCESSFUL;
	if (!EProcess || !ModulesInfo || !NumberOfModule || KeGetCurrentIrql() >= DISPATCH_LEVEL)
	{
		return Status;
	}
#ifdef _WIN64
	ULONG_PTR Offset = 0x338;
#else
	ULONG_PTR Offset = 0x1a8;
#endif
	if (!MmIsAddressValid((PVOID)((ULONG_PTR)EProcess + Offset)))
	{
		return Status;
	}
	if (IoGetCurrentProcess() != EProcess)  //IoGetCurrentProcess用于获取当前执行的进程的EPROCESS结构指针
	{
		KeStackAttachProcess(EProcess, &ApcState);  //KeStackAttachProcess将当前线程附加到指定的进程上下文   ApcState用于保存当前线程的上下文，以便稍后可以恢复。
		IsAttach = TRUE;
	}
	__try
	{
		PPEB Peb = *((PPEB*)((ULONG_PTR)EProcess + Offset));
		if ((ULONG_PTR)Peb > 0 && (ULONG_PTR)Peb <= USER_ADDRESS_END)
		{
			PPEB_LDR_DATA PebLdrData = NULL;
			ProbeForRead(Peb, sizeof(PEB), 1);
			ProbeForRead(Peb->Ldr, sizeof(PEB_LDR_DATA), 1);
			PebLdrData = (PPEB_LDR_DATA)(Peb->Ldr);
			if ((ULONG_PTR)PebLdrData > 0 && (ULONG_PTR)PebLdrData <= USER_ADDRESS_END)
			{
				WalkModuleList(&PebLdrData->InLoadOrderModuleList, 1, ModulesInfo, NumberOfModule);
				//WalkModuleList(&PebLdrData->InMemoryOrderModuleList, 2, ModulesInfo, NumberOfModule);
				//WalkModuleList(&PebLdrData->InInitializationOrderModuleList, 3, ModulesInfo, NumberOfModule);
				Status = STATUS_SUCCESS;
			}
		}
	}
	__except (1)
	{
		Status = STATUS_UNSUCCESSFUL;
	}

	if (IsAttach)
	{
		KeUnstackDetachProcess(&ApcState);
		IsAttach = FALSE;
	}
	return Status;
}
void WalkModuleList(PLIST_ENTRY ListEntry, ULONG EnumType,PMODULES_INFORMATION ModulesInfo, ULONG NumberOfModule)
{
	PLIST_ENTRY v1 = NULL;
	v1 = ListEntry->Flink;
	while ((ULONG_PTR)v1 > 0 && (ULONG_PTR)v1 <= USER_ADDRESS_END & v1 != ListEntry)
	{
		PLDR_DATA_TABLE_ENTRY LdrDataTableEntry = NULL;
		switch (EnumType)
		{
		case 1:
			LdrDataTableEntry = CONTAINING_RECORD(v1, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
			break;
		case 2:
			LdrDataTableEntry = CONTAINING_RECORD(v1, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
			break;
		case 3:
			LdrDataTableEntry = CONTAINING_RECORD(v1, LDR_DATA_TABLE_ENTRY, InInitializationOrderLinks);
			break;
		}
		if ((ULONG_PTR)LdrDataTableEntry > 0 && (ULONG_PTR)LdrDataTableEntry < USER_ADDRESS_END)
		{
			__try
			{
				ProbeForRead(LdrDataTableEntry, sizeof(LDR_DATA_TABLE_ENTRY), 1);
				if (!InModuleList((ULONG_PTR)LdrDataTableEntry->ModuleBaseAddress, LdrDataTableEntry->ModuleSize, ModulesInfo, NumberOfModule))
				{
					if (NumberOfModule > ModulesInfo->NumberOfModules)
					{
						ULONG Length = CmpAndGetStringLength(&LdrDataTableEntry->FullModuleName, MAX_PATH);
						ModulesInfo->ModuleInfo[ModulesInfo->NumberOfModules].ModuleBase = (ULONG_PTR)LdrDataTableEntry->ModuleBaseAddress;
						ModulesInfo->ModuleInfo[ModulesInfo->NumberOfModules].SizeOfImage = LdrDataTableEntry->ModuleSize;
						ProbeForRead(LdrDataTableEntry->FullModuleName.Buffer, Length * sizeof(WCHAR), sizeof(WCHAR));
						wcsncpy(ModulesInfo->ModuleInfo[ModulesInfo->NumberOfModules].ModulePath, LdrDataTableEntry->FullModuleName.Buffer, Length);
					}
					ModulesInfo->NumberOfModules++;
				}
					
					
			}
			__except (1)
			{

			}
		}
		v1 = v1->Flink;
	}

}
void WalkModuleList2(PLIST_ENTRY ListEntry, ULONG EnumType, ULONG_PTR ModuleBase)
{
	PLIST_ENTRY v1 = NULL;
	v1 = ListEntry->Flink;
	while ((ULONG_PTR)v1 > 0 && (ULONG_PTR)v1 <= USER_ADDRESS_END & v1 != ListEntry)
	{
		PLDR_DATA_TABLE_ENTRY LdrDataTableEntry = NULL;
		switch (EnumType)
		{
		case 1:
			LdrDataTableEntry = CONTAINING_RECORD(v1, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
			break;
		case 2:
			LdrDataTableEntry = CONTAINING_RECORD(v1, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
			break;
		case 3:
			LdrDataTableEntry = CONTAINING_RECORD(v1, LDR_DATA_TABLE_ENTRY, InInitializationOrderLinks);
			break;
		}
		if ((ULONG_PTR)LdrDataTableEntry > 0 && (ULONG_PTR)LdrDataTableEntry < USER_ADDRESS_END)
		{
			__try
			{
				ProbeForRead(LdrDataTableEntry, sizeof(LDR_DATA_TABLE_ENTRY), 1);
				if ((ULONG_PTR)LdrDataTableEntry->ModuleBaseAddress == ModuleBase && (ULONG_PTR)v1->Blink <= USER_ADDRESS_END && (ULONG_PTR)v1->Flink <= USER_ADDRESS_END)
				{
					RemoveEntryList(v1);
					break;
				}


			}
			__except (1)
			{

			}
		}
		v1 = v1->Flink;
	}


}
BOOLEAN InModuleList(ULONG_PTR ModuleBase, ULONG SizeOfImage, PMODULES_INFORMATION ModulesInfo, ULONG NumberOfModule)
{
	BOOLEAN IsOk = FALSE;
	ULONG i = 0;
	ULONG v1 = ModulesInfo->NumberOfModules > NumberOfModule ? NumberOfModule : ModulesInfo->NumberOfModules;
	for (i = 0; i < v1; i++)
	{
		if (ModuleBase == ModulesInfo->ModuleInfo[i].ModuleBase && SizeOfImage == ModulesInfo->ModuleInfo[i].SizeOfImage)
		{
			IsOk = TRUE;
			break;
		}
		
	}
		
	return IsOk;
}
NTSTATUS PsUnloadProcessModule(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
	NTSTATUS Status1 = STATUS_UNSUCCESSFUL, Status2 = STATUS_UNSUCCESSFUL;
	PCOMMUNICATE_PROCESS_MODULE v1 = (PCOMMUNICATE_PROCESS_MODULE)InputBuffer;
	ULONG_PTR ProcessIdentity = v1->ProcessIdentity;
	PEPROCESS EProcess = NULL;
	ULONG_PTR ModuleBase = v1->u1.Unload.ModuleBase;
	//参数检查
	if (!InputBuffer || InputBufferLength != sizeof(COMMUNICATE_PROCESS_MODULE))
	{
		return STATUS_INVALID_PARAMETER;
	}
	if (ProcessIdentity)
	{
		Status2 = PsLookupProcessByProcessId((HANDLE)ProcessIdentity, &EProcess);
	}
	if (!EProcess)
	{
		return Status1;
	}
	if (PsIsRealProcess(EProcess))
	{
		HANDLE ProcessHandle = NULL;
		Status1 = ObOpenObjectByPointer(EProcess, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, GENERIC_ALL, *PsProcessType, KernelMode, &ProcessHandle);
		if (NT_SUCCESS(Status1))
		{
			if (__NtUnmapViewOfSection == NULL || __NtClose == NULL)
			{
				PSYSTEM_SERVICE_DESCRIPTOR_TABLE SystemServiceDescriptorTable = NULL;
				ULONG32 ServiceIndex = 0;
				SystemServiceDescriptorTable = GetKeServiceDescriptorTable();
				if (!GetNtXXXServiceIndex("NtUnmapViewOfSection", &ServiceIndex))
				{
					return;
				}
				if (!NT_SUCCESS(GetNtXXXServiceAddress(ServiceIndex,&__NtUnmapViewOfSection)))
				{
					return;
				}
				if (!GetNtXXXServiceIndex("NtClose", &ServiceIndex))
				{
					return;
				}
				if (!NT_SUCCESS(GetNtXXXServiceAddress(ServiceIndex, &__NtClose)))
				{
					return;
				}
			}
			PETHREAD EThread = PsGetCurrentThread();
			CHAR PreviousMode = ChangePreviousMode(EThread);
			Status1 = __NtUnmapViewOfSection(ProcessHandle, (PVOID)ModuleBase);
			if (NT_SUCCESS(Status1))
			{
				RemoveProcessModuleInPeb(EProcess, ModuleBase);
			}
			__NtClose(ProcessHandle);
			RecoverPreviousMode(EThread, PreviousMode);
		}
	}
	if (NT_SUCCESS(Status2))
	{
		ObfDereferenceObject(EProcess);
	}
	return Status1;

}
NTSTATUS RemoveProcessModuleInPeb(PEPROCESS EProcess, ULONG_PTR ModuleBase)
{
	NTSTATUS Status = STATUS_UNSUCCESSFUL;
	if (EProcess && KeGetCurrentIrql() < DISPATCH_LEVEL)
	{
#ifdef _WIN64
		ULONG_PTR Offset = 0x338;
#else
		ULONG_PTR Offset = 0x1a8;
#endif 
		BOOLEAN IsAttach = FALSE;
		KAPC_STATE ApcState;
		if (!MmIsAddressValid((PVOID)((ULONG_PTR)EProcess + Offset)))
		{
			return Status;
		}
		if (IoGetCurrentProcess() != EProcess)
		{
			KeStackAttachProcess(EProcess, &ApcState);
			IsAttach = TRUE;
		}
		__try
		{
			PPEB Peb = *(PPEB*)((ULONG_PTR)EProcess + Offset);
			if ((ULONG_PTR)Peb > 0 && (ULONG_PTR)Peb <= USER_ADDRESS_END)
			{
				PPEB_LDR_DATA PebLdrData = NULL;
				ProbeForRead(Peb, sizeof(PEB), 1);
				ProbeForRead(Peb->Ldr, sizeof(PEB_LDR_DATA), 1);
				PebLdrData = (PPEB_LDR_DATA)(Peb->Ldr);
				if ((ULONG_PTR)PebLdrData > 0 && (ULONG_PTR)PebLdrData < SYSTEM_ADDRESS_START)
				{
					WalkModuleList2(&PebLdrData->InLoadOrderModuleList, 1, ModuleBase);
					WalkModuleList2(&PebLdrData->InMemoryOrderModuleList, 2, ModuleBase);
					WalkModuleList2(&PebLdrData->InInitializationOrderModuleList, 3, ModuleBase);
					Status = STATUS_SUCCESS;
				}
			}
		}
		__except (1)
		{

		}
		if (IsAttach)
		{
			KeUnstackDetachProcess(&ApcState);
			IsAttach = TRUE;
		}

	}
	return Status;
}
