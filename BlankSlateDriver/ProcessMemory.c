#include"ProcessMemory.h"
#include"ProcessHelper.h"
#include"SystemHelper.h"
#include"MemoryHelper.h"
#include"ProcessThread.h"
#include"ProcessModule.h"

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

NTSTATUS EnumProcessMemorys(
	PEPROCESS EProcess,
	PMEMORYS_INFORMATION MemoryInfo,
	ULONG NumberOfMemory
)
{
	NTSTATUS Status;
	HANDLE ProcessHandle = NULL;
	PVOID VirtualAddress = 0;

	if (!EProcess || !MemoryInfo)
	{
		return STATUS_INVALID_PARAMETER;
	}

	MemoryInfo->NumberOfMemory = 0;

	//
	// 打开进程句柄
	//
	Status = ObOpenObjectByPointer(
		EProcess,
		OBJ_KERNEL_HANDLE,
		NULL,
		PROCESS_QUERY_INFORMATION,
		*PsProcessType,
		KernelMode,
		&ProcessHandle);

	if (!NT_SUCCESS(Status))
	{
		return Status;
	}

	//
	// 枚举内存
	//
	while ((ULONG_PTR)VirtualAddress <
		(ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
	{
		MEMORY_BASIC_INFORMATION MemoryBasicInfo;
		SIZE_T ReturnLength = 0;

		RtlZeroMemory(
			&MemoryBasicInfo,
			sizeof(MEMORY_BASIC_INFORMATION));

		Status = ZwQueryVirtualMemory(
			ProcessHandle,
			VirtualAddress,
			MemoryBasicInformation,
			&MemoryBasicInfo,
			sizeof(MEMORY_BASIC_INFORMATION),
			&ReturnLength);

		if (NT_SUCCESS(Status))
		{
			ULONG Index =
				MemoryInfo->NumberOfMemory;

			if (Index < NumberOfMemory)
			{
				MemoryInfo->MemoryInfo[Index].BaseAddress =
					VirtualAddress;

				MemoryInfo->MemoryInfo[Index].RegionSize =
					MemoryBasicInfo.RegionSize;

				MemoryInfo->MemoryInfo[Index].Protect =
					MemoryBasicInfo.Protect;

				MemoryInfo->MemoryInfo[Index].State =
					MemoryBasicInfo.State;

				MemoryInfo->MemoryInfo[Index].Type =
					MemoryBasicInfo.Type;
			}

			MemoryInfo->NumberOfMemory++;

			//
			// 防止死循环
			//
			if (MemoryBasicInfo.RegionSize == 0)
			{
				VirtualAddress =
					(PVOID)((ULONG_PTR)VirtualAddress + PAGE_SIZE);
			}
			else
			{
				VirtualAddress =
					(PVOID)((ULONG_PTR)VirtualAddress +
						MemoryBasicInfo.RegionSize);
			}
		}
		else
		{
			VirtualAddress =
				(PVOID)((ULONG_PTR)VirtualAddress + PAGE_SIZE);
		}
	}

	ZwClose(ProcessHandle);

	return STATUS_SUCCESS;
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
NTSTATUS PsWriteProcessMem(
	PVOID InputBuffer,
	ULONG InputBufferLength,
	PVOID OutputBuffer,
	ULONG OutputBufferLength,
	ULONG* ReturnValue)
{
	NTSTATUS Status1 = STATUS_UNSUCCESSFUL;
	NTSTATUS Status2 = STATUS_UNSUCCESSFUL;

	PEPROCESS EProcess = NULL;
	PVOID BufferData = NULL;

	BOOLEAN IsAttach = FALSE;

	KAPC_STATE ApcState;

	HANDLE ProcessHandle = NULL;

	ULONG OldProtect = 0;

	PCOMMUNICATE_PROCESS_MEMORY v1 =
		(PCOMMUNICATE_PROCESS_MEMORY)InputBuffer;

	//
	// 参数检测
	//
	if (!InputBuffer ||
		InputBufferLength !=
		sizeof(COMMUNICATE_PROCESS_MEMORY) +
		v1->ul.Write.RegionSize)
	{
		return STATUS_INVALID_PARAMETER;
	}

	if (v1->ul.Write.ProcessIdentity)
	{
		Status2 = PsLookupProcessByProcessId(
			(HANDLE)v1->ul.Write.ProcessIdentity,
			&EProcess);

		if (NT_SUCCESS(Status2) &&
			EProcess &&
			PsIsRealProcess(EProcess))
		{
			BufferData = AllocatePoolWithTag(
				PagedPool,
				v1->ul.Write.RegionSize);

			if (!BufferData)
			{
				ObDereferenceObject(EProcess);
				return STATUS_INSUFFICIENT_RESOURCES;
			}

			RtlCopyMemory(
				BufferData,
				v1->ul.Write.BufferData,
				v1->ul.Write.RegionSize);

			__try
			{
				PVOID BaseAddress =
					v1->ul.Write.BaseAddress;

				SIZE_T RegionSize =
					v1->ul.Write.RegionSize;

				PVOID v5 = BaseAddress;
				SIZE_T v7 = RegionSize;

				//
				// 第一种：直接写
				//
				KeStackAttachProcess(
					EProcess,
					&ApcState);

				IsAttach = TRUE;

				__try
				{
					RtlCopyMemory(
						BaseAddress,
						BufferData,
						RegionSize);

					Status1 = STATUS_SUCCESS;
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					Status1 = GetExceptionCode();
				}

				if (IsAttach)
				{
					KeUnstackDetachProcess(
						&ApcState);

					IsAttach = FALSE;
				}

				//
				// 如果失败，尝试修改保护
				//
				if (!NT_SUCCESS(Status1))
				{
					Status1 =
						ObOpenObjectByPointer(
							EProcess,
							OBJ_KERNEL_HANDLE,
							NULL,
							PROCESS_VM_OPERATION |
							PROCESS_VM_WRITE,
							*PsProcessType,
							KernelMode,
							&ProcessHandle);

					if (NT_SUCCESS(Status1))
					{
						Status1 =
							ZwProtectVirtualMemory(
								ProcessHandle,
								&v5,
								&v7,
								PAGE_READWRITE,
								&OldProtect);

						if (NT_SUCCESS(Status1))
						{
							__try
							{
								KeStackAttachProcess(
									EProcess,
									&ApcState);

								IsAttach = TRUE;

								RtlCopyMemory(
									BaseAddress,
									BufferData,
									RegionSize);

								Status1 =
									STATUS_SUCCESS;

								if (IsAttach)
								{
									KeUnstackDetachProcess(
										&ApcState);

									IsAttach = FALSE;
								}

								//
								// 恢复保护
								//
								ZwProtectVirtualMemory(
									ProcessHandle,
									&BaseAddress,
									&v7,
									OldProtect,
									&OldProtect);
							}
							__except (EXCEPTION_EXECUTE_HANDLER)
							{
								if (IsAttach)
								{
									KeUnstackDetachProcess(
										&ApcState);

									IsAttach = FALSE;
								}

								Status1 =
									GetExceptionCode();
							}
						}
					}
				}
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				if (IsAttach)
				{
					KeUnstackDetachProcess(
						&ApcState);

					IsAttach = FALSE;
				}

				Status1 = GetExceptionCode();
			}

			if (BufferData)
			{
				FreePoolWithTag(BufferData);
			}

			if (ProcessHandle)
			{
				ZwClose(ProcessHandle);
			}

			ObDereferenceObject(EProcess);
		}
	}

	return Status1;
}
NTSTATUS PsModifyProcessMem(
	PVOID InputBuffer,
	ULONG InputBufferLength,
	PVOID OutputBuffer,
	ULONG OutputBufferLength,
	ULONG* ReturnValue)
{
	NTSTATUS Status1 = STATUS_UNSUCCESSFUL;
	NTSTATUS Status2 = STATUS_UNSUCCESSFUL;

	PCOMMUNICATE_PROCESS_MEMORY v1 =
		(PCOMMUNICATE_PROCESS_MEMORY)InputBuffer;

	PEPROCESS EProcess = NULL;

	HANDLE ProcessHandle = NULL;

	ULONG NewProtect = 0;

	PVOID BaseAddress = NULL;

	SIZE_T RegionSize = 0;

	ULONG_PTR ProcessIdentity = 0;

	//
	// 参数检测
	//
	if (!InputBuffer ||
		InputBufferLength !=
		sizeof(COMMUNICATE_PROCESS_MEMORY))
	{
		return STATUS_INVALID_PARAMETER;
	}

	BaseAddress = v1->Modify.BaseAddress;
	RegionSize = v1->Modify.RegionSize;
	NewProtect = v1->Modify.NewProtect;
	ProcessIdentity = v1->Modify.ProcessIdentity;

	if ((ULONG_PTR)BaseAddress >
		USER_ADDRESS_END ||

		RegionSize >=
		SYSTEM_ADDRESS_START ||

		(ULONG_PTR)BaseAddress +
		RegionSize >=
		SYSTEM_ADDRESS_START)
	{
		return STATUS_INVALID_PARAMETER;
	}

	if (ProcessIdentity)
	{
		Status2 =
			PsLookupProcessByProcessId(
				(HANDLE)ProcessIdentity,
				&EProcess);
	}

	if (NT_SUCCESS(Status2) &&
		PsIsRealProcess(EProcess))
	{
		Status1 =
			ObOpenObjectByPointer(
				EProcess,
				OBJ_KERNEL_HANDLE |
				OBJ_CASE_INSENSITIVE,
				NULL,
				PROCESS_VM_OPERATION,
				*PsProcessType,
				KernelMode,
				&ProcessHandle);

		if (NT_SUCCESS(Status1))
		{
			ULONG OldProtect = 0;

			Status1 =
				ZwProtectVirtualMemory(
					ProcessHandle,
					&BaseAddress,
					&RegionSize,
					NewProtect,
					&OldProtect);

			if (NT_SUCCESS(Status1))
			{
				if (OutputBuffer &&
					OutputBufferLength >=
					sizeof(ULONG))
				{
					*(PULONG)OutputBuffer =
						OldProtect;
				}
			}

			ZwClose(ProcessHandle);
		}

		ObDereferenceObject(EProcess);
	}

	return Status1;
}

