#pragma once
#include<fltKernel.h>
#include"IoControlHelper.h"

#ifdef _WIN64
#define _HANDLE_TABLE_ 0x570
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

// 句柄表空闲列表结构
typedef struct _HANDLE_TABLE_FREE_LIST
{
    SINGLE_LIST_ENTRY FreeListHead;       // +0x000: 空闲列表头
    ULONG_PTR Reserved;                   // +0x008: 保留字段（用于对齐）
} HANDLE_TABLE_FREE_LIST, * PHANDLE_TABLE_FREE_LIST;

typedef struct _HANDLE_TABLE
{
    // 句柄表基本信息
    ULONG NextHandleNeedingPool;          // +0x000: 下一个需要分配池的句柄索引
    LONG ExtraInfoPages;                  // +0x004: 额外信息页数
    ULONG_PTR TableCode;                  // +0x008: 句柄表代码（包含层级信息）

    // 进程关联信息
    PEPROCESS QuotaProcess;               // +0x010: 配额进程（拥有此句柄表的进程）
    LIST_ENTRY HandleTableList;           // +0x018: 句柄表链表（连接其他进程句柄表）

    // 进程标识
    ULONG UniqueProcessId;                // +0x028: 进程ID
    union
    {
        ULONG Flags;                      // +0x02c: 标志位
        struct
        {
            ULONG StrictFIFO : 1;         // 位0: 严格FIFO顺序
            ULONG EnableHandleExceptions : 1; // 位1: 启用句柄异常
            ULONG Rundown : 1;            // 位2: 运行保护（防止访问已释放句柄表）
            ULONG Duplicated : 1;         // 位3: 是否重复
            ULONG RaiseUMExceptionOnInvalidHandleClose : 1; // 位4: 无效句柄关闭时引发用户模式异常
            ULONG Reserved : 27;          // 位5-31: 保留位
        };
    };

    // 同步对象
    EX_PUSH_LOCK HandleContentionEvent;   // +0x030: 句柄争用事件
    EX_PUSH_LOCK HandleTableLock;         // +0x038: 句柄表锁

    // 空闲列表管理
    union
    {
        HANDLE_TABLE_FREE_LIST FreeLists[1]; // +0x040: 空闲句柄列表
        UCHAR ActualEntry[32];            // +0x040: 实际条目（32字节）
    };

    // 调试信息
    PVOID DebugInfo;   // +0x060: 调试信息指针

} HANDLE_TABLE, * PHANDLE_TABLE;
typedef struct _HANDLE_TABLE_ENTRY
{
    union
    {
        volatile LONG64 VolatileLowValue;
        LONG64 LowValue;
        struct
        {
            // 第一个8字节的各种位域组合
            ULONG_PTR Unlocked : 1;        // 位0: 是否未锁定
            ULONG_PTR RefCnt : 16;         // 位1-16: 引用计数
            ULONG_PTR Attributes : 3;      // 位17-19: 属性标志
            ULONG_PTR ObjectPointerBits : 44; // 位20-63: 对象指针位
        };
        struct
        {
            VOID* Object;
            union
            {
                ULONG ObAttributes;
                ULONG_PTR InfoTable;
                ULONG_PTR Value;
            };
        };
    };

    union
    {
        LONG64 HighValue;
        struct
        {
            // 第二个8字节的各种位域组合
            ULONG GrantedAccessBits : 25;  // 位0-24: 访问权限位
            ULONG NoRightsUpgrade : 1;     // 位25: 是否禁止权限升级
            ULONG Spare1 : 6;              // 位26-31: 保留位1
            ULONG Spare2;                  // 位32-63: 保留位2(实际是4字节)
        };
        struct
        {
            USHORT GrantedAccessIndex;
            USHORT CreatorBackTraceIndex;
            LONG NextFreeTableEntry;
        };
    };
} HANDLE_TABLE_ENTRY, * PHANDLE_TABLE_ENTRY;


NTSTATUS PsEnumProcessHandles(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);
NTSTATUS EnumProcessHandlesByHandleTable(HANDLE ProcessIdentity, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS HandleTable0(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS HandleTable1(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS HandleTable2(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS HandleTable3(ULONG_PTR TableCode, PEPROCESS EProcess, PHANDLES_INFORMATION HandlesInfo, ULONG NumberOfHandle);
NTSTATUS InsertHandleToList(PEPROCESS EProcess, HANDLE HandleValue, ULONG_PTR ObjectHeader, PHANDLES_INFORMATION HandlesInfo);

