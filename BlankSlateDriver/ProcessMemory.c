#include"ProcessMemory.h"
#include"ProcessHelper.h"
#include"SystemHelper.h"
#include"MemoryHelper.h"
#include"ProcessThread.h"
#include"ProcessModule.h"
LPFN_NTQUERYVIRTUALMEMORY __NtQueryVirtualMemory = NULL;
LPFN_NTPROTECTVIRTUALMEMORY __NtProtectVirtualMemory = NULL;
NTSTATUS PsEnumProcessMem(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
	NTSTATUS Status1 = STATUS_UNSUCCESSFUL, Status2 = STATUS_UNSUCCESSFUL;
	PCOMMUNICATE_PROCESS_MEMORY v1 = (PCOMMUNICATE_PROCESS_MEMORY)InputBuffer;
	PEPROCESS EProcess = NULL;
	ULONG_PTR ProcessIdentity = 0;
	ULONG NumberOfMemory = (OutputBufferLength - sizeof(MEMORYS_INFORMATION)) / sizeof(MEMORY_INFORMATION_ENTRY);
	//参数检测
	if (!InputBuffer || InputBufferLength != sizeof(COMMUNICATE_PROCESS_MEMORY) || !OutputBuffer || OutputBufferLength < sizeof(MEMORYS_INFORMATION))
	{
		return STATUS_INVALID_PARAMETER;
	}
	ProcessIdentity = v1->ul.Query.ProcessIdentity;
	if (ProcessIdentity)
	{
		Status2 = PsLookupProcessByProcessId((HANDLE)ProcessIdentity, &EProcess);
	}
	if (PsIsRealProcess(EProcess))
	{
		Status1 = EnumProcessMemorys(EProcess, (PMEMORYS_INFORMATION)OutputBuffer, NumberOfMemory);
		if (NT_SUCCESS(Status1))
		{
			if (NumberOfMemory >= ((PMEMORYS_INFORMATION)OutputBuffer)->NumberOfMemory)
			{
				Status1 = STATUS_SUCCESS;
			}
			else
			{
				Status1 = STATUS_BUFFER_TOO_SMALL;
			}
		}
	}
	if (NT_SUCCESS(Status2))
	{
		ObfDereferenceObject(EProcess);
	}
	return Status1;
}

NTSTATUS EnumProcessMemorys(PEPROCESS EProcess, PMEMORYS_INFORMATION MemoryInfo, ULONG NumberOfMemory)
{
	NTSTATUS Status = STATUS_UNSUCCESSFUL;
	HANDLE ProcessHandle = NULL;
	PETHREAD EThread = NULL;
	CHAR PreviousMode = 0;
	Status = ObOpenObjectByPointer(EProcess, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, GENERIC_ALL, *PsProcessType, KernelMode, &ProcessHandle);
	if (NT_SUCCESS(Status))
	{
		PVOID VirtualAddress = 0;
		if (__NtQueryVirtualMemory == NULL)
		{
			PSYSTEM_SERVICE_DESCRIPTOR_TABLE SystemServiceDescriptorTable = NULL;
			ULONG32 ServiceIndex = 0;
			SystemServiceDescriptorTable = GetKeServiceDescriptorTable2();
			if (!GetNtXXXServiceIndex("NtQueryVirtualMemory", &ServiceIndex))
			{
				return STATUS_UNSUCCESSFUL;
			}
			if (!NT_SUCCESS(GetNtXXXServiceAddress(ServiceIndex, (PVOID*)&__NtQueryVirtualMemory)))
			{
				return STATUS_UNSUCCESSFUL;
			}
		}
		if (__NtClose == NULL)
		{
			PSYSTEM_SERVICE_DESCRIPTOR_TABLE SystemServiceDescriptorTable = NULL;
			ULONG32 ServiceIndex = 0;
			SystemServiceDescriptorTable = GetKeServiceDescriptorTable2();
			if (!GetNtXXXServiceIndex("NtClose", &ServiceIndex))
			{
				return STATUS_UNSUCCESSFUL;
			}
			if (!NT_SUCCESS(GetNtXXXServiceAddress(ServiceIndex, (PVOID*)&__NtClose)))
			{
				return STATUS_UNSUCCESSFUL;
			}
		}
		EThread = PsGetCurrentThread();
		PreviousMode = ChangePreviousMode(EThread);
		ULONG_PTR v100 = (ULONG_PTR)MM_HIGHEST_USER_ADDRESS;
		while (VirtualAddress < MM_HIGHEST_USER_ADDRESS)
		{
			MEMORY_BASIC_INFORMATION MemoryBasicInfo;
			SIZE_T ReturnLength = 0;
			NTSTATUS Status2 = __NtQueryVirtualMemory(ProcessHandle, (PVOID)VirtualAddress, MemoryBasicInformation, &MemoryBasicInfo, sizeof(MEMORY_BASIC_INFORMATION), &ReturnLength);
			if (NT_SUCCESS(Status2))
			{
				ULONG v1 = MemoryInfo->NumberOfMemory;
				if (NumberOfMemory > v1)
				{
					MemoryInfo->MemoryInfo[v1].BaseAddress = VirtualAddress;
					MemoryInfo->MemoryInfo[v1].RegionSize = MemoryBasicInfo.RegionSize;
					MemoryInfo->MemoryInfo[v1].Protect = MemoryBasicInfo.Protect;
					MemoryInfo->MemoryInfo[v1].State = MemoryBasicInfo.State;
					MemoryInfo->MemoryInfo[v1].Type = MemoryBasicInfo.Type;
				}
				MemoryInfo->NumberOfMemory++;
				//VirtualAddress += MemoryBasicInfo.RegionSize;
				VirtualAddress = (PVOID)((ULONG_PTR)VirtualAddress + MemoryBasicInfo.RegionSize);
			}
			else
			{
				VirtualAddress = (PVOID)((ULONG_PTR)VirtualAddress + PAGE_SIZE);
			}

		}
		__NtClose(ProcessHandle);
		RecoverPreviousMode(EThread, PreviousMode);
	}
	return Status;
}

