#include "MemoryHelper.h"
#include"ProcessHelper.h"
#include"IoControlHelper.h"
BOOLEAN MapFileInKernelSpace(WCHAR* FullPath, PVOID* VirtualAddress, PSIZE_T ViewSize)
{
	NTSTATUS Status;
	UNICODE_STRING v1;
	OBJECT_ATTRIBUTES ObjectAttributes;
	IO_STATUS_BLOCK IoStatusBlock;
	HANDLE FileHandle = NULL;
	HANDLE SectionHandle = NULL;
	if (!FullPath && MmIsAddressValid(FullPath))
		return FALSE;
	if (!VirtualAddress && MmIsAddressValid(VirtualAddress))
	{
		return FALSE;
	}

	//将文件路径转换成UNICODE_STRING存储
	RtlInitUnicodeString(&v1, FullPath);
	//根据UNICODE STRING创建对象属性
	InitializeObjectAttributes(&ObjectAttributes,
		&v1,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL,
		NULL);
	// 获得文件句柄
	//zwcrete
	Status = IoCreateFile(&FileHandle,
		GENERIC_READ | SYNCHRONIZE,
		&ObjectAttributes, // 文件绝对路径
		&IoStatusBlock,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_READ,
		FILE_OPEN,
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL,
		0,
		CreateFileTypeNone,
		NULL,
		IO_NO_PARAMETER_CHECKING
	);
	if (!NT_SUCCESS(Status))
	{
		return FALSE;
	}
	//根据文件句柄创建映射对象
	ObjectAttributes.ObjectName = NULL;
	//创建内存映射
	Status = ZwCreateSection(&SectionHandle,
		SECTION_QUERY | SECTION_MAP_READ,
		&ObjectAttributes,
		NULL,
		PAGE_WRITECOPY,
		SEC_IMAGE,    //内存对齐  0x1000
		FileHandle
	);
	ZwClose(FileHandle);
	if (!NT_SUCCESS(Status))
	{
		return FALSE;
	}
	Status = ZwMapViewOfSection(SectionHandle,
		NtCurrentProcess(),  // 映射到当前进程的内存空间中
		VirtualAddress,
		0,
		0,
		0,
		ViewSize,
		ViewUnmap,
		0,
		PAGE_WRITECOPY
	);
	ZwClose(SectionHandle);
	if (!NT_SUCCESS(Status))
	{
		return FALSE;
	}
		
	return	TRUE;
				
}

NTSTATUS VirtualProtect(PMDL* Mdl, PVOID VirtualAddress1, PSIZE_T ViewSize, PVOID* VirtualAddress2)
{
	*Mdl = MmCreateMdl(NULL, VirtualAddress1, ViewSize);    //新建内存描述链，两个虚拟地址指向同一块物理地址，新建是为了可以对目标物理地址进行写操作
	if (!(*Mdl))
		return STATUS_UNSUCCESSFUL;
	MmBuildMdlForNonPagedPool(*Mdl);
	(*Mdl)->MdlFlags = (*Mdl)->MdlFlags | MDL_MAPPED_TO_SYSTEM_VA;
	*VirtualAddress2 = MmMapLockedPages(*Mdl, KernelMode);
	return STATUS_SUCCESS;
}

NTSTATUS UnVirtualProtect(PMDL Mdl, PVOID VirtualAddress2)
{
	if (Mdl)
	{
		MmUnmapLockedPages(VirtualAddress2, Mdl);
		IoFreeMdl(Mdl);
	}
}

//NTSTATUS PsDumpProcessModule(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
//{
//	NTSTATUS Status1 = STATUS_UNSUCCESSFUL, Status2 = STATUS_UNSUCCESSFUL;
//	PCOMMUNICATE_PROCESS_MODULE v1 = (PCOMMUNICATE_PROCESS_MODULE)InputBuffer;
//	PEPROCESS EProcess = NULL, TempEProcess = NULL;
//	ULONG_PTR ModuleBase = 0;
//	ULONG SizeOfImage = 0;
//	ULONG_PTR ProcessIdentity = 0;
//	//参数检查
//	if (!InputBuffer || InputBufferLength != sizeof(COMMUNICATE_PROCESS_MODULE) || !OutputBuffer || !OutputBufferLength)
//	{
//		return STATUS_INVALID_PARAMETER;
//	}
//	ModuleBase = v1->u1.Dump.ModuleBase;
//	SizeOfImage = v1->u1.Dump.SizeOfImage;
//	ProcessIdentity = v1->ProcessIdentity;
//	if (ModuleBase > USER_ADDRESS_END || SizeOfImage > USER_ADDRESS_END || ModuleBase + SizeOfImage > USER_ADDRESS_END)
//	{
//		return STATUS_INVALID_PARAMETER;
//	}
//	if (ProcessIdentity)
//	{
//		Status2 = PsLookupProcessByProcessId((HANDLE)ProcessIdentity, &EProcess);  //目前这个函数没检测
//	}
//	if (!EProcess)
//	{
//		return Status1;
//	}
//	if (PsIsRealProcess(EProcess))
//	{
//		Status1 = SafeCopyProcessModule(EProcess, ModuleBase, SizeOfImage, OutputBuffer);
//	}
//	if (NT_SUCCESS(Status2))
//	{
//		ObfDereferenceObject(EProcess);
//	}
//	return Status1;
//
//
//}
NTSTATUS SafeCopyProcessModule(PEPROCESS EProcess, ULONG_PTR ModuleBase, ULONG SizeOfImage, PVOID OutputBuffer)
{
	BOOLEAN IsAttach = FALSE;
	KAPC_STATE ApcState;
	PVOID v5 = NULL;
	NTSTATUS Status = STATUS_UNSUCCESSFUL;
	if (SizeOfImage == 0 || OutputBuffer == NULL || EProcess == NULL)
	{
		return Status;
	}
	v5 = AllocatePoolWithTag(NonPagedPool, SizeOfImage);
	if (!v5)
	{
		return Status;
	}
	memset(v5, 0, SizeOfImage);
	if (EProcess != IoGetCurrentProcess())
	{
		KeStackAttachProcess(EProcess, &ApcState);  //附加进程 游戏检测KeStackAttachProcess
		IsAttach = TRUE;
	}
	Status = SafeCopyMemoryR32R0(ModuleBase, (ULONG_PTR)v5, SizeOfImage);
	if (IsAttach)
	{
		KeUnstackDetachProcess(&ApcState);
		IsAttach = FALSE;
	}
	if (NT_SUCCESS(Status))
	{
		Status = SafeCopyMemoryR02R3(v5, OutputBuffer, SizeOfImage);
	}
	if (v5)
	{
		FreePoolWithTag(v5, 0);
		v5 = NULL;
	}
	return Status;   //这里没有截到，不确定
}



