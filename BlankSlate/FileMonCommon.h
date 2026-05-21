// FileMonCommon.h - 文件监控共用定义
#pragma once

#include <windows.h>

// 复用 METHOD_NEITHER 方式的 MY_CTL_CODE
// 事件类型枚举
typedef enum _FILE_EVENT_TYPE {
    FileCreateOrOpen = 1,
    FileRead = 2,
    FileWrite = 3,
    FileDelete = 4,
    FileRename = 5,
    FileSetInfo = 6
} FILE_EVENT_TYPE;

// 文件事件结构
#pragma pack(push, 1)
typedef struct _FILE_EVENT {
    ULONG EventId;              // 事件ID
    FILE_EVENT_TYPE Type;        // 事件类型
    ULONG ProcessId;            // 进程ID
    ULONGLONG TimeStamp;        // 事件时间戳 (转换为自1601年以来的100纳秒间隔数，与 FILETIME 兼容)
    WCHAR ProcessName[256];     // 进程名
    WCHAR FilePath[520];         // 文件路径
    WCHAR ExtraInfo[256];       // 额外信息
} FILE_EVENT, * PFILE_EVENT;
#pragma pack(pop)

// 事件包
typedef struct _FILE_EVENT_PACKET {
    ULONG EventCount;           // 本次返回的事件数量
    ULONG BufferSize;           // 缓冲区总大小
    FILE_EVENT Events[1];       // 事件数组
} FILE_EVENT_PACKET, * PFILE_EVENT_PACKET;

// 缓冲区大小定义
#define MAX_FILE_EVENTS 1024
#define FILE_EVENT_PACKET_SIZE (sizeof(FILE_EVENT_PACKET) + (MAX_FILE_EVENTS - 1) * sizeof(FILE_EVENT))

// 文件保护标志
#define FILE_PROTECT_DELETE  0x01
#define FILE_PROTECT_MODIFY  0x02
#define FILE_PROTECT_COPY    0x04