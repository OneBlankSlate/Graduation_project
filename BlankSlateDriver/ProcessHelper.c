#include"ProcessHelper.h"
#include"MemoryHelper.h"
#include"IoControlHelper.h"
#include"ObjectHelper.h"
#include"ProcessPath.h"
#include"SystemHelper.h"
#include"ProcessThread.h"
#include"CallbackHelper.h"
#ifdef _WIN64

BOOLEAN CheckNtdll32(ULONG VirtualAddress, PULONG DllBase)
{
    PPEB_LDR_DATA32 PebLdrData32;
    PLDR_DATA_TABLE_ENTRY32 LdrDataTableEntry32;
    PebLdrData32 = (PPEB_LDR_DATA32)((PPEB32)PsGetCurrentProcessWow64Process())->Ldr;
    if (!PebLdrData32)
        return FALSE;
    LdrDataTableEntry32 = (PLDR_DATA_TABLE_ENTRY32)PebLdrData32->InLoadOrderModuleList.Flink;
    LdrDataTableEntry32 = (PLDR_DATA_TABLE_ENTRY32)LdrDataTableEntry32->InLoadOrderLinks.Flink;
    if (((ULONG_PTR)VirtualAddress >= (ULONG_PTR)LdrDataTableEntry32->ModuleBaseAddress) && ((ULONG_PTR)VirtualAddress < ((ULONG_PTR)LdrDataTableEntry32->ModuleBaseAddress + LdrDataTableEntry32->ModuleSize)))
    {
        *DllBase = LdrDataTableEntry32->ModuleBaseAddress;
        return TRUE;
    }
    return FALSE;
}

BOOLEAN CheckNtdll64(ULONG VirtualAddress, PULONG_PTR DllBase)
{
    PPEB_LDR_DATA PebLdrData;
    PLDR_DATA_TABLE_ENTRY LdrDataTableEntry;
    PebLdrData = ((PPEB_LDR_DATA)PsGetProcessPeb(PsGetCurrentProcess())->Ldr);
    if (!PebLdrData)
        return FALSE;
    LdrDataTableEntry = (PLDR_DATA_TABLE_ENTRY)PebLdrData->InLoadOrderModuleList.Flink;
    LdrDataTableEntry = (PLDR_DATA_TABLE_ENTRY)LdrDataTableEntry->InLoadOrderLinks.Flink;
    if (((ULONG_PTR)VirtualAddress >= (ULONG_PTR)LdrDataTableEntry->ModuleBaseAddress) && ((ULONG_PTR)VirtualAddress < ((ULONG_PTR)LdrDataTableEntry->ModuleBaseAddress + LdrDataTableEntry->ModuleSize)))
    {
        *DllBase = (ULONG_PTR)LdrDataTableEntry->ModuleBaseAddress;
        return TRUE;
    }
    return FALSE;
}

#else
#endif
BOOLEAN PsIsProcessTermination(PEPROCESS EProcess)
{
    LARGE_INTEGER v1 = { 0 };
    return KeWaitForSingleObject(EProcess, Executive, KernelMode, FALSE, &v1) == STATUS_WAIT_0;
}
BOOLEAN PsIsRealProcess(PEPROCESS EProcess)
{
    //查看EProcess是否具有进程对象的特征
    ULONG_PTR ObjectType;
    ULONG_PTR ObjectTypeAddress;
    ULONG_PTR ProcessType = ((ULONG_PTR)*PsProcessType);  //系统第一模块导出的全局变量
    //从系统的第一个模块（ntkrnlpa.exe)中的导出表中获得函数地址
    if (__ObGetObjectType == NULL)
    {
        UNICODE_STRING v1;
        RtlInitUnicodeString(&v1, L"ObGetObjectType");
        __ObGetObjectType = (LPFN_OBGETOBJECTTYPE)MmGetSystemRoutineAddress(&v1);
    }
    if (ProcessType && EProcess && MmIsAddressValid((PVOID)EProcess))
    {
        ObjectType = __ObGetObjectType((PVOID)EProcess);
        if (ObjectType && ProcessType == ObjectType)
        {
            return TRUE;
        }
    }
    return FALSE;
}

NTSTATUS PsEnumProcess(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PPROCESS_INFORMATIONS ProcessInfos = (PPROCESS_INFORMATIONS)OutputBuffer;
    //参数检查
    if (!InputBuffer || InputBufferLength != sizeof(OPERATE_TYPE) || !OutputBuffer || OutputBufferLength < sizeof(PROCESS_INFORMATIONS))
    {
        return STATUS_INVALID_PARAMETER;
    }
    return PsEnumProcessInternal(ProcessInfos, OutputBufferLength);
}

