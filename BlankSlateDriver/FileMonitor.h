// FileMonitor.h
#pragma once

// 包含项目头文件
#include "FileMonCommon.h" // 包含文件事件结构和事件缓冲区结构
#include "IoControlHelper.h"
// 文件监控上下文结构
typedef struct _FILE_MONITOR_CONTEXT {
    PDEVICE_OBJECT DeviceObject;
    FILE_EVENT_BUFFER EventBuffer; // 事件缓冲区
    BOOLEAN IsMonitoring;
    PFLT_FILTER FilterHandle;     // MiniFilter 过滤器句柄
    LARGE_INTEGER StartTime;
} FILE_MONITOR_CONTEXT, * PFILE_MONITOR_CONTEXT;

// 系统进程过滤列表
typedef struct _SYSTEM_PROCESS_FILTER {
    WCHAR ProcessName[256];
    struct _SYSTEM_PROCESS_FILTER* Next;
} SYSTEM_PROCESS_FILTER, * PSYSTEM_PROCESS_FILTER;

// 全局变量声明
extern PFILE_MONITOR_CONTEXT g_FileMonitorContext;
extern PSYSTEM_PROCESS_FILTER g_SystemProcessFilterList;

// MiniFilter 相关声明
extern CONST FLT_OPERATION_REGISTRATION FilterCallbacks[];
extern CONST FLT_REGISTRATION FilterRegistration;
extern PFLT_FILTER gFilterHandle;

// 文件监控处理函数声明
NTSTATUS StartFileMonitor(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, PULONG ReturnValue);
NTSTATUS StopFileMonitor(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, PULONG ReturnValue);
NTSTATUS GetFileEvents(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, PULONG ReturnValue);

// 缓冲区管理函数
VOID AddFileEventToBuffer(PFILE_EVENT Event);
ULONG ReadFileEventsFromBuffer(PFILE_EVENT OutputBuffer, ULONG MaxEvents);

// MiniFilter 回调函数
FLT_PREOP_CALLBACK_STATUS FilePreOperationCallback(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID* CompletionContext
);

FLT_POSTOP_CALLBACK_STATUS FilePostOperationCallback(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID CompletionContext,
    FLT_POST_OPERATION_FLAGS Flags
);

// MiniFilter 管理函数
NTSTATUS InitializeFileMonitor(PDRIVER_OBJECT DriverObject);
VOID UninitializeFileMonitor();
NTSTATUS PtUnload(FLT_FILTER_UNLOAD_FLAGS Flags);
NTSTATUS PtInstanceSetup(
    PCFLT_RELATED_OBJECTS FltObjects,
    FLT_INSTANCE_SETUP_FLAGS Flags,
    DEVICE_TYPE VolumeDeviceType,
    FLT_FILESYSTEM_TYPE VolumeFilesystemType
);

// 辅助函数
BOOLEAN IsSystemProcessByName(PWCHAR ProcessName);
VOID LogFileOperation(
    PFLT_CALLBACK_DATA Data,
    PCSTR OperationName
);
VOID InitializeSystemProcessFilter();
NTKERNELAPI
LPSTR
NTAPI
PsGetProcessImageFileName(PEPROCESS Process);