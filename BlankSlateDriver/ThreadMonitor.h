// ThreadMonitor.h - 内核模块头文件
#pragma once

#include <fltKernel.h>
#include "IoControlHelper.h"
// 线程事件类型
typedef enum _THREAD_EVENT_TYPE {
    ThreadCreate = 1,
    ThreadExit = 2
} THREAD_EVENT_TYPE;

// 线程事件结构
#pragma pack(push, 1)
typedef struct _THREAD_EVENT {
    THREAD_EVENT_TYPE Type;     // 事件类型
    ULONG ThreadId;             // 线程ID
    ULONG ProcessId;            // 所属进程ID
    ULONG64 CreateTime;         // 创建时间(FILETIME格式)
    ULONG64 ExitTime;           // 退出时间(FILETIME格式)
    ULONG ExitStatus;           // 退出状态
    WCHAR ImagePath[520];       // 进程映像路径
    WCHAR ModuleName[200];      // 模块名（线程起始地址所在模块）
    ULONG Priority;             // 线程优先级
    ULONG BasePriority;         // 基本优先级
} THREAD_EVENT, * PTHREAD_EVENT;
#pragma pack(pop)
// 线程监控通信结构
typedef struct _COMMUNICATE_THREAD_MON {
    OPERATE_TYPE OperateType;
} COMMUNICATE_THREAD_MON, * PCOMMUNICATE_THREAD_MON;

// 缓冲区容量
#define THREAD_MAX_EVENTS 1024
#define THREAD_CONTEXT_TAG 'tmoc'

// 事件缓冲区结构
typedef struct _THREAD_EVENT_BUFFER {
    THREAD_EVENT Events[THREAD_MAX_EVENTS];
    KSPIN_LOCK BufferLock;
    ULONG ReadIndex;
    ULONG WriteIndex;
    ULONG EventCount;
    ULONG TotalEvents;
} THREAD_EVENT_BUFFER, * PTHREAD_EVENT_BUFFER;

// 监控上下文
typedef struct _THREAD_MONITOR_CONTEXT {
    PDEVICE_OBJECT DeviceObject;
    THREAD_EVENT_BUFFER EventBuffer;
    BOOLEAN IsMonitoring;
    PVOID NotifyHandle;
    LARGE_INTEGER StartTime;
} THREAD_MONITOR_CONTEXT, * PTHREAD_MONITOR_CONTEXT;

// 事件包（用于批量传输）
typedef struct _THREAD_EVENT_PACKET {
    ULONG EventCount;           // 事件数量
    ULONG BufferSize;           // 缓冲区大小
    THREAD_EVENT Events[1];     // 事件数组
} THREAD_EVENT_PACKET, * PTHREAD_EVENT_PACKET;

// 驱动状态
typedef struct _THREAD_DRIVER_STATUS {
    BOOLEAN IsMonitoring;       // 是否在监控中
    ULONG TotalEvents;          // 总事件数
    ULONG EventsInBuffer;       // 缓冲区中事件数
    ULONG64 StartTime;          // 监控开始时间
    ULONG BufferCapacity;       // 缓冲区容量
} THREAD_DRIVER_STATUS, * PTHREAD_DRIVER_STATUS;
// 全局上下文指针
extern PTHREAD_MONITOR_CONTEXT g_ThreadContext;

// 函数声明
NTSTATUS MonitorThread();
NTSTATUS StopMonitorThread();
NTSTATUS GetThreadEvents(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);

VOID ThreadNotifyCallback(
    _In_ HANDLE ProcessId,
    _In_ HANDLE ThreadId,
    _In_ BOOLEAN Create
);

NTSTATUS GetThreadInfo(
    _In_ HANDLE ThreadId,
    _In_ HANDLE ProcessId,
    _In_ BOOLEAN Create,
    _Inout_ PTHREAD_EVENT Event
);

VOID AddThreadEventToBuffer(PTHREAD_EVENT_BUFFER Buffer, PTHREAD_EVENT Event);
ULONG ReadThreadEventsFromBuffer(PTHREAD_EVENT_BUFFER Buffer, PTHREAD_EVENT OutputBuffer, ULONG MaxEvents);