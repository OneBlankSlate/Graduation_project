#include"ProcessHandle.h"
#include"ProcessHelper.h"
#include"ObjectHelper.h"

NTSTATUS PsEnumProcessHandles(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
    NTSTATUS Status1 = STATUS_UNSUCCESSFUL,Status2 = STATUS_UNSUCCESSFUL;
    PCOMMUNICATE_PROCESS_HANDLE v1 = (PCOMMUNICATE_PROCESS_HANDLE)InputBuffer;
    PEPROCESS EProcess = NULL;
    HANDLE ProcessIdentity = 0;
    ULONG NumberOfHandle = (OutputBufferLength - sizeof(HANDLES_INFORMATION)) / sizeof(HANDLE_INFORMATION_ENTRY);
    if (!InputBuffer || InputBufferLength != sizeof(COMMUNICATE_PROCESS_HANDLE) || !OutputBuffer || OutputBufferLength < sizeof(HANDLES_INFORMATION))
    {
        return STATUS_INVALID_PARAMETER;
    }
    ProcessIdentity = v1->ProcessIdentity;
    if (ProcessIdentity)
    {
        Status2 = PsLookupProcessByProcessId(ProcessIdentity, &EProcess);
    }
    if (!EProcess)
    {
        return STATUS_UNSUCCESSFUL;
    }
    if (PsIsRealProcess(EProcess))
    {
        //Status1 = EnumProcessHandlesByService(ProcessIdentity,EProcess,(PHANDLES_INFORMATION)OutputBuffer,NumberOfHandle);
        Status1 = EnumProcessHandlesByHandleTable(ProcessIdentity, EProcess, (PHANDLES_INFORMATION)OutputBuffer, NumberOfHandle);
    }
    if (NT_SUCCESS(Status2))
    {
        ObfDereferenceObject(EProcess);
    }
    return Status1;
}