NTSTATUS PsEnumProcessInternal(PPROCESS_INFORMATIONS ProcessInfos, ULONG OutputBufferLength)
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    ULONG NumberOfProcess = (OutputBufferLength - sizeof(PROCESS_INFORMATIONS)) / sizeof(PROCESS_INFORMATION_ENTRY);
    if (!ProcessInfos)
    {
        return STATUS_INVALID_PARAMETER;
    }
    EnumProcessByService(ProcessInfos, NumberOfProcess);
    if (NumberOfProcess >= ProcessInfos->NumberOfProcess)
    {
        Status = STATUS_SUCCESS;
    }
    else
    {
        Status = STATUS_BUFFER_TOO_SMALL;
    }
    return Status;
}

VOID EnumProcessByService(PPROCESS_INFORMATIONS ProcessInfos, ULONG NumberOfProcess)
{
    ULONG ReturnValue = 0, i = 0;
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    PVOID v5 = NULL;
    ULONG v7 = 0x10000;
    PSYSTEM_PROCESS_INFORMATION v1 = NULL;
    do
    {
        v5 = AllocatePoolWithTag(PagedPool, v7);
        if (v5)
        {
            Status = ZwQuerySystemInformation(SystemProcessInformation, v5, v7, &ReturnValue);
            if (NT_SUCCESS(Status))
            {
                v1 = (PSYSTEM_PROCESS_INFORMATION)v5;
                while (1)
                {
                    PEPROCESS EProcess = NULL;
                    if (NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)(v1->UniqueProcessId), &EProcess)))
                    {
                        SetProcessInfoToList(ProcessInfos, NumberOfProcess, EProcess);
                        ObfDereferenceObject(EProcess);
                    }

                    if (v1->NextEntryOffset == 0)
                    {
                        break;
                    }
                    v1 = (PSYSTEM_PROCESS_INFORMATION)((ULONG_PTR)v1 + v1->NextEntryOffset);
                }

            }
            else
            {
                v7 *= 2;
            }
            FreePoolWithTag(v5);
            v5 = NULL;
        }
        else
        {
            break;
        }
    } while (Status == STATUS_INFO_LENGTH_MISMATCH && ++i < 10);
       
}

VOID SetProcessInfoToList(PPROCESS_INFORMATIONS ProcessInfos, ULONG NumberOfProcess, PEPROCESS EProcess)
{
    ULONG v1 = ProcessInfos->NumberOfProcess;
    if (NumberOfProcess > v1)
    { 
#ifdef _WIN64
        ULONG_PTR ProcessIdentity = *(PULONG_PTR)((ULONG_PTR)EProcess + 0x440);
        strcpy_s(ProcessInfos->ProcessInfo[v1].ImageName, 15, (char*)((ULONG_PTR)EProcess + 0x5a8));
        /*ULONG_PTR ProcessIdentity = *(PULONG_PTR)((ULONG_PTR)EProcess + 0x180);
        strcpy_s(ProcessInfos->ProcessInfo[v1].ImageName, 15, (char*)((ULONG_PTR)EProcess + 0x2e0));*/
#else
        ULONG_PTR ProcessIdentity = *(PULONG_PTR)((ULONG_PTR)EProcess + 0xb4);

#endif 

        if (ProcessIdentity)
        {
            ProcessInfos->ProcessInfo[v1].ProcessIdentity = ProcessIdentity;
        }
        else
        {
            ProcessInfos->ProcessInfo[v1].ProcessIdentity = PsGetProcessId(EProcess);
        }
        //ProcessInfos->ProcessInfo[v1].CreateTime = PsGetProcessCreateTimeQuadPart(EProcess);

        ProcessInfos->ProcessInfo[v1].ParentPid = (ULONG_PTR)PsGetProcessInheritedFromUniqueProcessId(EProcess);
        ProcessInfos->ProcessInfo[v1].EProcess = EProcess;
        wchar_t ProcessPath[MAX_PATH] = { 0 };
        GetProcessFullPathByPeb(EProcess, ProcessPath, MAX_PATH);
        RtlCopyMemory(ProcessInfos->ProcessInfo[v1].ProcessPath, ProcessPath, MAX_PATH);
    }
    ProcessInfos->NumberOfProcess++;
    
}

