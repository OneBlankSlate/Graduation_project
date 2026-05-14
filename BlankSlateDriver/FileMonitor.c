#include "FileMonitor.h"
#include "ProcessHelper.h"
#include "SystemHelper.h"

// 全局变量定义
PFILE_MONITOR_CONTEXT g_FileMonitorContext = NULL;
PSYSTEM_PROCESS_FILTER g_SystemProcessFilterList = NULL;
PFLT_FILTER gFilterHandle = NULL;

// 默认系统进程过滤列表
WCHAR* g_DefaultSystemProcesses[] = {
    L"system",
    L"registry",
    L"system.exe",
    L"tiworker.exe",
    L"searchindexer.",
    L"dwm.exe",
    L"csrss.exe",
    L"winlogon.exe",
    L"services.exe",
    L"lsass.exe",
    L"smss.exe",
    L"svchost.exe",
    L"taskhostw.exe",
    L"runtimebroker.exe",
    L"shellexperiencehost.exe",
    L"searchui.exe",
    L"fontdrvhost.exe",
    L"spoolsv.exe",
    L"msmpeng.exe",
    L"nissrv.exe",
    L"wmiprvse.exe",
    L"dllhost.exe",
    L"wudfhost.exe",
    L"audiodg.exe",
    L"sihost.exe",
    L"ctfmon.exe",
    L"taskmgr.exe",
    L"onedrive.exe",
    L"vmtoolsd.exe",
    L"backgroundtask",
    L"runtimebroker.",
    L"searchapp.exe",
    L"onedrive.sync.",
    L"system",
    L"mspcmanagerser",
    L"wmiprvse.exe",
    L"msmpeng.exe",
    L"msedgewebview2",
    L"smartscreen.exe",
    L"smartscreen.ex",
    L"microsoftedgeu",
    L"filecoauth.exe",
    L"searchfilterHo",
    L"searchprotocol",
    L"hxtsr.exe",
    L"mscorsvw.exe",
    L"ngen.exe",
    L"ngentask.exe",
    NULL
};

// 获取数组元素个数的宏（替代 _countof）
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// 初始化系统进程过滤器
VOID InitializeSystemProcessFilter()
{
    for (ULONG i = 0; g_DefaultSystemProcesses[i] != NULL; i++) {
        PSYSTEM_PROCESS_FILTER filter = (PSYSTEM_PROCESS_FILTER)ExAllocatePoolWithTag(
            NonPagedPool,
            sizeof(SYSTEM_PROCESS_FILTER),
            'FMon'
        );

        if (filter) {
            // 使用 RtlCopyMemory 替代 RtlStringCchCopyW
            size_t len = 0;
            for (len = 0; len < ARRAY_SIZE(filter->ProcessName) - 1 && g_DefaultSystemProcesses[i][len] != L'\0'; len++) {
                filter->ProcessName[len] = g_DefaultSystemProcesses[i][len];
            }
            filter->ProcessName[len] = L'\0';

            filter->Next = g_SystemProcessFilterList;
            g_SystemProcessFilterList = filter;
        }
    }
}

// 检查是否为系统进程
BOOLEAN IsSystemProcessByName(PWCHAR ProcessName)
{
    if (!ProcessName) {
        return FALSE;
    }

    // 转换为小写进行比较
    WCHAR lowerProcessName[256];
    SIZE_T length = 0;

    // 安全地获取字符串长度
    for (length = 0; length < ARRAY_SIZE(lowerProcessName) - 1 && ProcessName[length] != L'\0'; length++) {
        if (ProcessName[length] >= L'A' && ProcessName[length] <= L'Z') {
            lowerProcessName[length] = ProcessName[length] + (L'a' - L'A');
        }
        else {
            lowerProcessName[length] = ProcessName[length];
        }
    }
    lowerProcessName[length] = L'\0';

    // 检查是否在系统进程列表中
    PSYSTEM_PROCESS_FILTER filter = g_SystemProcessFilterList;
    while (filter) {
        if (wcscmp(lowerProcessName, filter->ProcessName) == 0) {
            return TRUE;
        }
        filter = filter->Next;
    }

    return FALSE;
}