NTSTATUS SafeCopyMemoryR32R0(ULONG_PTR Source, ULONG_PTR Destination, ULONG ViewSize)
{
	NTSTATUS Status = STATUS_UNSUCCESSFUL;
	ULONG v1 = 0;
	ULONG v2 = PAGE_SIZE - (Source & 0xFFF);   //剩余部分
	if (KeGetCurrentIrql() <= APC_LEVEL && Source <= USER_ADDRESS_END && Destination >= SYSTEM_ADDRESS_START && ViewSize > 0)
	{
		while (v1 < ViewSize)
		{
			PMDL Mdl = NULL;
			PVOID VirtualAddress = NULL;
			BOOLEAN IsOk = FALSE;
			if (ViewSize - v1 < v2)
			{
				v2 = ViewSize - v1;
			}
#ifdef _WIN64
			Mdl = IoAllocateMdl((PVOID)(Source & 0xFFFFFFFFFFFFF000), PAGE_SIZE, FALSE, FALSE, NULL);  //分配内存描述符列表 (MDL)
#else
			Mdl = IoAllocateMdl((PVOID)(Source & 0xFFFFF000), PAGE_SIZE, FALSE, FALSE, NULL);
#endif
			if (Mdl)
			{
				__try
				{
					//探测指定的虚拟内存页，使其驻留，并将其锁定在内存中， (例如 DMA 传输) 。 这可确保当设备驱动
					//程序 (或硬件) 仍在使用页面时，无法释放和重新分配页面。
					MmProbeAndLockPages(Mdl, UserMode, IoReadAccess);   //主要作用是确保在设备驱动程序或硬件使用内存页面时，这些页面不会被释放或重新分配
					IsOk = TRUE;
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					IsOk = FALSE;
				}
				if (IsOk)
				{
					/*
					MmGetSystemAddressForMdlSafe 是一个用于将内存描述符列表（MDL）中描述的内存映射到系统地址空间的函数，以便驱动程序可以访问该内存
					其主要功能是将用户模式缓冲区映射为系统地址，以便设备驱动程序能够读取或写入这些数据
					*/
					VirtualAddress = MmGetSystemAddressForMdlSafe(Mdl, NormalPagePriority);   
					if (VirtualAddress)
					{
						RtlCopyMemory((PVOID)Destination, (PVOID)((ULONG_PTR)VirtualAddress + (Source & 0xFFF)), v2);
					}
					MmUnlockPages(Mdl);  //解除锁定
				}
				IoFreeMdl(Mdl);  //释放Mdl
			}
			if (v1)
			{
				v2 = PAGE_SIZE;
			}
			v1 += v2;
			Source += v2;
			Destination += v2;
		}
		Status = STATUS_SUCCESS;
	}
	return Status;

}
NTSTATUS SafeCopyMemoryR02R3(ULONG_PTR Source, ULONG_PTR Destination, ULONG SizeOfImage)
{
	PMDL Mdl1 = NULL, Mdl2 = NULL;
	PUCHAR VirtualAddress1 = NULL, VirtualAddress2 = NULL;
	NTSTATUS Status = STATUS_UNSUCCESSFUL;
	Mdl1 = IoAllocateMdl(Source, SizeOfImage, FALSE, FALSE, NULL);
	if (!Mdl1)
	{
		return Status;
	}
	MmBuildMdlForNonPagedPool(Mdl1);  //接收指定非分页虚拟内存缓冲区的 MDL，并更新它以描述基础物理页。
	VirtualAddress1 = MmGetSystemAddressForMdlSafe(Mdl1, NormalPagePriority);
	if (!VirtualAddress1)
	{
		IoFreeMdl(Mdl1);
		return Status;
	}
	Mdl2 = IoAllocateMdl(Destination, SizeOfImage, FALSE, FALSE, NULL);
	if (!Mdl2)
	{
		IoFreeMdl(Mdl1);
		return Status;
	}
	__try
	{
		MmProbeAndLockPages(Mdl2, UserMode, IoWriteAccess);
		VirtualAddress2 = MmGetSystemAddressForMdlSafe(Mdl2, NormalPagePriority);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
	if (VirtualAddress2)
	{
		RtlCopyMemory(VirtualAddress2, VirtualAddress1, SizeOfImage);
		MmUnlockPages(Mdl2);
		Status = STATUS_SUCCESS;
	}
	IoFreeMdl(Mdl2);
	IoFreeMdl(Mdl1);
	return Status;

}