NTSTATUS PsReadProcessMem(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
	NTSTATUS Status1 = STATUS_UNSUCCESSFUL;
	NTSTATUS Status2 = STATUS_UNSUCCESSFUL;
	PEPROCESS EProcess;
	PVOID BufferData = NULL;
	BOOLEAN IsAttach = FALSE;
	KAPC_STATE ApcState;
	PCOMMUNICATE_PROCESS_MEMORY v1 = (PCOMMUNICATE_PROCESS_MEMORY)InputBuffer;
	//参数检测
	if (!InputBuffer || InputBufferLength != sizeof(COMMUNICATE_PROCESS_MEMORY) || !OutputBuffer || OutputBufferLength < MAX_LENGTH)
	{
		return STATUS_INVALID_PARAMETER;
	}
	if (v1->ul.Read.ProcessIdentity)
	{
		Status2 = PsLookupProcessByProcessId((HANDLE)v1->ul.Read.ProcessIdentity, &EProcess);
		if (NT_SUCCESS(Status2) && EProcess != NULL && PsIsRealProcess(EProcess))
		{
			BufferData = AllocatePoolWithTag(PagedPool, OutputBufferLength);
			if (BufferData == NULL)
			{
				return STATUS_UNSUCCESSFUL;
			}
			memset(BufferData, 0, OutputBufferLength);
			__try
			{
				ULONG_PTR ProcessIdentity = v1->ul.Read.ProcessIdentity;
				PVOID BaseAddress = v1->ul.Read.BaseAddress;
				SIZE_T RegionSize = v1->ul.Read.RegionSize;
				KeStackAttachProcess(EProcess, &ApcState);
				IsAttach = TRUE;
				ProbeForRead(BaseAddress, RegionSize, 1);
				memcpy(BufferData, BaseAddress, RegionSize);
				if (IsAttach)
				{
					KeUnstackDetachProcess(&ApcState);
					IsAttach = FALSE;
				}
				memcpy(OutputBuffer, BufferData, OutputBufferLength);
				if (BufferData != NULL)
				{
					FreePoolWithTag(BufferData);
				}
				Status1 = STATUS_SUCCESS;
			}
			__except (1)
			{
				if (IsAttach == TRUE)
				{
					KeUnstackDetachProcess(&ApcState);
				}
				if (BufferData != NULL)
				{
					FreePoolWithTag(BufferData);
				}
				Status1 = STATUS_UNSUCCESSFUL;
			}
		}
		if (NT_SUCCESS(Status2))
		{
			ObDereferenceObject(EProcess);
		}

	}
	return Status1;


}

