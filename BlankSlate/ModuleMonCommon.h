// ModuleMonCommon.h - 映像加载监控共用定义
#pragma once

#include <windows.h>

// 映像加载事件类型
typedef enum _MODULE_EVENT_TYPE {
    ImageLoad = 1,
    ImageUnload = 2
} MODULE_EVENT_TYPE;

// 映像加载事件结构
#pragma pack(push, 1)
typedef struct _MODULE_EVENT {
    MODULE_EVENT_TYPE Type;     // 事件类型
    ULONG ProcessId;            // 所属进程ID
    WCHAR ProcessName[200];     // 进程名（驱动层通过GetProcessFullPathByEProcess+GetNameByPath获取）
    ULONG64 LoadTime;           // 加载时间(FILETIME格式)
    ULONG64 ImageBase;          // 映像加载基地址
    ULONG ImageSize;            // 映像大小
    WCHAR ImagePath[520];       // 映像完整路径
    WCHAR ImageName[200];       // 映像文件名
} MODULE_EVENT, * PMODULE_EVENT;
#pragma pack(pop)

// 事件包（用于批量传输）
typedef struct _MODULE_EVENT_PACKET {
    ULONG EventCount;           // 事件数量
    ULONG BufferSize;           // 缓冲区大小
    MODULE_EVENT Events[1];     // 事件数组
} MODULE_EVENT_PACKET, * PMODULE_EVENT_PACKET;

// 驱动状态
typedef struct _MODULE_DRIVER_STATUS {
    BOOLEAN IsMonitoring;       // 是否在监控中
    ULONG TotalEvents;          // 总事件数
    ULONG EventsInBuffer;       // 缓冲区中事件数
    ULONG64 StartTime;          // 监控开始时间
    ULONG BufferCapacity;       // 缓冲区容量
} MODULE_DRIVER_STATUS, * PMODULE_DRIVER_STATUS;