NTSTATUS EnumProcessHandlesByHandleTable(HANDLE ProcessIdentity, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle)
{
    PHANDLE_TABLE HandleTable = NULL;
    ULONG_PTR TableCode = 0;
    ULONG Flag = 0;
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG_PTR ObjectCount = 0;
    __try
    {
        HandleTable = (PHANDLE_TABLE)(*((ULONG_PTR*)((ULONG_PTR)EProcess + _HANDLE_TABLE_)));
        if (MmIsAddressValid(HandleTable))
        {
            TableCode = (ULONG_PTR)(HandleTable->TableCode) & 0xFFFFFFFFFFFFFFFC;
            Flag = (ULONG)(HandleTable->TableCode) & 0x03;
            switch (Flag)
            {
            case 0:
            {
                Status = HandleTable0(TableCode, EProcess, HandlesInfo, NumberOfHandle);
                break;
            }
            case 1:
            {
                Status = HandleTable1(TableCode, EProcess, HandlesInfo, NumberOfHandle);
                break;
            }
            case 2:
            {
                Status = HandleTable2(TableCode, EProcess, HandlesInfo, NumberOfHandle);
                break;
            }
            case 3:
            {
                Status = HandleTable3(TableCode, EProcess, HandlesInfo, NumberOfHandle);
                break;
            }

            }
        }
        else
        {
            Status = STATUS_UNSUCCESSFUL;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {

    }
    return Status;
}

NTSTATUS HandleTable0(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle)
{
    PHANDLE_TABLE_ENTRY HandleTableEntry = NULL;
    ULONG Index = 0;
    NTSTATUS Status;
    HandleTableEntry = (PHANDLE_TABLE_ENTRY)((ULONG_PTR*)(TableCode + _OFFSET_));
    for (Index = 1; Index <= (PAGE_SIZE / sizeof(HANDLE_TABLE_ENTRY)); Index++)
    {
        if (MmIsAddressValid((PVOID)HandleTableEntry))
        {
            PVOID ObjectHeader = (PVOID)(*(ULONG_PTR*)HandleTableEntry & 0xFFFFFFFFFFFFFFF8);
            if (MmIsAddressValid(ObjectHeader))
            {
                PVOID ObjectBody = (PVOID)((ULONG_PTR)ObjectHeader + _OBJECT_BODY_);
                if (MmIsAddressValid(ObjectBody))
                {
                    DbgPrint("ObjectBody:%p\r\n", ObjectBody);
                    if (NumberOfHandle > HandlesInfo->NumberOfHandle)
                    {
                        InsertHandleToList((PEPROCESS)EProcess, (HANDLE)((HandlesInfo->NumberOfHandle+1) * sizeof(int)), (ULONG_PTR)ObjectHeader, HandlesInfo);
                        HandlesInfo->NumberOfHandle++;
                    }
                }
            }
        }
        HandleTableEntry++;
    }
    if (NumberOfHandle >= HandlesInfo->NumberOfHandle)
    {
        Status = STATUS_SUCCESS;
    }
    else
    {
        Status = STATUS_BUFFER_TOO_SMALL;
    }
    return Status;
}

NTSTATUS HandleTable1(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle)
{
    NTSTATUS Status = STATUS_SUCCESS;
    do
    {
        Status = HandleTable0(*(ULONG_PTR*)TableCode, EProcess, HandlesInfo, NumberOfHandle);
        TableCode += sizeof(ULONG_PTR);

    } while (*(PULONG_PTR)TableCode != 0 && MmIsAddressValid((PVOID) * (PULONG_PTR)TableCode));
    return Status;
}

NTSTATUS HandleTable2(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle)
{
    NTSTATUS Status = STATUS_SUCCESS;
    do
    {
        Status = HandleTable1(*(ULONG_PTR*)TableCode, EProcess, HandlesInfo, NumberOfHandle);
        TableCode += sizeof(ULONG_PTR);

    } while (*(PULONG_PTR)TableCode != 0);
    return Status;
}

NTSTATUS HandleTable3(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle)
{
    NTSTATUS Status = STATUS_SUCCESS;
    do
    {
        Status = HandleTable2(*(ULONG_PTR*)TableCode, EProcess, HandlesInfo, NumberOfHandle);
        TableCode += sizeof(ULONG_PTR);

    } while (*(PULONG_PTR)TableCode != 0);
    return Status;
}
NTSTATUS InsertHandleToList(PEPROCESS EProcess,HANDLE HandleValue,ULONG_PTR ObjectHeader, PHANDLES_INFORMATION HandlesInfo)
{
    PVOID ObjectBody = (PVOID)(ObjectHeader + _OBJECT_BODY_);
    //句柄类型代号
    HandlesInfo->HandleInfo[HandlesInfo->NumberOfHandle].Index = *(UCHAR*)((ULONG_PTR)ObjectHeader + 0x18);
    //引用计数
    HandlesInfo->HandleInfo[HandlesInfo->NumberOfHandle].Count = *(ULONG_PTR*)((ULONG_PTR)ObjectHeader + 0);
    //句柄值
    HandlesInfo->HandleInfo[HandlesInfo->NumberOfHandle].Handle = HandleValue;
    //句柄对象
    HandlesInfo->HandleInfo[HandlesInfo->NumberOfHandle].Object = ObjectBody;
    //对象类型
    POBJECT_TYPE Type = __ObGetObjectType(ObjectBody);  //传入对象体指针，EProcess是进程对象的对象体，传入则返回的类型是Process
   /* kd > dt _object_type
        nt!_OBJECT_TYPE
        + 0x000 TypeList         : _LIST_ENTRY
        + 0x010 Name : _UNICODE_STRING*/
#ifdef _WIN64
    PUNICODE_STRING Name = (PUNICODE_STRING)((ULONG_PTR)Type + 0x10);
#else
    PUNICODE_STRING Name = (PUNICODE_STRING)((ULONG_PTR)Type + 0x08);
#endif
    
    //RtlCopyMemory(HandlesInfo->HandleInfo[HandlesInfo->NumberOfHandle].HandleType, Name->Buffer, (wcslen(Name->Buffer) + 1) * 2);
    RtlCopyMemory(HandlesInfo->HandleInfo[HandlesInfo->NumberOfHandle].HandleType, Name->Buffer, Name->Length);

    //对象名  ObQueryNameString
    POBJECT_NAME_INFORMATION NameInfo = NULL;
    ULONG RequiredLength = 0;
    // 第一次调用获取所需缓冲区大小
    NTSTATUS Status = ObQueryNameString(ObjectBody,NULL,0,&RequiredLength);
    // 分配缓冲区
    NameInfo = (POBJECT_NAME_INFORMATION)ExAllocatePool(PagedPool, RequiredLength);
    // 第二次调用获取对象名称
    Status = ObQueryNameString(ObjectBody, NameInfo,RequiredLength,&RequiredLength);
    if (NT_SUCCESS(Status)) 
    {
        RtlCopyMemory(HandlesInfo->HandleInfo[HandlesInfo->NumberOfHandle].HandleName, NameInfo->Name.Buffer, NameInfo->Name.Length);
    }
    else 
    {
        ExFreePool(NameInfo);
    }
    
    return Status;
}

