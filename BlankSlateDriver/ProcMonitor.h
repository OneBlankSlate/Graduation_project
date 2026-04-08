#pragma once
#include<fltKernel.h>
#include"IoControlHelper.h"

typedef struct COMMUNICATE_PROCESS_MON
{
	OPERATE_TYPE OperateType;
}COMMUNICATE_PROCESS_MON, * PCOMMUNICATE_PROCESS_MON;
// 进程事件类型
typedef enum _PROCESS_EVENT_TYPE {
    ProcessCreate = 1,
    ProcessExit = 2
} PROCESS_EVENT_TYPE;

// 进程事件结构
#pragma pack(push, 1)
typedef struct _PROCESS_EVENT {
    ULONG EventId;              // 事件ID
    PROCESS_EVENT_TYPE Type;    // 事件类型
    ULONG ProcessId;            // 进程ID
    ULONG ParentProcessId;      // 父进程ID
    ULONG64 CreateTime;         // 创建时间(FILETIME格式)
    ULONG64 ExitTime;           // 退出时间(FILETIME格式)
    ULONG ExitStatus;           // 退出状态
    WCHAR ImageName[256];       // 映像名称
    WCHAR ImagePath[520];       // 映像路径
    WCHAR CommandLine[1024];    // 命令行
    WCHAR UserName[128];        // 用户名
} PROCESS_EVENT, * PPROCESS_EVENT;
#pragma pack(pop)

// 事件缓冲包
typedef struct _EVENT_PACKET {
    ULONG EventCount;           // 事件数量
    ULONG BufferSize;           // 缓冲区大小
    PROCESS_EVENT Events[1];    // 事件数组
} EVENT_PACKET, * PEVENT_PACKET;

// 驱动状态
typedef struct _DRIVER_STATUS {
    BOOLEAN IsMonitoring;       // 是否正在监控
    ULONG TotalEvents;          // 总事件数
    ULONG EventsInBuffer;       // 缓冲区中事件数
    ULONG64 StartTime;          // 监控开始时间
    ULONG BufferCapacity;       // 缓冲区容量
} DRIVER_STATUS, * PDRIVER_STATUS;

// 常量定义
#define CONTEXT_TAG 'nmoc'
#define MAX_EVENTS 1024

// 环形缓冲区结构
typedef struct _EVENT_BUFFER {
    PROCESS_EVENT Events[MAX_EVENTS];
    KSPIN_LOCK BufferLock;
    ULONG ReadIndex;
    ULONG WriteIndex;
    ULONG EventCount;
    ULONG TotalEvents;
} EVENT_BUFFER, * PEVENT_BUFFER;

// 监控上下文
typedef struct _MONITOR_CONTEXT {
    PDEVICE_OBJECT DeviceObject;
    EVENT_BUFFER EventBuffer;
    BOOLEAN IsMonitoring;
    PVOID NotifyHandle;
    LARGE_INTEGER StartTime;
} MONITOR_CONTEXT, * PMONITOR_CONTEXT;

extern PMONITOR_CONTEXT g_context;


NTSTATUS MonitorProcess();
NTSTATUS StopMonitorProcess();
NTSTATUS GetProcEvents(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);
VOID ProcessNotifyCallback(
    _Inout_ PEPROCESS Process,
    _In_ HANDLE ProcessId,
    _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo
);
NTSTATUS GetProcessInfo(
    _In_ HANDLE ProcessId,
    _In_ PEPROCESS Process,
    _Inout_ PPROCESS_EVENT Event,
    _In_ BOOLEAN Create
);
VOID AddEventToBuffer(PEVENT_BUFFER Buffer, PPROCESS_EVENT Event);
ULONG ReadEventsFromBuffer(PEVENT_BUFFER Buffer, PPROCESS_EVENT OutputBuffer, ULONG MaxEvents);
NTKERNELAPI
LPSTR
NTAPI
PsGetProcessImageFileName(PEPROCESS Process);