NTSTATUS PsWriteProcessMem(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
	__debugbreak();
	NTSTATUS Status1 = STATUS_UNSUCCESSFUL;
	NTSTATUS Status2 = STATUS_UNSUCCESSFUL;
	PEPROCESS EProcess = NULL;
	PVOID BufferData = NULL;
	BOOLEAN IsAttach = FALSE;
	KAPC_STATE ApcState;
	HANDLE ProcessHandle = NULL;
	PETHREAD EThread = NULL;
	CHAR PreviousMode = 0;
	ULONG OldProtect = 0;
	PCOMMUNICATE_PROCESS_MEMORY v1 = (PCOMMUNICATE_PROCESS_MEMORY)InputBuffer;
	//参数检测
	if (!InputBuffer || InputBufferLength != sizeof(COMMUNICATE_PROCESS_MEMORY)+v1->ul.Write.RegionSize)
	{
		return STATUS_INVALID_PARAMETER;
	}
	if (v1->ul.Write.ProcessIdentity)
	{
		Status2 = PsLookupProcessByProcessId((HANDLE)v1->ul.Write.ProcessIdentity, &EProcess);
		if (NT_SUCCESS(Status2) && EProcess && PsIsRealProcess(EProcess))
		{
			BufferData = AllocatePoolWithTag(PagedPool, v1->ul.Write.RegionSize);
			if (BufferData == NULL)
			{
				return STATUS_UNSUCCESSFUL;
			}
			memcpy(BufferData, v1->ul.Write.BufferData, v1->ul.Write.RegionSize);
			__try
			{
				PVOID BaseAddress = v1->ul.Write.BaseAddress;
				SIZE_T RegionSize = v1->ul.Write.RegionSize;
				PVOID v5 = BaseAddress;
				SIZE_T v7 = RegionSize;
				KeStackAttachProcess(EProcess, &ApcState);
				IsAttach = TRUE;
				__try
				{
					memcpy((PVOID)BaseAddress, (PVOID)BufferData, RegionSize);
					if (IsAttach)
					{
						KeUnstackDetachProcess(&ApcState);
						IsAttach = FALSE;
					}
					if (BufferData != NULL)
					{
						FreePoolWithTag(BufferData);
					}
					Status1 = STATUS_SUCCESS;
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					if (IsAttach)
					{
						KeUnstackDetachProcess(&ApcState);
						IsAttach = FALSE;
					}
					EThread = PsGetCurrentThread();
					PreviousMode = ChangePreviousMode(EThread);
					Status1 = ObOpenObjectByPointer(EProcess, OBJ_KERNEL_HANDLE, NULL, GENERIC_ALL, *PsProcessType, KernelMode, &ProcessHandle);
					if (!NT_SUCCESS(Status1))
					{
						RecoverPreviousMode(EThread, PreviousMode);
						if (BufferData != NULL)
						{
							FreePoolWithTag(BufferData);
						}
						Status1 = STATUS_UNSUCCESSFUL;
					}
					else
					{
						if (__NtProtectVirtualMemory == NULL)
						{
							PSYSTEM_SERVICE_DESCRIPTOR_TABLE SystemServiceDescriptorTable = NULL;
							ULONG32 ServiceIndex = 0;
							SystemServiceDescriptorTable = GetKeServiceDescriptorTable2();
							if (!GetNtXXXServiceIndex("NtProtectVirtualMemory", &ServiceIndex))
							{
								Status1 = STATUS_UNSUCCESSFUL;
							}
							if (!NT_SUCCESS(GetNtXXXServiceAddress(ServiceIndex, (PVOID*)&__NtProtectVirtualMemory)))
							{
								Status1 = STATUS_UNSUCCESSFUL;
							}
						}
						if (__NtProtectVirtualMemory != NULL)
						{
							Status1 = __NtProtectVirtualMemory(ProcessHandle, &v5, &v7, PAGE_READWRITE, &OldProtect); //这里使用v5与v7的原因是该函数的执行会改变我们的BaseAddress为页基址，导致下方拷贝失败，所以使用替身来改变属性
							if (!NT_SUCCESS(Status1))
							{
								RecoverPreviousMode(EThread, PreviousMode);
								ZwClose(ProcessHandle);
								if (BufferData != NULL)
								{
									FreePoolWithTag(BufferData);
								}
								Status1 = STATUS_UNSUCCESSFUL;
							}
							else
							{
								__try
								{
									KeStackAttachProcess(EProcess, &ApcState);
									IsAttach = TRUE;
									memcpy((PVOID)BaseAddress, (PVOID)BufferData, RegionSize);
									if (IsAttach)
									{
										KeUnstackDetachProcess(&ApcState);
										IsAttach = FALSE;
									}
									EThread = PsGetCurrentThread();
									PreviousMode = ChangePreviousMode(EThread);
									Status1 = __NtProtectVirtualMemory(ProcessHandle, &BaseAddress, &v7, OldProtect, NULL);
									RecoverPreviousMode(EThread, PreviousMode);
									ZwClose(ProcessHandle);
									if (BufferData != NULL)
									{
										ExFreePool(BufferData);
									}

								}
								__except (1)
								{
									if (IsAttach)
									{
										KeUnstackDetachProcess(&ApcState);
										IsAttach = FALSE;
									}
									if (BufferData != NULL)
									{
										FreePoolWithTag(BufferData);
									}
									Status1 = STATUS_UNSUCCESSFUL;
								}
							}
							RecoverPreviousMode(EThread, PreviousMode);
						}
						
					}
				}
			}
			__except (1)
			{
				if (IsAttach)
				{
					KeUnstackDetachProcess(&ApcState);
					IsAttach = FALSE;
				}
				if (BufferData != NULL)
				{
					FreePoolWithTag(BufferData);
				}
				Status1 = STATUS_UNSUCCESSFUL;
			}
		}
	}
	if (NT_SUCCESS(Status2))
	{
		ObDereferenceObject(EProcess);
	}
	return Status1;

}

