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
//关闭句柄
typedef struct COMMUNICATE_CLOSE_HANDLE
{
    OPERATE_TYPE OperateType;
    HANDLE ProcessId;
    HANDLE TargetHandle;
}COMMUNICATE_CLOSE_HANDLE, * PCOMMUNICATE_CLOSE_HANDLE;
typedef enum _SYSTEM_INFORMATION_CLASS {
    SystemBasicInformation = 0,
    SystemProcessInformation = 5,
    // ... 其他成员省略
    SystemHandleInformation = 16,  // <--- 你要用的值
    SystemObjectInformation = 17,
    // ... 等等
} SYSTEM_INFORMATION_CLASS;
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

NTSTATUS PsEnumProcessHandles(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);
NTSTATUS EnumProcessHandlesByHandleTable(HANDLE ProcessIdentity, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS EnumProcessHandlesByService(HANDLE ProcessIdentity, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS HandleTable0(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS HandleTable1(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS HandleTable2(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS HandleTable3(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS InsertHandleToList(PEPROCESS EProcess, HANDLE HandleValue, ULONG_PTR ObjectHeader, PHANDLES_INFORMATION HandlesInfo);
NTSTATUS PsCloseHandle(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);