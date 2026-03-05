#include"SystemHelper.h"
#include"MemoryHelper.h"
#include"StringHelper.h"
#include"ProcessHelper.h"
PEPROCESS __SystemEProcess = NULL;
PVOID __Ntoskrnl = NULL;
ULONG __ImageSize = 0;
UNICODE_STRING __NtoskrnlPath = { 0 };
PSYSTEM_SERVICE_DESCRIPTOR_TABLE __SystemServiceDescriptorTable = NULL;
PVOID GetNtoskrnlInfo(OUT PUNICODE_STRING NtoskrnlPath, OUT PULONG ImageSize)
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG ReturnLength = 0;
	PRTL_PROCESS_MODULES RtlProcessModules = NULL;
	PVOID v1 = NULL;
	UNICODE_STRING ServiceName;
	ANSI_STRING v2;
	if (__Ntoskrnl != NULL)
	{
		if (ImageSize)
			*ImageSize = __ImageSize;
		if (NtoskrnlPath)
			UnicodeStringCopy2UnicodeString(NtoskrnlPath, &NtoskrnlPath);
		return __Ntoskrnl;
	}
	//从系统第1个模块中的导出表中获取ZwOpenProcess
	RtlInitUnicodeString(&ServiceName, L"ZwOpenProcess");
	v1 = MmGetSystemRoutineAddress(&ServiceName);
	if (v1 == NULL)
		return NULL;

	// 预查系统信息
	Status = ZwQuerySystemInformation(SystemModuleInformation, 0, ReturnLength, &ReturnLength);
	if(ReturnLength == 0)
		return NULL;
	
	RtlProcessModules = (PRTL_PROCESS_MODULES)AllocatePoolWithTag(PagedPool, ReturnLength);
	RtlZeroMemory(RtlProcessModules, ReturnLength);

	Status = ZwQuerySystemInformation(SystemModuleInformation, RtlProcessModules, ReturnLength, &ReturnLength);
	if (NT_SUCCESS(Status))
	{

		PRTL_PROCESS_MODULE_INFORMATION RtlProcessModuleInfo = RtlProcessModules->Modules;
		for (ULONG i = 0; i < RtlProcessModules->NumberOfModules; i++)
		{
			if (v1 > RtlProcessModuleInfo[i].ImageBase &&
				v1 < (PVOID)((PUCHAR)RtlProcessModuleInfo[i].ImageBase + RtlProcessModuleInfo[i].ImageSize))
			{
				// 获取到了系统第1模块信息
				__Ntoskrnl = RtlProcessModuleInfo[i].ImageBase;
				__ImageSize = RtlProcessModuleInfo[i].ImageSize;
				RtlInitAnsiString(&v2, RtlProcessModuleInfo[i].FullPathName);
				//单字转换为UnicodeString
				RtlAnsiStringToUnicodeString(&__NtoskrnlPath, &v2, TRUE);
				if (ImageSize)
				{
					*ImageSize = __ImageSize;
				}
				if (NtoskrnlPath)
				{
					UnicodeStringCopy2UnicodeString(NtoskrnlPath, &__NtoskrnlPath);
				}
				break;
					
			}

				
		}
			
	}
	if (RtlProcessModules)
	{
		ExFreePool(RtlProcessModules);
	}
	return __Ntoskrnl;
}
//通过遍历节区找ssdt
PSYSTEM_SERVICE_DESCRIPTOR_TABLE GetKeServiceDescriptorTable1()
{
#ifdef _WIN64
	ULONG ImageSize = 0;
	UNICODE_STRING NtoskrnlPath;
	PUCHAR Ntoskrnl = NULL;
	if (!__Ntoskrnl)
	{
		Ntoskrnl = GetNtoskrnlInfo(&NtoskrnlPath, &ImageSize);
	}

	else
	{
		Ntoskrnl = __Ntoskrnl;
	}
		PIMAGE_NT_HEADERS64 ImageNtHeaders = RtlImageNtHeader(Ntoskrnl);
		PIMAGE_SECTION_HEADER ImageSectionHeader = (PIMAGE_SECTION_HEADER)(ImageNtHeaders + 1);  //可以
		for (PIMAGE_SECTION_HEADER v1 = ImageSectionHeader; v1 < ImageSectionHeader + ImageNtHeaders->FileHeader.NumberOfSections; v1++)
		{
			//未分页  可执行  未丢弃
			if (v1->Characteristics & IMAGE_SCN_MEM_NOT_PAGED &&
				v1->Characteristics & IMAGE_SCN_MEM_EXECUTE &&
				!(v1->Characteristics & IMAGE_SCN_MEM_DISCARDABLE) &&
				(*(PULONG)v1->Name != 'TINI') &&
				(*(PULONG)v1->Name != 'EGAP'))
			{
				PVOID ReturnAddress = NULL;
				UCHAR PatternValue[] = "\x4c\x8d\x15\xcc\xcc\xcc\xcc\x4c\x8d\x1d\xcc\xcc\xcc\xcc\xf7";
				NTSTATUS Status = SearchPattern(
					PatternValue,
					0xCC,
					sizeof(PatternValue) - 1,
					Ntoskrnl + ImageSectionHeader->VirtualAddress,  //from
					ImageSectionHeader->Misc.VirtualSize, 			//To
					&ReturnAddress);
					
				if (NT_SUCCESS(Status))
				{
					__SystemServiceDescriptorTable = (PSYSTEM_SERVICE_DESCRIPTOR_TABLE)((PUCHAR)ReturnAddress +  *(PULONG)((PUCHAR)ReturnAddress + 3) + 7);
					return __SystemServiceDescriptorTable;
				}
			}
					
		}
#else
	extern PSYSTEM_SERVICE_DESCRIPTOR_TABLE KeServiceDescriptorTable;  //在x86中是一个全局可见的变量
	__SystemServiceDescriptorTable = KeServiceDescriptorTable;
	
	return __SystemServiceDescriptorTable;
#endif   //_WIN64

	return NULL;
}
//不用节区，直接rdmsr 0xC0000082来获取KiSystemCall64入口来找ssdt
PSYSTEM_SERVICE_DESCRIPTOR_TABLE GetKeServiceDescriptorTable2()
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
BOOLEAN GetNtXXXServiceIndex(CHAR* FunctionName, ULONG32* ServiceIndex)
{
	ULONG i;
	BOOLEAN IsOk = FALSE;
	WCHAR FileFullPath[] = L"\\SystemRoot\\System32\\ntdll.dll";
	PVOID VirtualAddress = NULL;
	SIZE_T ViewSize = 0;
	PIMAGE_EXPORT_DIRECTORY ImageExportDirectory = NULL;
	PIMAGE_NT_HEADERS ImageNtHeaders = NULL;
	UINT32* AddressOfFunctions = NULL;
	UINT32* AddressOfNames = NULL;
	UINT16* AddressOfNameOrdinals = NULL;
	CHAR* v1 = NULL;
	ULONG32 FunctionOrdinal = 0;
	PVOID FunctionAddress = 0;
#ifdef _WIN64
	ULONG32 Offset = 4;
#else
	ULONG32 Offset = 1;
#endif 
	*ServiceIndex = -1;
	IsOk = MapFileInKernelSpace(FileFullPath,
		&VirtualAddress, &ViewSize);
		if (IsOk = FALSE)
			return FALSE;
		else
		{
			__try
			{
				ImageNtHeaders = RtlImageNtHeader(VirtualAddress);
				if (ImageNtHeaders && ImageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress)
				{
					ImageExportDirectory = (IMAGE_EXPORT_DIRECTORY*)((UINT8*)VirtualAddress +
						ImageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
					AddressOfFunctions    = (UINT32*)((UINT8*)VirtualAddress + ImageExportDirectory->AddressOfFunctions);
					AddressOfNames        = (UINT32*)((UINT8*)VirtualAddress + ImageExportDirectory->AddressOfNames);
					AddressOfNameOrdinals = (UINT16*)((UINT8*)VirtualAddress + ImageExportDirectory->AddressOfNameOrdinals);
					for (i = 0; i < ImageExportDirectory->NumberOfNames; i++)
					{
						v1 = (char*)((ULONG_PTR)VirtualAddress + AddressOfNames[i]);//获得函数名称
						if (_stricmp(FunctionName, v1) == 0)
						{
							FunctionOrdinal = AddressOfNameOrdinals[i];
							FunctionAddress = (PVOID)((UINT8*)VirtualAddress + AddressOfFunctions[FunctionOrdinal]);
							*ServiceIndex = *(ULONG32*)((UINT8*)FunctionAddress + Offset);  //这里用*(ULONG32*)没问题是因为无论32位还是64位服务索引都是4字节
							break;
						}
						
					}
						
				}
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{

			}
		}
		ZwUnmapViewOfSection(NtCurrentProcess(), VirtualAddress);  //解除映射
		if (*ServiceIndex == -1)
		{
			return FALSE;
		}
		return TRUE;
			
}
NTSTATUS GetNtXXXServiceAddress(ULONG_PTR ServiceIndex, PVOID* ServiceAddress)
{
	PVOID v2 = NULL;
	PSYSTEM_SERVICE_DESCRIPTOR_TABLE SystemServiceDescriptorTable = NULL;
	PVOID* ServiceTableBase = NULL;
	if (__SystemServiceDescriptorTable == NULL)
	{
		SystemServiceDescriptorTable = GetKeServiceDescriptorTable2();
	}

	else
	{
		SystemServiceDescriptorTable = __SystemServiceDescriptorTable;
	}
	ServiceTableBase = (PVOID*)(SystemServiceDescriptorTable->ServiceTableBase);
	if (ServiceTableBase != NULL)
	{
#ifdef _WIN64
		if (ServiceIndex <= SystemServiceDescriptorTable->NumberOfServices)
		{
			LONG v1 = ((PULONG)ServiceTableBase)[ServiceIndex];
			v1 = v1 >> 4;
			v2 = (ULONGLONG)ServiceTableBase + (LONGLONG)v1;
		}
#else
		if (ServiceIndex <= SystemServiceDescriptorTable->NumberOfServices)
		{
			v2 = (SystemServiceDescriptorTable->ServiceTableBase)[ServiceIndex];
		}
#endif
		//对该函数地址进行校验
		if (MmIsAddressValid(v2))
		{
			if (ServiceAddress != NULL)
			{
				*ServiceAddress = v2;
				return STATUS_SUCCESS;
			}
		}
	}
	return STATUS_UNSUCCESSFUL;
}


VOID FreeNtoskrnlInfo()
{
	if (__NtoskrnlPath.Buffer != NULL && __NtoskrnlPath.Length != 0)
	{
		RtlFreeUnicodeString(&__NtoskrnlPath);
	}
}

PEPROCESS LookupWin32Process()
{
	PEPROCESS EProcess;
	ULONG ProcessIdentity;
	for (ProcessIdentity = 100; ProcessIdentity < 5000; ProcessIdentity += 4)
	{
		if (PsLookupProcessByProcessId((HANDLE)ProcessIdentity, &EProcess) == STATUS_SUCCESS)
		{
			if (PsGetProcessWin32Process(EProcess))   //寻找带有界面的进程
			{
				return EProcess;
			}
				
			
		}
	}
	return NULL;
}
VOID InitializeSystemSource()
{
	__SystemEProcess = PsGetCurrentProcess();
}
VOID UninitializeSystemSource()
{
	__SystemEProcess = NULL;
}
//游戏驱动中的GetFunc
PVOID64 GetSSDTAddre()
{
	PUCHAR msr = 0;
	PUCHAR StartAddre = 0, EndAddre = 0;            // 开始遍历的地方,和遍历长度,获取到SSDT的位置
	UCHAR b0 = 0, b1 = 0, b2 = 0, b7 = 0, b8 = 0, b9 = 0, b14 = 0, b15 = 0;
	ULONG deviation = 0;      //偏差                              //KiSystemServiceRepeat+7到SSDT的偏移
	ULONGLONG SSDTAddre = 0;

	msr = (PUCHAR)__readmsr(0xC0000082);
	//KiSystemCall64往下遍历  搜特征码【4C 8D 15 XX XX XX XX 4C 8D 1D XX XX XX XX F7 43】找到KiSystemServiceRepeat
	// 第二个 4C 8D 1D XX XX XX XX 是Shadow SSDT地址
	StartAddre = msr, EndAddre = msr + 0x500;
	for (; StartAddre < EndAddre; StartAddre++)
		if (MmIsAddressValid(StartAddre) && MmIsAddressValid(StartAddre + 1) && MmIsAddressValid(StartAddre + 2))
		{
			b0 = *StartAddre;
			b1 = *(StartAddre + 1);
			b2 = *(StartAddre + 2);
			b7 = *(StartAddre + 7);
			b8 = *(StartAddre + 8);
			b9 = *(StartAddre + 9);
			b14 = *(StartAddre + 14);
			b15 = *(StartAddre + 15);
			if (b0 == 0x4C && b1 == 0x8d && b2 == 0x15 && b7 == 0x4C && b8 == 0x8d && b9 == 0x1D && b14 == 0xF7 && b15 == 0x43)
			{
				memcpy(&deviation, StartAddre + 3, 4);
				SSDTAddre = (ULONGLONG)deviation + (ULONGLONG)StartAddre + 7;
				DbgPrint("[dk]: KiSystemCall64:=%p KiSystemServiceRepeat=%p SSDT=%p\n", msr, StartAddre, (PULONG64)SSDTAddre);
				return (PVOID64)SSDTAddre;
			}
		}


	DbgPrint("[dk]: No find SSDT\n");
	return NULL;
}
PVOID GetSSDTServiceAddress(IN wchar_t* FuncName)  //这里要传Zw函数，然后后续得Nt
{
	UNICODE_STRING Name = { 0 };
	RtlInitUnicodeString(&Name, FuncName);
	ULONG64 Func = (ULONG64)MmGetSystemRoutineAddress(&Name);
	PVOID FuncAddr;

	ULONG Index = *(PULONG)((ULONG_PTR)Func + 21);  //得到索引
	PSYSTEM_SERVICE_DESCRIPTOR_TABLE SSDT = GetSSDTAddre();
	ULONG FakeOffset;
	ULONG Offset;
	if (SSDT)
	{
		FakeOffset = *((PULONG)SSDT->ServiceTableBase + Index);
		Offset = FakeOffset >> 4;
		FuncAddr = (PVOID)((ULONG_PTR)SSDT->ServiceTableBase + Offset);
		return FuncAddr;
	}
	DbgPrint("[wdk]:No find SSDT Function [%d]", Index);
	return 0;
}

