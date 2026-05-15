// ModuleMonitor.h
#pragma once

#include <fltKernel.h>
#include "IoControlHelper.h"
typedef enum _MODULE_EVENT_TYPE {
    ImageLoad = 1,
    ImageUnload = 2
} MODULE_EVENT_TYPE;

#pragma pack(push, 1)
typedef struct _MODULE_EVENT {
    MODULE_EVENT_TYPE Type;
    ULONG ProcessId;
    ULONG64 LoadTime;
    ULONG64 ImageBase;
    ULONG ImageSize;
    WCHAR ImagePath[520];
    WCHAR ImageName[200];
} MODULE_EVENT, *PMODULE_EVENT;
#pragma pack(pop)
typedef struct _COMMUNICATE_MODULE_MON {
    OPERATE_TYPE OperateType;
} COMMUNICATE_MODULE_MON, *PCOMMUNICATE_MODULE_MON;

#define MODULE_MAX_EVENTS 1024
#define MODULE_CONTEXT_TAG 'mmoc'

typedef struct _MODULE_EVENT_BUFFER {
    MODULE_EVENT Events[MODULE_MAX_EVENTS];
    KSPIN_LOCK BufferLock;
    ULONG ReadIndex;
    ULONG WriteIndex;
    ULONG EventCount;
    ULONG TotalEvents;
} MODULE_EVENT_BUFFER, *PMODULE_EVENT_BUFFER;

typedef struct _MODULE_MONITOR_CONTEXT {
    PDEVICE_OBJECT DeviceObject;
    MODULE_EVENT_BUFFER EventBuffer;
    BOOLEAN IsMonitoring;
    PVOID NotifyHandle;
    LARGE_INTEGER StartTime;
} MODULE_MONITOR_CONTEXT, *PMODULE_MONITOR_CONTEXT;

typedef struct _MODULE_EVENT_PACKET {
    ULONG EventCount;
    ULONG BufferSize;
    MODULE_EVENT Events[1];
} MODULE_EVENT_PACKET, *PMODULE_EVENT_PACKET;

typedef struct _MODULE_DRIVER_STATUS {
    BOOLEAN IsMonitoring;
    ULONG TotalEvents;
    ULONG EventsInBuffer;
    ULONG64 StartTime;
    ULONG BufferCapacity;
} MODULE_DRIVER_STATUS, *PMODULE_DRIVER_STATUS;
extern PMODULE_MONITOR_CONTEXT g_ModuleContext;

NTSTATUS MonitorModule();
NTSTATUS StopMonitorModule();
NTSTATUS GetModuleEvents(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);

VOID LoadImageNotifyCallback(
    _In_opt_ PUNICODE_STRING FullImageName,
    _In_ HANDLE ProcessId,
    _In_ PIMAGE_INFO ImageInfo
);

NTSTATUS GetModuleInfo(
    _In_ HANDLE ProcessId,
    _In_opt_ PUNICODE_STRING FullImageName,
    _In_ PIMAGE_INFO ImageInfo,
    _Inout_ PMODULE_EVENT Event
);

VOID AddModuleEventToBuffer(PMODULE_EVENT_BUFFER Buffer, PMODULE_EVENT Event);
ULONG ReadModuleEventsFromBuffer(PMODULE_EVENT_BUFFER Buffer, PMODULE_EVENT OutputBuffer, ULONG MaxEvents);
