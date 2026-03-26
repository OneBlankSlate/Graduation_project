#include <refs.hpp>
#include <etwhook_init.hpp>
#include <etwhook_manager.hpp>
#include <kstl/ksystem_info.hpp>

// IOCTL控制码定义
#define FILE_DEVICE_ETWHOOK 0x8000

#define IOCTL_PROTECT_TERMINATE \
    CTL_CODE(FILE_DEVICE_ETWHOOK, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_PROTECT_WRITE \
    CTL_CODE(FILE_DEVICE_ETWHOOK, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)



// 设备符号链接
#define ETWHOOK_DEVICE_NAME L"\\Device\\EtwHookDevice"
#define ETWHOOK_DOS_NAME L"\\DosDevices\\EtwHook"

// 保护配置结构
typedef struct _PROTECT_CONFIG {
	WCHAR ProcessName[256];  // 进程名
	BOOLEAN IsProtected;     // 是否受保护
} PROTECT_CONFIG, * PPROTECT_CONFIG;

// 设备扩展结构
typedef struct _DEVICE_EXTENSION {
	PROTECT_CONFIG TerminateProtect;  // 终止保护配置
	PROTECT_CONFIG WriteProtect;      // 写保护配置
	FAST_MUTEX Lock;                  // 互斥锁，保护配置访问
} DEVICE_EXTENSION, * PDEVICE_EXTENSION;

PDEVICE_EXTENSION __deviceExtension = NULL;
// 原始函数指针
NTSTATUS(*OriginalNtTerminateProcess)(
	_In_opt_ HANDLE ProcessHandle,
	_In_ NTSTATUS ExitStatus
	) = NULL;

NTSTATUS(*OriginalNtWriteVirtualMemory)(
	_In_ HANDLE ProcessHandle,
	_In_opt_ PVOID BaseAddress,
	_In_reads_bytes_opt_(BufferSize) PVOID Buffer,
	_In_ SIZE_T BufferSize,
	_Out_opt_ PSIZE_T NumberOfBytesWritten
	) = NULL;



typedef PIMAGE_NT_HEADERS64(NTAPI* PRTLIMAGENTHEADER)(PVOID);
PRTLIMAGENTHEADER GetRtlImageNtHeader() {
	UNICODE_STRING funcName = RTL_CONSTANT_STRING(L"RtlImageNtHeader");
	return (PRTLIMAGENTHEADER)MmGetSystemRoutineAddress(&funcName);
}
PRTLIMAGENTHEADER __pRtlImageNtHeader = NULL; 

typedef NTSTATUS (NTAPI* lpfn_NtWriteVirtualMemory)(
	_In_ HANDLE ProcessHandle,
	_In_ PVOID  BaseAddress,
	_In_ PVOID Buffer,
	_In_ SIZE_T NumberOfBytesToWrite,
	_Out_opt_ PSIZE_T NumberOfBytesWritten
	);
lpfn_NtWriteVirtualMemory _NtWriteVirtualMemory_ = NULL;

typedef NTSTATUS (NTAPI* lpfn_ZwTerminateProcess)(
	 HANDLE   ProcessHandle,
	          NTSTATUS ExitStatus
);
lpfn_ZwTerminateProcess _NtTerminateProcess_ = NULL;
typedef struct _SYSTEM_SERVICE_DESCRIPTOR_TABLE
{
	PULONG_PTR ServiceTableBase;   //指针数组
	PULONG_PTR ServiceCounterTableBase;
	ULONG_PTR NumberOfServices;    //多少个函数
	PULONG_PTR ParamterTableBase;
} SYSTEM_SERVICE_DESCRIPTOR_TABLE, * PSYSTEM_SERVICE_DESCRIPTOR_TABLE;

//宏11 
typedef struct _RTL_PROCESS_MODULE_INFORMATION
{
	HANDLE Section;
	PVOID MappedBase;
	PVOID ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	UCHAR  FullPathName[256];
} RTL_PROCESS_MODULE_INFORMATION, * PRTL_PROCESS_MODULE_INFORMATION;
typedef struct _RTL_PROCESS_MODULES
{
	ULONG NumberOfModules;
	RTL_PROCESS_MODULE_INFORMATION Modules[1];
} RTL_PROCESS_MODULES, * PRTL_PROCESS_MODULES;



//PVOID __Ntoskrnl = NULL;
//ULONG  __ImageSize = 0;
//UNICODE_STRING __NtoskrnlPath = { 0 };

NTSTATUS UnicodeStringCopy2UnicodeString(OUT PUNICODE_STRING DestinationString,
	IN PUNICODE_STRING SourceString)
{
	ASSERT(DestinationString != NULL && SourceString != NULL);
	if (DestinationString == NULL || SourceString == NULL || SourceString->Buffer == NULL)
		return STATUS_INVALID_PARAMETER;
	if (SourceString->Length == 0)
	{
		DestinationString->Length = DestinationString->MaximumLength = 0;
		DestinationString->Buffer = NULL;
		return STATUS_SUCCESS;
	}

	DestinationString->Buffer = (PWCH)ExAllocatePool(PagedPool, SourceString->MaximumLength);
	DestinationString->Length = SourceString->Length;
	DestinationString->MaximumLength = SourceString->MaximumLength;

	memcpy(DestinationString->Buffer, SourceString->Buffer, SourceString->Length);

	return STATUS_SUCCESS;
}

PVOID GetNtoskrnlInfo1(OUT PUNICODE_STRING NtoskrnlPath, OUT PULONG ImageSize)
{

	NTSTATUS Status = STATUS_SUCCESS;
	ULONG ReturnLength = 0;
	PRTL_PROCESS_MODULES RtlProcessModules = NULL;
	PVOID v1 = NULL;
	UNICODE_STRING FunctionName;
	ANSI_STRING v2;
	PVOID Ntoskrnl = NULL;

	//从系统第1个模块中的导出表中获取ZwOpenProcess
	RtlInitUnicodeString(&FunctionName, L"ZwOpenProcess");

	//从模块导出表中获得地址
	v1 = MmGetSystemRoutineAddress(&FunctionName);
	if (v1 == NULL)
	{
		return NULL;
	}

	//预查系统信息
	Status = ZwQuerySystemInformation(SystemModuleInformation, 0, ReturnLength, &ReturnLength);
	if (ReturnLength == 0)
	{

		return NULL;
	}
	//动态申请内存  
	RtlProcessModules = (PRTL_PROCESS_MODULES)ExAllocatePool(PagedPool, ReturnLength);
	RtlZeroMemory(RtlProcessModules, ReturnLength);

	Status = ZwQuerySystemInformation(SystemModuleInformation, RtlProcessModules, ReturnLength, &ReturnLength);
	if (NT_SUCCESS(Status))
	{
		//
		PRTL_PROCESS_MODULE_INFORMATION RtlProcessModuleInfo = RtlProcessModules->Modules;

		for (ULONG i = 0; i < RtlProcessModules->NumberOfModules; i++)
		{

			if (v1 >= RtlProcessModuleInfo[i].ImageBase &&
				v1 < (PVOID)((PUCHAR)RtlProcessModuleInfo[i].ImageBase + RtlProcessModuleInfo[i].ImageSize))
			{
				//获取到了系统第1模块信息
				Ntoskrnl = RtlProcessModuleInfo[i].ImageBase;
				RtlInitAnsiString(&v2, (PCSZ)RtlProcessModuleInfo[i].FullPathName);

				//单字转换为UnicodeString
				RtlAnsiStringToUnicodeString(NtoskrnlPath, &v2, TRUE);

				if (ImageSize)
				{
					*ImageSize = RtlProcessModuleInfo[i].ImageSize;;
				}

				break;
			}
		}
	}
	if (RtlProcessModules)
	{
		ExFreePool(RtlProcessModules);
	}


	return Ntoskrnl;
}
BOOLEAN
MappingFileInRing0Space(WCHAR* FullPath,
	PVOID* VirtualAddress, PSIZE_T ViewSize)
{

	NTSTATUS          Status;
	UNICODE_STRING    v1;
	OBJECT_ATTRIBUTES ObjectAttributes;
	IO_STATUS_BLOCK   IoStatusBlock;
	HANDLE   FileHandle = NULL;
	HANDLE   SectionHandle = NULL;

	if (!FullPath && MmIsAddressValid(FullPath))
	{
		return FALSE;
	}

	if (!VirtualAddress && MmIsAddressValid(VirtualAddress))
	{
		return FALSE;
	}


	//将文件路径转换成UNICODE_STRING存储
	RtlInitUnicodeString(&v1, FullPath);

	//根据UNICODE_STRING创建对象属性
	InitializeObjectAttributes(&ObjectAttributes,
		&v1,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL,
		NULL
	);

	//获得文件句柄
	//zwcrete

	Status = IoCreateFile(&FileHandle,
		GENERIC_READ | SYNCHRONIZE,
		&ObjectAttributes,   //文件绝对路径
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
		0x1000000, //内存对齐  0x1000
		FileHandle
	);
	ZwClose(FileHandle);
	if (!NT_SUCCESS(Status))
	{
		return FALSE;
	}
	Status = ZwMapViewOfSection(SectionHandle,
		NtCurrentProcess(),    //映射到当前进程的内存空间中
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
	return TRUE;
}
BOOLEAN GetNtXXXServcieIndex(CHAR* FunctionName,
	ULONG32* ServiceIndex)
{
	/*
	0:007> u ZwTerminateProcess
ntdll!NtTerminateProcess:
00000000`77621570 4c8bd1          mov     r10,rcx
00000000`77621573 b829000000      mov     eax,29h
00000000`77621578 0f05            syscall
00000000`7762157a c3              ret
	*/

	ULONG    i;
	BOOLEAN  IsOk = FALSE;
	WCHAR	 FileFullPath[] = L"\\SystemRoot\\System32\\ntdll.dll";     //C:\Windows\  
	PVOID    VirtualAddress = NULL;
	SIZE_T   ViewSize = 0;
	PIMAGE_EXPORT_DIRECTORY ImageExportDirectory = NULL;
	PIMAGE_NT_HEADERS  ImageNtHeaders = NULL;
	UINT32* AddressOfFunctions = NULL;
	UINT32* AddressOfNames = NULL;
	UINT16* AddressOfNameOrdinals = NULL;
	CHAR* v1 = NULL;
	ULONG32  FunctionOrdinal = 0;
	PVOID    FunctionAddress = 0;


#ifdef _WIN64
	ULONG32  Offset = 4;
#else 
	ULONG32  Offset = 1;
#endif // 

	* ServiceIndex = -1;
	IsOk = MappingFileInRing0Space(FileFullPath,
		&VirtualAddress, &ViewSize);
	if (IsOk == FALSE)
	{
		return FALSE;
	}
	else
	{
		__try {
			ImageNtHeaders = __pRtlImageNtHeader(VirtualAddress);

			if (ImageNtHeaders && ImageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress)
			{
				ImageExportDirectory = (IMAGE_EXPORT_DIRECTORY*)((UINT8*)VirtualAddress +
					ImageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

				AddressOfFunctions = (UINT32*)((UINT8*)VirtualAddress + ImageExportDirectory->AddressOfFunctions);
				AddressOfNames = (UINT32*)((UINT8*)VirtualAddress + ImageExportDirectory->AddressOfNames);
				AddressOfNameOrdinals = (UINT16*)((UINT8*)VirtualAddress + ImageExportDirectory->AddressOfNameOrdinals);
				for (i = 0; i < ImageExportDirectory->NumberOfNames; i++)
				{
					v1 = (char*)((ULONG_PTR)VirtualAddress + AddressOfNames[i]);   //获得函数名称
					if (_stricmp(FunctionName, v1) == 0)
					{
						FunctionOrdinal = AddressOfNameOrdinals[i];
						FunctionAddress = (PVOID)((UINT8*)VirtualAddress + AddressOfFunctions[FunctionOrdinal]);


						*ServiceIndex = *(ULONG32*)((UINT8*)FunctionAddress + Offset);
						break;
					}
				}
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			;
		}
	}
	ZwUnmapViewOfSection(NtCurrentProcess(), VirtualAddress);  //解除映射
	if (*ServiceIndex == -1)
	{
		return FALSE;
	}

	return TRUE;
}

PSYSTEM_SERVICE_DESCRIPTOR_TABLE GetKeServiceDescriptorTable()
{
	PSYSTEM_SERVICE_DESCRIPTOR_TABLE SystemServiceDescriptorTable = NULL;
#ifdef  _WIN64
	PUCHAR v10 = (PUCHAR)__readmsr(0xC0000082);
	PUCHAR KiSystemCall64 = v10;
	PUCHAR KiSystemCall64Shadow = v10;
	PUCHAR StartAddress = 0, EndAddress = 0;
	PUCHAR i = NULL;

	UCHAR v1, v2, v3, v4, v5;
	ULONG OffsetSsdt = 0;
	INT OffsetUser = 0;
	DbgPrint(("[zsh]Msr C0000082:%x\n"), v10);
	if (*(v10 + 0x9) == 0x00) //走这里说明Msr C0000082得到的是KiSystemCall64    win7与部分win10（如22H2）
	{
		StartAddress = KiSystemCall64;
		EndAddress = StartAddress + PAGE_SIZE;
	}
	else if (*(v10 + 0x9) == 0x70) //走这里说明Msr C0000082得到的是KiSystemCall64Shadow  win10-20H2
	{
		PUCHAR EndSearchUser = KiSystemCall64Shadow + PAGE_SIZE;

		for (i = KiSystemCall64Shadow; i < EndSearchUser; i++)
		{
			if (MmIsAddressValid(i) && MmIsAddressValid(i + 5))
			{
				v4 = *i;
				v5 = *(i + 5);
				if (v4 == 0xe9 && v5 == 0xc3)
				{
					memcpy(&OffsetUser, i + 1, 4);
					StartAddress = OffsetUser + (i + 5);
					KdPrint(("KiSystemServiceUser:%x\n", StartAddress));
					EndAddress = StartAddress + PAGE_SIZE;
				}
			}
		}
	}
	//硬编码搜索4c 8d 15
	for (i = StartAddress; i < EndAddress; i++)
	{
		if (MmIsAddressValid(i) && MmIsAddressValid(i + 1) && MmIsAddressValid(i + 2))
		{
			v1 = *i;
			v2 = *(i + 1);
			v3 = *(i + 2);
			if (v1 == 0x4c && v2 == 0x8d && v3 == 0x15)
			{
				memcpy(&OffsetSsdt, i + 3, 4);
				SystemServiceDescriptorTable = (PSYSTEM_SERVICE_DESCRIPTOR_TABLE)((ULONG_PTR)OffsetSsdt + (ULONG_PTR)i + 7);
				return SystemServiceDescriptorTable;
			}
		}
	}
	return SystemServiceDescriptorTable;
#else	
	extern  PSYSTEM_SERVICE_DESCRIPTOR_TABLE KeServiceDescriptorTable;    //扩展声明
	return KeServiceDescriptorTable;
#endif //  _WIN64
}
NTSTATUS GetNtXXXServcieAddress(ULONG_PTR ServiceIndex, PVOID* FunctionAddress)
{
	PVOID v2 = NULL;
	PVOID* ServiceTableBase = NULL;
	PSYSTEM_SERVICE_DESCRIPTOR_TABLE SystemServiceDescriptorTable = GetKeServiceDescriptorTable();

	ServiceTableBase = (PVOID*)(SystemServiceDescriptorTable->ServiceTableBase);

	if (ServiceTableBase != NULL)
	{
#ifdef  _WIN64
		if (ServiceIndex <= SystemServiceDescriptorTable->NumberOfServices)
		{
			LONG v1 = ((PULONG)ServiceTableBase)[ServiceIndex];
			v1 = v1 >> 4;
			v2 = (PVOID)((ULONGLONG)ServiceTableBase + (LONGLONG)v1);
		}
#else
		if (ServiceIndex <= SystemServiceDescriptorTable->NumberOfServices)
		{
			v2 = (SystemServiceDescriptorTable->ServiceTableBase)[ServiceIndex];
		}
#endif //  _WIN64

		//对该函数地址进行校验
		if (MmIsAddressValid(v2))
		{
			if (FunctionAddress != NULL)
			{
				*FunctionAddress = v2;
				return STATUS_SUCCESS;
			}
		}
	}
	return STATUS_UNSUCCESSFUL;

}
NTSTATUS detour_NtCreateFile(
	_Out_ PHANDLE FileHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES ObjectAttributes,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_In_opt_ PLARGE_INTEGER AllocationSize,
	_In_ ULONG FileAttributes,
	_In_ ULONG ShareAccess,
	_In_ ULONG CreateDisposition,
	_In_ ULONG CreateOptions,
	_In_reads_bytes_opt_(EaLength) PVOID EaBuffer,
	_In_ ULONG EaLength) {

	if (ObjectAttributes &&
		ObjectAttributes->ObjectName &&
		ObjectAttributes->ObjectName->Buffer)
	{
		wchar_t* name = (wchar_t*)ExAllocatePoolWithTag(NonPagedPool, ObjectAttributes->ObjectName->Length + sizeof(wchar_t),'lala');
		
		if (name)
		{
			RtlZeroMemory(name, ObjectAttributes->ObjectName->Length + sizeof(wchar_t));
			RtlCopyMemory(name, ObjectAttributes->ObjectName->Buffer, ObjectAttributes->ObjectName->Length);

			if (wcsstr(name, L"oxygen.txt"))
			{
				ExFreePool(name);
				return STATUS_ACCESS_DENIED;
			}

			ExFreePool(name);
		}
	}


	return NtCreateFile(FileHandle, DesiredAccess, ObjectAttributes, \
		IoStatusBlock, AllocationSize, FileAttributes, ShareAccess, \
		CreateDisposition, CreateOptions, EaBuffer, EaLength);
}


NTSTATUS detour_NtClose(HANDLE h) {

	//LOG_INFO("ZwClose was Caguth\r\n");

	return NtClose(h);

}
// Hook 函数
//NTSTATUS NTAPI detour_NtTerminateProcess(
//	_In_opt_ HANDLE ProcessHandle,
//	_In_ NTSTATUS ExitStatus
//) {
//	DbgPrint("[HOOK] NtTerminateProcess 被调用\n");
//
//	// 阻止进程终止
//	return STATUS_ACCESS_VIOLATION;  // 或者 STATUS_ACCESS_DENIED
//
//	// 如果允许继续执行原始函数：
//	// return ((ZWTERMINATEPROCESS)g_OriginalZwTerminateProcess)(
//	//     ProcessHandle, ExitStatus);
//}

VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
	__debugbreak();
	EtwHookManager::get_instance()->remove_hook(OriginalNtTerminateProcess);
	EtwHookManager::get_instance()->remove_hook(OriginalNtWriteVirtualMemory);
	EtwHookManager::get_instance()->destory();
	UNICODE_STRING dosName;
	// 删除符号链接
	RtlInitUnicodeString(&dosName, ETWHOOK_DOS_NAME);
	IoDeleteSymbolicLink(&dosName);

	// 删除设备对象
	if (DriverObject->DeviceObject) {
		IoDeleteDevice(DriverObject->DeviceObject);
	}
	return VOID();
}

NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}
NTSTATUS GetProcessImageName(PEPROCESS Process, PWCHAR ProcessName, ULONG BufferSize)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	ULONG returnedLength = 0;
	PUNICODE_STRING imagePath = NULL;

	if (!Process || !ProcessName || BufferSize < sizeof(WCHAR)) {
		return STATUS_INVALID_PARAMETER;
	}

	// 获取进程映像路径
	status = SeLocateProcessImageName(Process, &imagePath);
	if (!NT_SUCCESS(status) || !imagePath) {
		return status;
	}

	// 提取文件名
	PWCHAR fileName = wcsrchr(imagePath->Buffer, L'\\');
	if (fileName) {
		fileName++;  // 跳过反斜杠
	}
	else {
		fileName = imagePath->Buffer;
	}

	// 复制进程名
	ULONG nameLength = (ULONG)(wcslen(fileName) + 1) * sizeof(WCHAR);
	if (nameLength <= BufferSize) {
		RtlCopyMemory(ProcessName, fileName, nameLength);
		status = STATUS_SUCCESS;
	}
	else {
		status = STATUS_BUFFER_TOO_SMALL;
	}

	ExFreePool(imagePath);
	return status;
}
BOOLEAN IsProcessProtected(PEPROCESS TargetProcess, PROTECT_CONFIG* ProtectConfig)
{
	WCHAR processName[256] = { 0 };
	NTSTATUS status;

	if (!ProtectConfig->IsProtected) {
		return FALSE;
	}

	// 获取目标进程名
	status = GetProcessImageName(TargetProcess, processName, sizeof(processName));
	if (!NT_SUCCESS(status)) {
		return FALSE;
	}

	// 比较进程名
	if (_wcsicmp(processName, ProtectConfig->ProcessName) == 0) {
		return TRUE;
	}

	return FALSE;
}


NTSTATUS detour_NtTerminateProcess(HANDLE ProcessHandle, NTSTATUS ExitStatus)
{
	PEPROCESS targetProcess = NULL;
	NTSTATUS status = STATUS_SUCCESS;

	// 获取目标进程对象
	if (ProcessHandle) {
		status = ObReferenceObjectByHandle(
			ProcessHandle,
			PROCESS_ALL_ACCESS,
			*PsProcessType,
			KernelMode,
			(PVOID*)&targetProcess,
			NULL
		);

		if (!NT_SUCCESS(status)) {
			return status;
		}
	}
	else {
		// 如果ProcessHandle为NULL，表示终止当前进程
		targetProcess = PsGetCurrentProcess();
	}

	// 获取设备扩展（这里需要全局访问）
	// 实际实现中需要保存设备扩展的全局指针
	if (__deviceExtension) {
		//ExAcquireFastMutex(&__deviceExtension->Lock);

		// 检查是否受终止保护
		if (IsProcessProtected(targetProcess, &__deviceExtension->TerminateProtect)) {
			status = STATUS_ACCESS_DENIED;
		}
		else {
			// 调用原始函数
			if (OriginalNtTerminateProcess) {
				status = OriginalNtTerminateProcess(ProcessHandle, ExitStatus);
			}
			else {
				status = STATUS_UNSUCCESSFUL;
			}
		}

		//ExReleaseFastMutex(&__deviceExtension->Lock);
	}
	else {
		// 如果没有设备扩展，直接调用原始函数
		if (OriginalNtTerminateProcess) {
			status = OriginalNtTerminateProcess(ProcessHandle, ExitStatus);
		}
		else {
			status = STATUS_UNSUCCESSFUL;
		}
	}

	// 释放进程对象引用
	if (ProcessHandle && targetProcess) {
		ObDereferenceObject(targetProcess);
	}

	return status;
}

NTSTATUS detour_NtWriteVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, SIZE_T BufferSize, PSIZE_T NumberOfBytesWritten)
{
	PEPROCESS targetProcess = NULL;
	NTSTATUS status = STATUS_SUCCESS;

	// 获取目标进程对象
	status = ObReferenceObjectByHandle(
		ProcessHandle,
		PROCESS_ALL_ACCESS,
		*PsProcessType,
		KernelMode,
		(PVOID*)&targetProcess,
		NULL
	);

	if (!NT_SUCCESS(status)) {
		return status;
	}

	// 获取设备扩展
	if (__deviceExtension) {
		//ExAcquireFastMutex(&__deviceExtension->Lock);

		// 检查是否受写保护
		if (IsProcessProtected(targetProcess, &__deviceExtension->WriteProtect)) {
			status = STATUS_ACCESS_DENIED;
		}
		else {
			// 调用原始函数
			if (OriginalNtWriteVirtualMemory) {
				status = OriginalNtWriteVirtualMemory(
					ProcessHandle,
					BaseAddress,
					Buffer,
					BufferSize,
					NumberOfBytesWritten
				);
			}
			else {
				status = STATUS_UNSUCCESSFUL;
			}
		}

		//ExReleaseFastMutex(&__deviceExtension->Lock);
	}
	else {
		// 如果没有设备扩展，直接调用原始函数
		if (OriginalNtWriteVirtualMemory) {
			status = OriginalNtWriteVirtualMemory(
				ProcessHandle,
				BaseAddress,
				Buffer,
				BufferSize,
				NumberOfBytesWritten
			);
		}
		else {
			status = STATUS_UNSUCCESSFUL;
		}
	}

	// 释放进程对象引用
	ObDereferenceObject(targetProcess);

	return status;
}
NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION irpStack;
	PPROTECT_CONFIG inputBuffer;
	ULONG inputBufferLength;
	ULONG outputBufferLength;
	ULONG ioControlCode;

	// 获取IRP栈位置
	irpStack = IoGetCurrentIrpStackLocation(Irp);
	// 获取IOCTL参数
	ioControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;
	inputBufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
	outputBufferLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

	// 获取输入缓冲区
	inputBuffer = (PPROTECT_CONFIG)Irp->AssociatedIrp.SystemBuffer;


	CHAR FunctionName[] = "NtTerminateProcess";
	ULONG32 Index = 0;


	CHAR FunctionName2[] = "NtWriteVirtualMemory";
	ULONG32 Index2 = 0;


	// 处理不同的IOCTL
	switch (ioControlCode) {
	case IOCTL_PROTECT_TERMINATE:


		GetNtXXXServcieIndex(FunctionName, &Index);
		GetNtXXXServcieAddress(Index, (PVOID*)&OriginalNtTerminateProcess);
		EtwHookManager::get_instance()->add_hook(OriginalNtTerminateProcess, detour_NtTerminateProcess);


		if (inputBufferLength >= sizeof(PROTECT_CONFIG)) {
			// 加锁保护配置访问
			//ExAcquireFastMutex(&__deviceExtension->Lock);

			// 复制进程名
			RtlCopyMemory(__deviceExtension->TerminateProtect.ProcessName,
				inputBuffer->ProcessName,
				sizeof(inputBuffer->ProcessName));
			__deviceExtension->TerminateProtect.IsProtected = TRUE;

			//ExReleaseFastMutex(&__deviceExtension->Lock);
			status = STATUS_SUCCESS;
		}
		else {
			status = STATUS_BUFFER_TOO_SMALL;
		}
		break;

	case IOCTL_PROTECT_WRITE:

		GetNtXXXServcieIndex(FunctionName2, &Index2);
		GetNtXXXServcieAddress(Index2, (PVOID*)&OriginalNtWriteVirtualMemory);
		EtwHookManager::get_instance()->add_hook(OriginalNtWriteVirtualMemory, detour_NtWriteVirtualMemory);


		if (inputBufferLength >= sizeof(PROTECT_CONFIG)) {

			//ExAcquireFastMutex(&__deviceExtension->Lock);

			RtlCopyMemory(__deviceExtension->WriteProtect.ProcessName,
				inputBuffer->ProcessName,
				sizeof(inputBuffer->ProcessName));
			__deviceExtension->WriteProtect.IsProtected = TRUE;

			//ExReleaseFastMutex(&__deviceExtension->Lock);
			status = STATUS_SUCCESS;
		}
		else {
			status = STATUS_BUFFER_TOO_SMALL;
		}
		break;

	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	// 完成IRP
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}




