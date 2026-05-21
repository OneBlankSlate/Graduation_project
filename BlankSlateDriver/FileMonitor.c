#include "FileMonitor.h"
#include "ProcessHelper.h"
#include "SystemHelper.h"

// 全局变量定义
PFILE_MONITOR_CONTEXT g_FileMonitorContext = NULL;
PSYSTEM_PROCESS_FILTER g_SystemProcessFilterList = NULL;
PFLT_FILTER gFilterHandle = NULL;
PPROTECTED_FILE_ENTRY g_ProtectedFileList = NULL;
FAST_MUTEX g_ProtectedFileListMutex;

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

    // 初始化文件保护互斥锁
    ExInitializeFastMutex(&g_ProtectedFileListMutex);
    g_ProtectedFileList = NULL;

    return status;
}

// 反初始化文件监控模块
VOID UninitializeFileMonitor()
{
    // 清理文件保护列表
    CleanupFileProtectionList();

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

// 大小写不敏感的宽字符串比较
BOOLEAN WcsEqualIgnoreCase(WCHAR* str1, WCHAR* str2)
{
    if (!str1 || !str2) return FALSE;
    while (*str1 && *str2) {
        WCHAR c1 = (*str1 >= L'A' && *str1 <= L'Z') ? *str1 + (L'a' - L'A') : *str1;
        WCHAR c2 = (*str2 >= L'A' && *str2 <= L'Z') ? *str2 + (L'a' - L'A') : *str2;
        if (c1 != c2) return FALSE;
        str1++;
        str2++;
    }
    return *str1 == *str2;
}

// 添加文件保护
VOID AddFileProtection(WCHAR* FilePath, ULONG ProtectFlag)
{
    ExAcquireFastMutex(&g_ProtectedFileListMutex);

    // 搜索是否已存在该文件的保护条目
    PPROTECTED_FILE_ENTRY entry = g_ProtectedFileList;
    while (entry) {
        if (WcsEqualIgnoreCase(entry->FilePath, FilePath)) {
            entry->ProtectFlags |= ProtectFlag;
            ExReleaseFastMutex(&g_ProtectedFileListMutex);
            return;
        }
        entry = entry->Next;
    }

    // 创建新条目
    entry = (PPROTECTED_FILE_ENTRY)ExAllocatePoolWithTag(
        NonPagedPool, sizeof(PROTECTED_FILE_ENTRY), 'FPro');

    if (entry) {
        ULONG copyLen = 0;
        for (copyLen = 0; copyLen < 519 && FilePath[copyLen] != L'\0'; copyLen++) {
            entry->FilePath[copyLen] = FilePath[copyLen];
        }
        entry->FilePath[copyLen] = L'\0';
        entry->ProtectFlags = ProtectFlag;
        entry->Next = g_ProtectedFileList;
        g_ProtectedFileList = entry;
    }

    ExReleaseFastMutex(&g_ProtectedFileListMutex);
}

// 移除文件保护
VOID RemoveFileProtection(WCHAR* FilePath, ULONG ProtectFlag)
{
    ExAcquireFastMutex(&g_ProtectedFileListMutex);

    PPROTECTED_FILE_ENTRY entry = g_ProtectedFileList;
    PPROTECTED_FILE_ENTRY prev = NULL;

    while (entry) {
        if (WcsEqualIgnoreCase(entry->FilePath, FilePath)) {
            entry->ProtectFlags &= ~ProtectFlag;
            if (entry->ProtectFlags == 0) {
                // 所有保护已移除，删除该条目
                if (prev) {
                    prev->Next = entry->Next;
                }
                else {
                    g_ProtectedFileList = entry->Next;
                }
                ExFreePoolWithTag(entry, 'FPro');
            }
            ExReleaseFastMutex(&g_ProtectedFileListMutex);
            return;
        }
        prev = entry;
        entry = entry->Next;
    }

    ExReleaseFastMutex(&g_ProtectedFileListMutex);
}

// 检查文件是否受指定保护
BOOLEAN IsFileProtected(WCHAR* FilePath, ULONG ProtectFlag)
{
    ExAcquireFastMutex(&g_ProtectedFileListMutex);

    PPROTECTED_FILE_ENTRY entry = g_ProtectedFileList;
    while (entry) {
        if (WcsEqualIgnoreCase(entry->FilePath, FilePath) &&
            (entry->ProtectFlags & ProtectFlag)) {
            ExReleaseFastMutex(&g_ProtectedFileListMutex);
            return TRUE;
        }
        entry = entry->Next;
    }

    ExReleaseFastMutex(&g_ProtectedFileListMutex);
    return FALSE;
}

// 清理文件保护列表
VOID CleanupFileProtectionList()
{
    ExAcquireFastMutex(&g_ProtectedFileListMutex);

    while (g_ProtectedFileList) {
        PPROTECTED_FILE_ENTRY next = g_ProtectedFileList->Next;
        ExFreePoolWithTag(g_ProtectedFileList, 'FPro');
        g_ProtectedFileList = next;
    }

    ExReleaseFastMutex(&g_ProtectedFileListMutex);
}

// 文件保护IOCTL处理函数
NTSTATUS PsProtectFileDelete(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, PULONG ReturnValue)
{
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    if (InputBufferLength < sizeof(COMMUNICATE_FILE_PROTECT)) {
        return STATUS_INVALID_PARAMETER;
    }

    PCOMMUNICATE_FILE_PROTECT input = (PCOMMUNICATE_FILE_PROTECT)InputBuffer;
    AddFileProtection(input->FilePath, FILE_PROTECT_DELETE);

    *ReturnValue = 0;
    return STATUS_SUCCESS;
}

NTSTATUS PsUnprotectFileDelete(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, PULONG ReturnValue)
{
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    if (InputBufferLength < sizeof(COMMUNICATE_FILE_PROTECT)) {
        return STATUS_INVALID_PARAMETER;
    }

    PCOMMUNICATE_FILE_PROTECT input = (PCOMMUNICATE_FILE_PROTECT)InputBuffer;
    RemoveFileProtection(input->FilePath, FILE_PROTECT_DELETE);

    *ReturnValue = 0;
    return STATUS_SUCCESS;
}

NTSTATUS PsProtectFileModify(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, PULONG ReturnValue)
{
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    if (InputBufferLength < sizeof(COMMUNICATE_FILE_PROTECT)) {
        return STATUS_INVALID_PARAMETER;
    }

    PCOMMUNICATE_FILE_PROTECT input = (PCOMMUNICATE_FILE_PROTECT)InputBuffer;
    AddFileProtection(input->FilePath, FILE_PROTECT_MODIFY);

    *ReturnValue = 0;
    return STATUS_SUCCESS;
}

NTSTATUS PsUnprotectFileModify(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, PULONG ReturnValue)
{
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    if (InputBufferLength < sizeof(COMMUNICATE_FILE_PROTECT)) {
        return STATUS_INVALID_PARAMETER;
    }

    PCOMMUNICATE_FILE_PROTECT input = (PCOMMUNICATE_FILE_PROTECT)InputBuffer;
    RemoveFileProtection(input->FilePath, FILE_PROTECT_MODIFY);

    *ReturnValue = 0;
    return STATUS_SUCCESS;
}

NTSTATUS PsProtectFileCopy(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, PULONG ReturnValue)
{
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    if (InputBufferLength < sizeof(COMMUNICATE_FILE_PROTECT)) {
        return STATUS_INVALID_PARAMETER;
    }

    PCOMMUNICATE_FILE_PROTECT input = (PCOMMUNICATE_FILE_PROTECT)InputBuffer;
    AddFileProtection(input->FilePath, FILE_PROTECT_COPY);

    *ReturnValue = 0;
    return STATUS_SUCCESS;
}

NTSTATUS PsUnprotectFileCopy(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, PULONG ReturnValue)
{
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    if (InputBufferLength < sizeof(COMMUNICATE_FILE_PROTECT)) {
        return STATUS_INVALID_PARAMETER;
    }

    PCOMMUNICATE_FILE_PROTECT input = (PCOMMUNICATE_FILE_PROTECT)InputBuffer;
    RemoveFileProtection(input->FilePath, FILE_PROTECT_COPY);

    *ReturnValue = 0;
    return STATUS_SUCCESS;
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
    ULONG protectFlag = 0;  // 需要检查的保护标志

    // 确定操作类型及对应保护标志
    switch (iopb->MajorFunction) {
    case IRP_MJ_CREATE:
        operationName = "CREATE/OPEN";
        {
            ACCESS_MASK desiredAccess = iopb->Parameters.Create.SecurityContext->DesiredAccess;
            ULONG createOptions = iopb->Parameters.Create.Options;

            // 防删除：检查 DELETE 访问权限和 FILE_DELETE_ON_CLOSE 标志
            if ((desiredAccess & DELETE) || (createOptions & FILE_DELETE_ON_CLOSE)) {
                protectFlag |= FILE_PROTECT_DELETE;
            }
            // 防修改：检查写访问权限（GENERIC_WRITE 包含 FILE_WRITE_DATA 等）
            if (desiredAccess & (GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA)) {
                protectFlag |= FILE_PROTECT_MODIFY;
            }
            // 防复制：检查读访问权限（GENERIC_READ 包含 FILE_READ_DATA）
            if (desiredAccess & (GENERIC_READ | FILE_READ_DATA)) {
                protectFlag |= FILE_PROTECT_COPY;
            }
        }
        break;

    case IRP_MJ_READ:
        operationName = "READ";
        protectFlag = FILE_PROTECT_COPY;  // 防复制：阻止读取
        break;

    case IRP_MJ_WRITE:
        operationName = "WRITE";
        protectFlag = FILE_PROTECT_MODIFY;  // 防修改：阻止写入
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
                    protectFlag = FILE_PROTECT_DELETE;  // 防删除：阻止删除操作
                }
                else {
                    operationName = "SET_INFORMATION";
                    protectFlag = FILE_PROTECT_MODIFY;
                }
            }
            else {
                operationName = "SET_INFORMATION";
                protectFlag = FILE_PROTECT_MODIFY;
            }
        }
        // 检查是否为重命名操作
        else if (iopb->Parameters.SetFileInformation.FileInformationClass == FileRenameInformation ||
            iopb->Parameters.SetFileInformation.FileInformationClass == FileRenameInformationEx) {
            operationName = "RENAME";
            protectFlag = FILE_PROTECT_MODIFY;  // 重命名也是修改
        }
        else {
            operationName = "SET_INFORMATION";
            protectFlag = FILE_PROTECT_MODIFY;
        }
        break;

    default:
        operationName = NULL;
        break;
    }

    // 记录文件操作（仅监控状态下）
    if (operationName != NULL && g_FileMonitorContext && g_FileMonitorContext->IsMonitoring) {
        LogFileOperation(Data, operationName);
    }

    // 检查文件保护
    if (protectFlag != 0 && g_ProtectedFileList != NULL) {
        PFLT_FILE_NAME_INFORMATION fileNameInfo = NULL;
        NTSTATUS status = FltGetFileNameInformation(
            Data,
            FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
            &fileNameInfo
        );

        if (NT_SUCCESS(status)) {
            status = FltParseFileNameInformation(fileNameInfo);
        }

        if (NT_SUCCESS(status) && fileNameInfo && fileNameInfo->Name.Buffer) {
            WCHAR filePath[520] = { 0 };
            ULONG copyLength = min(
                fileNameInfo->Name.Length,
                sizeof(filePath) - sizeof(WCHAR)
            );

            RtlCopyMemory(filePath, fileNameInfo->Name.Buffer, copyLength);
            filePath[copyLength / sizeof(WCHAR)] = L'\0';

            // 检查该文件是否受保护
            ULONG matchedFlags = 0;
            ExAcquireFastMutex(&g_ProtectedFileListMutex);
            PPROTECTED_FILE_ENTRY entry = g_ProtectedFileList;
            while (entry) {
                if (WcsEqualIgnoreCase(entry->FilePath, filePath)) {
                    matchedFlags = entry->ProtectFlags;
                    break;
                }
                entry = entry->Next;
            }
            ExReleaseFastMutex(&g_ProtectedFileListMutex);

            FltReleaseFileNameInformation(fileNameInfo);

            if (matchedFlags & protectFlag) {
                Data->IoStatus.Status = STATUS_ACCESS_DENIED;
                Data->IoStatus.Information = 0;
                return FLT_PREOP_COMPLETE;
            }
        }
        else if (fileNameInfo) {
            FltReleaseFileNameInformation(fileNameInfo);
        }
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