// 初始化文件监控模块
NTSTATUS InitializeFileMonitor(PDRIVER_OBJECT DriverObject)
{
    NTSTATUS status = STATUS_SUCCESS;

    // 分配上下文内存
    g_FileMonitorContext = (PFILE_MONITOR_CONTEXT)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(FILE_MONITOR_CONTEXT),
        'FMon'
    );

    if (!g_FileMonitorContext) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(g_FileMonitorContext, sizeof(FILE_MONITOR_CONTEXT));

    // 初始化事件缓冲区
    KeInitializeSpinLock(&g_FileMonitorContext->EventBuffer.BufferLock);
    g_FileMonitorContext->EventBuffer.ReadIndex = 0;
    g_FileMonitorContext->EventBuffer.WriteIndex = 0;
    g_FileMonitorContext->EventBuffer.EventCount = 0;
    g_FileMonitorContext->EventBuffer.TotalEvents = 0;

    g_FileMonitorContext->DeviceObject = DriverObject->DeviceObject;
    g_FileMonitorContext->IsMonitoring = FALSE;

    // 初始化系统进程过滤器
    InitializeSystemProcessFilter();

    return status;
}

// 反初始化文件监控模块
VOID UninitializeFileMonitor()
{
    // 清理系统进程过滤器
    while (g_SystemProcessFilterList) {
        PSYSTEM_PROCESS_FILTER next = g_SystemProcessFilterList->Next;
        ExFreePoolWithTag(g_SystemProcessFilterList, 'FMon');
        g_SystemProcessFilterList = next;
    }

    if (g_FileMonitorContext) {
        ExFreePoolWithTag(g_FileMonitorContext, 'FMon');
        g_FileMonitorContext = NULL;
    }
}

// 启动文件监控
NTSTATUS StartFileMonitor(
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength,
    PULONG ReturnValue
)
{
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    if (!g_FileMonitorContext) {
        return STATUS_UNSUCCESSFUL;
    }

    if (!g_FileMonitorContext->IsMonitoring) {
        g_FileMonitorContext->IsMonitoring = TRUE;
        KeQuerySystemTime(&g_FileMonitorContext->StartTime);

        // 清空缓冲区
        KIRQL oldIrql;
        KeAcquireSpinLock(&g_FileMonitorContext->EventBuffer.BufferLock, &oldIrql);
        g_FileMonitorContext->EventBuffer.ReadIndex = 0;
        g_FileMonitorContext->EventBuffer.WriteIndex = 0;
        g_FileMonitorContext->EventBuffer.EventCount = 0;
        g_FileMonitorContext->EventBuffer.TotalEvents = 0;
        KeReleaseSpinLock(&g_FileMonitorContext->EventBuffer.BufferLock, oldIrql);
    }

    *ReturnValue = 0;
    return STATUS_SUCCESS;
}

// 停止文件监控
NTSTATUS StopFileMonitor(
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength,
    PULONG ReturnValue
)
{
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    if (g_FileMonitorContext) {
        g_FileMonitorContext->IsMonitoring = FALSE;
    }

    *ReturnValue = 0;
    return STATUS_SUCCESS;
}

