#pragma once
#include<fltKernel.h>
#include"IoControlHelper.h"

#ifdef _WIN64
#define _HANDLE_TABLE_ 0x200
#define _OFFSET_ 0x10
#define _OBJECT_BODY_ 0x30
#else
#define _HANDLE_TABLE_ 0x0f4
#define _OFFSET_ 0x8
#define _OBJECT_BODY_ 0x18
#endif

//进程句柄
typedef struct _HANDLE_INFORMATION_ENTRY_
{
	WCHAR HandleType[0x20];
	WCHAR HandleName[MAX_PATH];
	HANDLE Handle;
	PVOID Object;
	UCHAR Index;   //句柄类型的代号、索引
	ULONG64 Count;   //句柄的引用计数	
}HANDLE_INFORMATION_ENTRY, * PHANDLE_INFORMATION_ENTRY;
typedef struct _HANDLES_INFORMATION_
{
    ULONG NumberOfHandle;
    HANDLE_INFORMATION_ENTRY HandleInfo[1];
}HANDLES_INFORMATION, * PHANDLES_INFORMATION;

typedef struct _COMMUNICATE_PROCESS_HANDLE_
{
    OPERATE_TYPE OperateType;
    HANDLE ProcessIdentity;
}COMMUNICATE_PROCESS_HANDLE, * PCOMMUNICATE_PROCESS_HANDLE;

typedef struct _HANDLE_TABLE
{
    ULONG_PTR TableCode;                  //指向句柄表的存储结构
    PVOID QuotaProcess;               //句柄表的内存资源记录在此进程中
    PVOID UniqueProcessId;                //创建进程的ID，用于回调函数
    ULONG_PTR HandleLock;      //HANDLE_TABLE_LOCKS=4，句柄表锁，仅在句柄表扩展时使用
    LIST_ENTRY HandleTableList;           //所有的句柄表形成一个链表，链表头为全局变量HandleTableListHead
    ULONG_PTR HandleContentionEvent;   //若在访问句柄时发生竞争，则在此推锁上等待
    PVOID DebugInfo;   //调试信息，仅在调试句柄时有意义
    LONG ExtraInfoPages;                  //审计信息所占用的页面数量
    union
    {
        ULONG Flags;                      //标志域
        UCHAR StrictFIFO : 1;               //是否使用FIFO风格的重用，即先释放先重用
    };
    ULONG FirstFreeHandle;                      //空闲链表表头的句柄索引
    struct _HANDLE_TABLE_ENTRY* LastFreeHandleEntry;
    ULONG HandleCount;
    ULONG NextHandleNeedingPool;          //下一次句柄表扩展的起始句柄索引
    ULONG HandleCountHighWatermark;                     //正在使用的句柄表项的数量
}HANDLE_TABLE, * PHANDLE_TABLE;
typedef struct _HANDLE_TABLE_ENTRY
{
    union
    {
        PVOID Object;                         //指向句柄所代表的对象，二进制的后三位清零可以dt到_OBJECT_HEADER
        ULONG_PTR ObAttributes;               //最低三位有特别含义，参加OBJ_HANDLE_ATTRIBUTES宏定义
        PVOID InfoTable;   //PHANDLE_TABLE_ENTRY_INFO 各个句柄表页面的第一个表项，使用此成员指向一张表
        ULONG_PTR Value;
    };
    union
    {
        ULONG GrantedAccess;                  //访问掩码
        struct
        {                                     //当NtGlobalFlag中包含FLG_KERNEL_STACK_DB标记时使用
            USHORT GrantedAccessIndex;
            USHORT CreatorBackTraceIndex;
        };
        ULONG NextFreeTableEntry;              //空闲时表示下一个空闲句柄索引
    };
} HANDLE_TABLE_ENTRY, * PHANDLE_TABLE_ENTRY;


NTSTATUS PsEnumProcessHandles(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);
NTSTATUS EnumProcessHandlesByHandleTable(HANDLE ProcessIdentity, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS HandleTable0(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS HandleTable1(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS HandleTable2(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS HandleTable3(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS InsertHandleToList(PEPROCESS EProcess, HANDLE HandleValue, ULONG_PTR ObjectHeader, PHANDLES_INFORMATION HandlesInfo);