NTSTATUS PsTerminateProcess(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    PEPROCESS EProcess;
    HANDLE ProcessHandle;
    PCOMMUNICATE_HIDE_PROCESS v5 = (PCOMMUNICATE_HIDE_PROCESS)InputBuffer;  
    //参数检查
    if (!InputBuffer || InputBufferLength != sizeof(COMMUNICATE_HIDE_PROCESS))
    {
        return STATUS_INVALID_PARAMETER;
    }
    if (v5->ProcessIdentity == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }
    if (NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)(v5->ProcessIdentity), &EProcess)))
    {
        if (PsIsRealProcess(EProcess))
        {
            Status = ObOpenObjectByPointer(EProcess, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, GENERIC_ALL, *PsProcessType, KernelMode, &ProcessHandle);
            if (NT_SUCCESS(Status))
            {
                LPFN_NTTERMINATEPROCESS NtTerminateProcess = NULL;   // GetSSDTServiceAddress(L"ZwTerminateProcess");
                
                PSYSTEM_SERVICE_DESCRIPTOR_TABLE SystemServiceDescriptorTable = NULL;
                ULONG32 ServiceIndex = 0;
                SystemServiceDescriptorTable = GetKeServiceDescriptorTable2();
                if (!GetNtXXXServiceIndex("NtTerminateProcess", &ServiceIndex))
                {
                    Status = STATUS_UNSUCCESSFUL;
                }
                if (!NT_SUCCESS(GetNtXXXServiceAddress(ServiceIndex, (PVOID*)&NtTerminateProcess)))
                {
                    Status = STATUS_UNSUCCESSFUL;
                }
                if (NtTerminateProcess != NULL)
                {
                    PETHREAD EThread = PsGetCurrentThread();
                    CHAR PreviousMode = ChangePreviousMode(EThread);
                    Status = NtTerminateProcess(ProcessHandle, NULL);
                    RecoverPreviousMode(EThread, PreviousMode);
                }
                else
                {
                    Status = STATUS_UNSUCCESSFUL;
                }
            }
        }
        
    }
    return Status;
}

NTSTATUS PsHideProcess(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PEPROCESS TargetEProcess = NULL;   //要隐藏的进程
    PEPROCESS AheadEProcess = NULL;    //保存__System进程对象的前一个
    PEPROCESS v1 = NULL;         //游走对象
    char* TargetImageFileName = NULL;   //要隐藏的进程的名称
    char* ImageFileName = NULL;        //保存游走对象中的ImageFileName
    PLIST_ENTRY ListEntry;
    PCOMMUNICATE_HIDE_PROCESS v5 = (PCOMMUNICATE_HIDE_PROCESS)InputBuffer;
    //参数检查
    if (!InputBuffer || InputBufferLength != sizeof(COMMUNICATE_HIDE_PROCESS))
    {
        return STATUS_INVALID_PARAMETER;
    }
    HANDLE ProcessIdentity = v5->ProcessIdentity;
    if (ProcessIdentity == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }
    else
    {
#ifdef _WIN64
        int ImageNameOffset = 0x2e0;
        int ActiveLinkOffset = 0x188;
#else
        int ImageNameOffset = 0x16c;
        int ActiveLinkOffset = 0x0b8;
#endif
        if (NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)(v5->ProcessIdentity), &TargetEProcess)))
        {
            if (TargetEProcess != NULL)
            {
                TargetImageFileName = (char*)((UINT8*)TargetEProcess + ImageNameOffset);
                v1 = __SystemEProcess;
                ListEntry = (PLIST_ENTRY)((UINT8*)__SystemEProcess + ActiveLinkOffset);
                AheadEProcess = (PEPROCESS)(((ULONG_PTR)(ListEntry->Blink)) - ActiveLinkOffset);  //System的前一个进程对象
                ListEntry = NULL;
                while (v1 != AheadEProcess)
                {
                    ImageFileName = (char*)((UINT8*)v1 + ImageNameOffset);   //游走映像名
                    ListEntry = (PLIST_ENTRY)((ULONG_PTR)v1 + ActiveLinkOffset);
                    if (strstr(ImageFileName, TargetImageFileName) != NULL)
                    {
                        if (ListEntry != NULL)
                        {
                            RemoveEntryList(ListEntry);
                            break;
                        }
                    }
                    v1 = (PEPROCESS)(((ULONG_PTR)(ListEntry->Flink)) - ActiveLinkOffset);
                }
            }
            ObfDereferenceObject(TargetEProcess);
        }
    }
    return Status;
}
