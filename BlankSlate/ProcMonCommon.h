// ProcMonCommon.h - 驱动与应用层共享
#pragma once

#include <windows.h>

#define CTL_CODE(DeviceType, Function, Method, Access) \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method)

#define IOCTL_PROCMON_START_MONITOR \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_PROCMON_STOP_MONITOR \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_PROCMON_GET_EVENTS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_PROCMON_GET_STATUS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

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
    WCHAR ParentProcessName[200];//父进程名
    WCHAR ImageName[200];       // 映像名称
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