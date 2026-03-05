#include"ProcessThread.h"
#include"ProcessHelper.h"
#include"SystemHelper.h"

NTSTATUS PsLookupThreadByEProcess(PEPROCESS EProcess, PETHREAD* EThread)
{
	NTSTATUS Status = STATUS_SUCCESS;
	HANDLE ProcessIdentity = PsGetProcessId(EProcess);//目标进程
	//0x000000000000059c
	PVOID BufferData = ExAllocatePool(NonPagedPool, 1024*1024);
	// 系统下的所有进程信息
	PSYSTEM_PROCESS_INFORMATION SystemProcessInfo = (PSYSTEM_PROCESS_INFORMATION)BufferData;
	// 返回值
	if (EThread == NULL)
	{
		return STATUS_INVALID_PARAMETER;
	}
		
	//动态内存是否成功
	if (!SystemProcessInfo)
	{
		return STATUS_NO_MEMORY;
	}
	//获得进程队列
	Status = ZwQuerySystemInformation(SystemProcessInformation, SystemProcessInfo, 1024 * 1024, NULL);
	if (!NT_SUCCESS(Status))
	{
		ExFreePool(BufferData);
		return Status;
	}

	// 查找目标进程
	if (NT_SUCCESS(Status))
	{
		Status = STATUS_NOT_FOUND;
		for (;;)
		{
			if (SystemProcessInfo->UniqueProcessId == ProcessIdentity)//枚举出来的进程与目标进程一致
			{
				Status = STATUS_SUCCESS;
				break;
			}
			else if (SystemProcessInfo->NextEntryOffset)
			{
				SystemProcessInfo = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)SystemProcessInfo + SystemProcessInfo->NextEntryOffset);
			}
			else
				break;
		}
			
	}
	// 查找目标线程
	if (NT_SUCCESS(Status))
	{

		Status = STATUS_NOT_FOUND;
		//获得第一个线程
		for (ULONG i = 0; i < SystemProcessInfo->NumberOfThreads; i++)
		{
			if (SystemProcessInfo->TH[i].ClientId.UniqueThread == PsGetCurrentThreadId())
			{
				continue; // 除过我们自己
			}
			// 获取第一个不是自己的线程
			Status = PsLookupThreadByThreadId(SystemProcessInfo->TH[i].ClientId.UniqueThread, EThread);
			break;
		}
			
		
	}
	else
	{
	}
	if (BufferData)
	{
		ExFreePool(BufferData);
	}
	//没有合适的线程
	if (!*EThread)
	{
		Status = STATUS_NOT_FOUND;
	}
	return Status;
		
}
NTSTATUS ExecuteInNewThread(IN PVOID BaseAddress, IN PVOID ParameterData, IN ULONG Flags, IN BOOLEAN IsWait, OUT PNTSTATUS ExitStatus)
{
	HANDLE ThreadHandle = NULL;
	OBJECT_ATTRIBUTES ObjectAttributes = { 0 };
	//指定一个对象句柄的属性句柄只能在内核模式访问。
	InitializeObjectAttributes(&ObjectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

	//创建线程
	NTSTATUS Status = CreateThreadEx(
		&ThreadHandle, THREAD_QUERY_LIMITED_INFORMATION, &ObjectAttributes,
		ZwCurrentProcess(), BaseAddress, ParameterData, Flags,
		0, 0x1000, 0x100000, NULL);
	// 等待线程完成
	if (NT_SUCCESS(Status) && IsWait != FALSE)
	{
		//60s
		LARGE_INTEGER Timeout = { 0 };
		Timeout.QuadPart = -(6011 * 10 * 1000 * 1000);
		Status = ZwWaitForSingleObject(ThreadHandle, TRUE, &Timeout);
		if (NT_SUCCESS(Status))
		{
			//查询线程退出码
			THREAD_BASIC_INFORMATION ThreadBasicInfo = { 0 };
			ULONG ReturnLength = 0;
			Status = ZwQueryInformationThread(ThreadHandle, ThreadBasicInformation, &ThreadBasicInfo, sizeof(ThreadBasicInfo), &ReturnLength);
			if (NT_SUCCESS(Status) && ExitStatus)
			{
				*ExitStatus = ThreadBasicInfo.ExitStatus;
			}
			else if (!NT_SUCCESS(Status))
			{

			}

		}
		else
		{

		}

	}
	else
	{

	}
	if (ThreadHandle)
	{
		ZwClose(ThreadHandle);
	}
	return Status;
}
NTSTATUS CreateThreadEx(OUT PHANDLE ThreadHandle, IN ACCESS_MASK DesiredAccess, IN PVOID ObjectAttributes, IN HANDLE ProcessHandle, IN PVOID StartAddress, IN PVOID ParameterData, IN ULONG Flags, IN SIZE_T StackZeroBits, IN SIZE_T SizeOfStackCommit, IN SIZE_T SizeOfStackReserve, IN PNT_PROC_THREAD_ATTRIBUTE_LIST AttributeList)
{
	NTSTATUS Status = STATUS_SUCCESS;
	LPFN_NTCREATETHREADEX NtCreateThreadEx = NULL;
	PSYSTEM_SERVICE_DESCRIPTOR_TABLE SystemServiceDescriptorTable = GetKeServiceDescriptorTable();
	ULONG_PTR ServiceIndex = 0;
	if (SystemServiceDescriptorTable)
	{
		//检查索引范围
		if (!GetNtXXXServiceIndex("NtCreateThreadEx", &ServiceIndex))
		{
			return STATUS_UNSUCCESSFUL;
		}
		GetNtXXXServiceAddress(ServiceIndex, &NtCreateThreadEx);
	}
	if (NtCreateThreadEx)
	{
#ifdef _WIN64
		PUCHAR v1 = (PUCHAR)PsGetCurrentThread() + 0x1F6;
#else
		PUCHAR v1 = (PUCHAR)PsGetCurrentThread() + 0x13a;   //+0x13a PreviousMode     : Char
#endif

		UCHAR PreviousMode = *v1;  //想要调用内核的ssdt服务函数，必须切换到内核模式
		*v1 = KernelMode;
		//创建线程
		Status = NtCreateThreadEx(
			ThreadHandle, DesiredAccess, ObjectAttributes,
			ProcessHandle, StartAddress, ParameterData,
			Flags, StackZeroBits, SizeOfStackCommit,
			SizeOfStackReserve, AttributeList
		);
		//恢复之前的线程模式
		*v1 = PreviousMode;
	}
	else
		Status = STATUS_NOT_FOUND;
	return Status;
}

CHAR ChangePreviousMode(PETHREAD EThread)
{
#ifdef _WIN64
	ULONG Offset = 0x1F6;
#else
	ULONG Offset = 0x13a;
#endif
	CHAR PreviousMode = *(PCHAR)((ULONG_PTR)EThread + Offset);
	*(PCHAR)((ULONG_PTR)EThread + Offset) = KernelMode;
	return PreviousMode;
}
VOID RecoverPreviousMode(PETHREAD EThread, CHAR PreviousMode)
{
#ifdef _WIN64
	ULONG Offset = 0x1F6;
#else
	ULONG Offset = 0x13a;
#endif
	*(PCHAR)((ULONG_PTR)EThread + Offset) = PreviousMode;
}