EXTERN_C NTSTATUS DriverEntry(PDRIVER_OBJECT drv, PUNICODE_STRING)
{
	NTSTATUS status = STATUS_SUCCESS;
	__pRtlImageNtHeader = GetRtlImageNtHeader();
	drv->DriverUnload = DriverUnload;
	kstd::Logger::init("etw_hook", nullptr);
	status = EtwHookManager::get_instance()->init();
	UNICODE_STRING deviceName, dosName;

	// 设置派遣函数
	drv->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
	drv->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;
	drv->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;

	// 创建设备对象
	RtlInitUnicodeString(&deviceName, ETWHOOK_DEVICE_NAME);
	RtlInitUnicodeString(&dosName, ETWHOOK_DOS_NAME);

	PDEVICE_OBJECT deviceObject = NULL;
	status = IoCreateDevice(
		drv,
		sizeof(DEVICE_EXTENSION),
		&deviceName,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&deviceObject
	);

	if (!NT_SUCCESS(status)) {
		return status;
	}

	// 创建设备符号链接
	status = IoCreateSymbolicLink(&dosName, &deviceName);
	if (!NT_SUCCESS(status)) {
		IoDeleteDevice(deviceObject);
		return status;
	}

	// 初始化设备扩展
	__deviceExtension = (PDEVICE_EXTENSION)deviceObject->DeviceExtension;
	RtlZeroMemory(__deviceExtension, sizeof(DEVICE_EXTENSION));
	//ExInitializeFastMutex(&__deviceExtension->Lock);

	// 设置设备标志
	deviceObject->Flags |= DO_BUFFERED_IO;
	//deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	return STATUS_SUCCESS;
}