NTSTATUS PsModifyProcessMem(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
	NTSTATUS Status1 = STATUS_UNSUCCESSFUL, Status2 = STATUS_UNSUCCESSFUL;
	PCOMMUNICATE_PROCESS_MEMORY v1 = (PCOMMUNICATE_PROCESS_MEMORY)InputBuffer;
	PEPROCESS EProcess = NULL;
	ULONG_PTR ProcessIdentity = 0;
	HANDLE ProcessHandle;
	ULONG NewProtect = 0;
	PVOID BaseAddress = NULL;
	SIZE_T RegionSize = 0;
	//参数检测
	if (!InputBuffer || InputBufferLength != sizeof(COMMUNICATE_PROCESS_MEMORY))
	{
		return STATUS_INVALID_PARAMETER;
	}

	BaseAddress = v1->Modify.BaseAddress;
	RegionSize = v1->Modify.RegionSize;
	NewProtect = v1->Modify.NewProtect;
	ProcessIdentity = v1->Modify.ProcessIdentity;
	if (BaseAddress > USER_ADDRESS_END || RegionSize >= SYSTEM_ADDRESS_START || (ULONG_PTR)BaseAddress + RegionSize >= SYSTEM_ADDRESS_START)
	{
		return STATUS_INVALID_PARAMETER;
	}
	if (ProcessIdentity)
	{
		Status2 = PsLookupProcessByProcessId((HANDLE)ProcessIdentity, &EProcess);
	}
	if (PsIsRealProcess(EProcess))
	{
		Status1 = ObOpenObjectByPointer(EProcess, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, GENERIC_ALL, *PsProcessType, KernelMode, &ProcessHandle);
		if (NT_SUCCESS(Status1))
		{
			ULONG ulOldProtect = 0;
			if (__NtProtectVirtualMemory == NULL)
			{
				PSYSTEM_SERVICE_DESCRIPTOR_TABLE SystemServiceDescriptorTable = NULL;
				ULONG32 ServiceIndex = 0;
				SystemServiceDescriptorTable = GetKeServiceDescriptorTable2();
				if (!GetNtXXXServiceIndex("NtProtectVirtualMemory", &ServiceIndex))
				{
					Status1 = STATUS_UNSUCCESSFUL;
				}
				if (!NT_SUCCESS(GetNtXXXServiceAddress(ServiceIndex, (PVOID*)&__NtProtectVirtualMemory)))
				{
					Status1 = STATUS_UNSUCCESSFUL;
				}
			}
			if (__NtProtectVirtualMemory != NULL)
			{
				PETHREAD EThread = PsGetCurrentThread();
				CHAR PreviousMode = ChangePreviousMode(EThread);
				Status1 = __NtProtectVirtualMemory(ProcessHandle, &BaseAddress, &RegionSize, NewProtect, (PULONG)OutputBuffer);
				RecoverPreviousMode(EThread, PreviousMode);
			}
			__NtClose(ProcessHandle);
		}
	}
	if (NT_SUCCESS(Status2))
	{
		ObfDereferenceObject(EProcess);
	}
	return Status1;
}

