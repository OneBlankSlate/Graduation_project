#include"SystemModule.h"
#include"SystemHelper.h"
#include"MemoryHelper.h"
#include"ProcessHelper.h"
PDRIVER_OBJECT __DriverObject = NULL;
void GetDriverObject(PDRIVER_OBJECT DriverObject)
{
	__DriverObject = DriverObject;
}
PVOID GetKernelModuleHandle(PCHAR ModuleName,ULONG* SizeOfImage)
{
	PVOID VirtualAddress = NULL;
	ULONG ViewSize = 0;
	PSYSTEM_MODULE_INFORMATION SystemModuleInfo;
	ULONG i;
	if (STATUS_INFO_LENGTH_MISMATCH == ZwQuerySystemInformation(SystemModuleInformation, 0, 0, &ViewSize))
	{
		VirtualAddress = AllocatePoolWithTag(NonPagedPool, ViewSize);
		if (VirtualAddress)
		{
			ZwQuerySystemInformation(SystemModuleInformation, VirtualAddress, ViewSize, 0);
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		return NULL;
	}
	SystemModuleInfo = (PSYSTEM_MODULE_INFORMATION)VirtualAddress;

	for (i = 0; i < SystemModuleInfo->Count; i++)
	{
		_strlwr(ModuleName);
		_strlwr(SystemModuleInfo->Module[i].ModuleName);
		if (strstr(SystemModuleInfo->Module[i].ModuleName, ModuleName))
		{
			PVOID v5;
			v5 = SystemModuleInfo->Module[i].ModuleBaseAddress;
			if (SizeOfImage != NULL)
			{
				*SizeOfImage = SystemModuleInfo->Module[i].ModuleSize;
			}
			FreePoolWithTag(VirtualAddress);
			return v5;
		}
	}
	FreePoolWithTag(VirtualAddress);
	return NULL;

}

NTSTATUS EnumDriverModule(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
	PDRIVER_MODULES DriverModules = (PDRIVER_MODULES)OutputBuffer;
	PLDR_DATA_TABLE_ENTRY pLdrTblEntry = (PLDR_DATA_TABLE_ENTRY)__DriverObject->DriverSection;
	PLIST_ENTRY pListHdr = &pLdrTblEntry->InLoadOrderLinks;
	PLIST_ENTRY pListNULL = pListHdr->Flink;   //NULL  这是头结点，没东西，但占位置
	PLIST_ENTRY pListStart = pListNULL->Flink;  //ntoskrnl.exe  系统第一模块  从这里开始枚举
	PLIST_ENTRY pListEntry = pListStart;
	while (pListEntry->Flink != pListStart)
	{
		DRIVER_MODULE_ENTRY v1 = { 0 };
		pLdrTblEntry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

		RtlCopyMemory(v1.DriverName, pLdrTblEntry->ModuleName.Buffer, pLdrTblEntry->ModuleName.Length);
		v1.ModuleBase = (ULONG_PTR)pLdrTblEntry->ModuleBaseAddress;
		v1.SizeOfImage = pLdrTblEntry->ModuleSize;
		v1.EntryPoint = (ULONG_PTR)pLdrTblEntry->EntryPoint;  //模块入口EntryPoint  原本是驱动对象的，没想到好办法获取
		RtlCopyMemory(v1.DriverPath, pLdrTblEntry->FullModuleName.Buffer, pLdrTblEntry->FullModuleName.Length);

		DriverModules->ModuleEntry[DriverModules->NumberOfModules++] = v1;

		pListEntry = pListEntry->Flink;
	}	
}
