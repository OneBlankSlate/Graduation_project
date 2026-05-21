#pragma once
#include<fltKernel.h>
#include"IoControlHelper.h"
#include"SystemHelper.h"
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

typedef struct _HANDLE_TABLE_FREE_LIST
{
    //
    // 空闲句柄数量
    //
    EX_PUSH_LOCK FreeListLock;

    //
    // 第一个空闲 HANDLE_TABLE_ENTRY
    //
    union
    {
        HANDLE_TABLE_ENTRY FirstFreeHandleEntry;

        struct
        {
            ULONG FirstFreeHandle;
            ULONG LastFreeHandleEntry;
        };
    };

    //
    // 空闲句柄计数
    //
    ULONG HandleCount;

    //
    // 高水位统计
    //
    ULONG HighWaterMark;

} HANDLE_TABLE_FREE_LIST, * PHANDLE_TABLE_FREE_LIST;
typedef struct _HANDLE_TABLE
{
    //
    // 下一个需要扩展池的位置
    //
    ULONG NextHandleNeedingPool;              // 0x000

    //
    // 扩展信息页数量
    //
    LONG ExtraInfoPages;                      // 0x004

    //
    // 三级句柄表编码地址
    //
    ULONG64 TableCode;                        // 0x008

    //
    // 配额所属进程
    //
    struct _EPROCESS* QuotaProcess;           // 0x010

    //
    // 全局 HandleTable 链表
    //
    LIST_ENTRY HandleTableList;               // 0x018

    //
    // PID
    //
    ULONG UniqueProcessId;                    // 0x028

    union
    {
        ULONG Flags;                          // 0x02C

        struct
        {
            ULONG StrictFIFO : 1;
            ULONG EnableHandleExceptions : 1;
            ULONG Rundown : 1;
            ULONG Duplicated : 1;
            ULONG RaiseUMExceptionOnInvalidHandleClose : 1;
            ULONG Reserved : 27;
        };
    };

    //
    // 句柄竞争锁
    //
    EX_PUSH_LOCK HandleContentionEvent;       // 0x030

    //
    // 句柄表锁
    //
    EX_PUSH_LOCK HandleTableLock;             // 0x038

    //
    // 空闲链表
    //
    HANDLE_TABLE_FREE_LIST FreeLists[1];      // 0x040

    //
    // 调试信息
    //
    PVOID DebugInfo;                          // 0x060

} HANDLE_TABLE, * PHANDLE_TABLE;

//关闭句柄
typedef struct COMMUNICATE_CLOSE_HANDLE
{
    OPERATE_TYPE OperateType;
    HANDLE ProcessId;
    HANDLE TargetHandle;
}COMMUNICATE_CLOSE_HANDLE, * PCOMMUNICATE_CLOSE_HANDLE;

// 单个句柄条目的详细信息
typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO {
    USHORT UniqueProcessId;     // 所属进程 PID
    USHORT CreatorBackTraceIndex; // 回溯索引 (通常无用)
    UCHAR ObjectTypeIndex;      // 内核对象类型索引 (如文件、进程、线程等)
    UCHAR HandleAttributes;     // 句柄属性 (如 OBJ_INHERIT)
    USHORT HandleValue;         // <--- 重点：句柄的数值 (如你之前看到的 0xC4)
    PVOID Object;               // 内核对象体的地址 (EPROCESS / EOBJECT 等)
    ULONG GrantedAccess;        // 该句柄拥有的访问权限 (ACCESS_MASK)
} SYSTEM_HANDLE_TABLE_ENTRY_INFO, * PSYSTEM_HANDLE_TABLE_ENTRY_INFO;

// 存放所有系统句柄的缓冲区头部
typedef struct _SYSTEM_HANDLE_INFORMATION {
    ULONG NumberOfHandles;      // 当前缓冲区中实际包含的句柄总数
    SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[1]; // 柔性数组，实际长度由 NumberOfHandles 决定
} SYSTEM_HANDLE_INFORMATION, * PSYSTEM_HANDLE_INFORMATION;
// 声明函数原型
typedef NTSTATUS(*PFN_ZW_QUERY_SYSTEM_INFORMATION)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
    );


//
// ExMapHandleToPointer
//
typedef PVOID(*PEX_MAP_HANDLE_TO_POINTER)(
    PHANDLE_TABLE HandleTable,
    HANDLE Handle
    );

extern PEX_MAP_HANDLE_TO_POINTER g_ExMapHandleToPointer;


NTSTATUS PsEnumProcessHandles(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);
NTSTATUS EnumProcessHandlesByHandleTable(HANDLE ProcessIdentity, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS EnumProcessHandlesByService(HANDLE ProcessIdentity, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS HandleTable0(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS HandleTable1(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS HandleTable2(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS HandleTable3(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS InsertHandleToList(PEPROCESS EProcess, HANDLE HandleValue, ULONG_PTR ObjectHeader, PHANDLES_INFORMATION HandlesInfo);
NTSTATUS PsCloseHandle(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);