// 获取文件事件
NTSTATUS GetFileEvents(
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength,
    PULONG ReturnValue
)
{
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferLength);

    KIRQL oldIrql;
    ULONG eventsToRead = 0;
    ULONG i = 0;
    PFILE_EVENT_PACKET packet = (PFILE_EVENT_PACKET)OutputBuffer;

    if (!g_FileMonitorContext || !g_FileMonitorContext->IsMonitoring || g_FileMonitorContext->EventBuffer.EventCount == 0) {
        *ReturnValue = 0;
        return STATUS_SUCCESS;
    }

    if (!packet || packet->BufferSize < sizeof(FILE_EVENT_PACKET)) {
        return STATUS_INVALID_PARAMETER;
    }

    // 计算可容纳的最大事件数
    ULONG maxEventsCanHold = (packet->BufferSize - FIELD_OFFSET(FILE_EVENT_PACKET, Events)) / sizeof(FILE_EVENT);
    ULONG requestedEvents = packet->EventCount;

    if (requestedEvents > maxEventsCanHold) {
        requestedEvents = maxEventsCanHold;
    }

    KeAcquireSpinLock(&g_FileMonitorContext->EventBuffer.BufferLock, &oldIrql);
    eventsToRead = min(requestedEvents, g_FileMonitorContext->EventBuffer.EventCount);

    for (i = 0; i < eventsToRead; i++) {
        ULONG index = (g_FileMonitorContext->EventBuffer.ReadIndex + i) % MAX_FILE_EVENTS;
        RtlCopyMemory(
            &packet->Events[i],
            &g_FileMonitorContext->EventBuffer.Events[index],
            sizeof(FILE_EVENT)
        );
    }

    packet->EventCount = eventsToRead;
    *ReturnValue = eventsToRead * sizeof(FILE_EVENT);

    g_FileMonitorContext->EventBuffer.ReadIndex =
        (g_FileMonitorContext->EventBuffer.ReadIndex + eventsToRead) % MAX_FILE_EVENTS;
    g_FileMonitorContext->EventBuffer.EventCount -= eventsToRead;

    KeReleaseSpinLock(&g_FileMonitorContext->EventBuffer.BufferLock, oldIrql);
    return STATUS_SUCCESS;
}

// 添加文件事件到缓冲区
VOID AddFileEventToBuffer(PFILE_EVENT Event)
{
    if (!g_FileMonitorContext || !g_FileMonitorContext->IsMonitoring) {
        return;
    }

    KIRQL oldIrql;
    KeAcquireSpinLock(&g_FileMonitorContext->EventBuffer.BufferLock, &oldIrql);

    // 如果缓冲区已满，覆盖最旧的事件
    if (g_FileMonitorContext->EventBuffer.EventCount >= MAX_FILE_EVENTS) {
        g_FileMonitorContext->EventBuffer.ReadIndex =
            (g_FileMonitorContext->EventBuffer.ReadIndex + 1) % MAX_FILE_EVENTS;
        g_FileMonitorContext->EventBuffer.EventCount--;
    }

    // 添加新事件
    ULONG writeIndex = g_FileMonitorContext->EventBuffer.WriteIndex;
    RtlCopyMemory(
        &g_FileMonitorContext->EventBuffer.Events[writeIndex],
        Event,
        sizeof(FILE_EVENT)
    );

    g_FileMonitorContext->EventBuffer.WriteIndex =
        (writeIndex + 1) % MAX_FILE_EVENTS;
    g_FileMonitorContext->EventBuffer.EventCount++;
    g_FileMonitorContext->EventBuffer.TotalEvents++;

    KeReleaseSpinLock(&g_FileMonitorContext->EventBuffer.BufferLock, oldIrql);
}

// 记录文件操作
VOID LogFileOperation(
    PFLT_CALLBACK_DATA Data,
    PCSTR OperationName
)
{
    NTSTATUS status;
    PFLT_FILE_NAME_INFORMATION fileNameInfo = NULL;

    //
    // 获取进程信息
    //
    HANDLE processId =
        PsGetCurrentProcessId();

    //
    // 创建文件事件
    //
    FILE_EVENT fileEvent = { 0 };

    fileEvent.ProcessId =
        (ULONG)(ULONG_PTR)processId;

    //
    // 设置事件类型
    //
    if (strcmp(OperationName, "CREATE/OPEN") == 0)
    {
        fileEvent.Type =
            FileCreateOrOpen;
    }
    else if (strcmp(OperationName, "READ") == 0)
    {
        fileEvent.Type =
            FileRead;
    }
    else if (strcmp(OperationName, "WRITE") == 0)
    {
        fileEvent.Type =
            FileWrite;
    }
    else if (strcmp(OperationName, "DELETE") == 0)
    {
        fileEvent.Type =
            FileDelete;
    }
    else if (strcmp(OperationName, "RENAME") == 0)
    {
        fileEvent.Type =
            FileRename;
    }
    else
    {
        fileEvent.Type =
            FileSetInfo;
    }

    //
    // 设置本地时间
    //
    LARGE_INTEGER systemTime;
    LARGE_INTEGER localTime;

    KeQuerySystemTimePrecise(
        &systemTime
    );

    ExSystemTimeToLocalTime(
        &systemTime,
        &localTime
    );

    fileEvent.TimeStamp =
        localTime.QuadPart;

    //
    // 获取文件名
    //
    status = FltGetFileNameInformation(
        Data,
        FLT_FILE_NAME_NORMALIZED |
        FLT_FILE_NAME_QUERY_DEFAULT,
        &fileNameInfo
    );

    if (NT_SUCCESS(status))
    {
        status =
            FltParseFileNameInformation(
                fileNameInfo
            );
    }

    //
    // 设置文件路径
    //
    if (NT_SUCCESS(status) &&
        fileNameInfo &&
        fileNameInfo->Name.Buffer)
    {
        ULONG copyLength = min(
            fileNameInfo->Name.Length,
            sizeof(fileEvent.FilePath)
            - sizeof(WCHAR)
        );

        RtlCopyMemory(
            fileEvent.FilePath,
            fileNameInfo->Name.Buffer,
            copyLength
        );

        fileEvent.FilePath[
            copyLength / sizeof(WCHAR)
        ] = L'\0';
    }

    //
    // 设置事件ID
    //
    if (g_FileMonitorContext)
    {
        fileEvent.EventId =
            g_FileMonitorContext
            ->EventBuffer.TotalEvents + 1;
    }
    else
    {
        fileEvent.EventId = 1;
    }

    //
    // 添加到缓冲区
    //
    AddFileEventToBuffer(
        &fileEvent
    );

    //
    // 释放文件名信息
    //
    if (fileNameInfo)
    {
        FltReleaseFileNameInformation(
            fileNameInfo
        );
    }
}

// MiniFilter 预操作回调
FLT_PREOP_CALLBACK_STATUS FilePreOperationCallback(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID* CompletionContext
)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
    PCSTR operationName = NULL;

    // 确定操作类型
    switch (iopb->MajorFunction) {
    case IRP_MJ_CREATE:
        operationName = "CREATE/OPEN";
        break;

    case IRP_MJ_READ:
        operationName = "READ";
        break;

    case IRP_MJ_WRITE:
        operationName = "WRITE";
        break;

    case IRP_MJ_SET_INFORMATION:
        // 检查是否为删除操作
        if (iopb->Parameters.SetFileInformation.FileInformationClass == FileDispositionInformation ||
            iopb->Parameters.SetFileInformation.FileInformationClass == FileDispositionInformationEx) {
            if (iopb->Parameters.SetFileInformation.InfoBuffer != NULL) {
                PFILE_DISPOSITION_INFORMATION dispositionInfo =
                    (PFILE_DISPOSITION_INFORMATION)iopb->Parameters.SetFileInformation.InfoBuffer;

                if (dispositionInfo->DeleteFile) {
                    operationName = "DELETE";
                }
                else {
                    operationName = "SET_INFORMATION";
                }
            }
            else {
                operationName = "SET_INFORMATION";
            }
        }
        // 检查是否为重命名操作
        else if (iopb->Parameters.SetFileInformation.FileInformationClass == FileRenameInformation ||
            iopb->Parameters.SetFileInformation.FileInformationClass == FileRenameInformationEx) {
            operationName = "RENAME";
        }
        else {
            operationName = "SET_INFORMATION";
        }
        break;

    default:
        operationName = NULL;
        break;
    }

    // 记录文件操作
    if (operationName != NULL) {
        LogFileOperation(Data, operationName);
    }

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

// MiniFilter 后操作回调
FLT_POSTOP_CALLBACK_STATUS FilePostOperationCallback(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID CompletionContext,
    FLT_POST_OPERATION_FLAGS Flags
)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    return FLT_POSTOP_FINISHED_PROCESSING;
}

// MiniFilter 操作注册表
CONST FLT_OPERATION_REGISTRATION FilterCallbacks[] = {
    { IRP_MJ_CREATE, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_CREATE_NAMED_PIPE, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_CLOSE, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_READ, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_WRITE, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_QUERY_INFORMATION, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_SET_INFORMATION, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_QUERY_EA, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_SET_EA, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_FLUSH_BUFFERS, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_QUERY_VOLUME_INFORMATION, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_SET_VOLUME_INFORMATION, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_DIRECTORY_CONTROL, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_FILE_SYSTEM_CONTROL, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_DEVICE_CONTROL, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_INTERNAL_DEVICE_CONTROL, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_SHUTDOWN, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_LOCK_CONTROL, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_CLEANUP, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_CREATE_MAILSLOT, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_QUERY_SECURITY, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_SET_SECURITY, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_QUERY_QUOTA, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_SET_QUOTA, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_PNP, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_RELEASE_FOR_SECTION_SYNCHRONIZATION, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_ACQUIRE_FOR_MOD_WRITE, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_RELEASE_FOR_MOD_WRITE, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_ACQUIRE_FOR_CC_FLUSH, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_RELEASE_FOR_CC_FLUSH, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_FAST_IO_CHECK_IF_POSSIBLE, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_NETWORK_QUERY_OPEN, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_MDL_READ, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_MDL_READ_COMPLETE, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_PREPARE_MDL_WRITE, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_MDL_WRITE_COMPLETE, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_VOLUME_MOUNT, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_VOLUME_DISMOUNT, 0, FilePreOperationCallback, FilePostOperationCallback },
    { IRP_MJ_OPERATION_END }
};

CONST FLT_REGISTRATION FilterRegistration = {
    sizeof(FLT_REGISTRATION),           // Size
    FLT_REGISTRATION_VERSION,           // Version - 必须为此值
    0,                                  // Flags
    NULL,                               // ContextRegistration
    FilterCallbacks,                    // OperationRegistration
    PtUnload,                           // FilterUnloadCallback
    PtInstanceSetup,                    // InstanceSetupCallback
    NULL,                               // InstanceQueryTeardownCallback
    NULL,                               // InstanceTeardownStartCallback
    NULL,                               // InstanceTeardownCompleteCallback
    NULL,                               // GenerateFileNameCallback
    NULL,                               // NormalizeNameComponentCallback
    NULL,                               // NormalizeContextCleanupCallback
    NULL,                               // TransactionNotificationCallback
    NULL,                               // NormalizeNameComponentExCallback
    NULL                                // SectionNotificationCallback
};
// MiniFilter 实例设置
NTSTATUS PtInstanceSetup(
    PCFLT_RELATED_OBJECTS FltObjects,
    FLT_INSTANCE_SETUP_FLAGS Flags,
    DEVICE_TYPE VolumeDeviceType,
    FLT_FILESYSTEM_TYPE VolumeFilesystemType
)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(VolumeDeviceType);
    UNREFERENCED_PARAMETER(VolumeFilesystemType);

    return STATUS_SUCCESS;
}

// MiniFilter 卸载
NTSTATUS PtUnload(FLT_FILTER_UNLOAD_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Flags);

    if (gFilterHandle) {
        FltUnregisterFilter(gFilterHandle);
        gFilterHandle = NULL;
    }

    return STATUS_SUCCESS;
}