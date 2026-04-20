#include"ProcMonitor.h"
#include"ProcessPath.h"
PMONITOR_CONTEXT g_context = NULL;
NTSTATUS MonitorProcess()
{
    //__debugbreak();
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    // 分配驱动上下文
    g_context = (PMONITOR_CONTEXT)ExAllocatePoolWithTag(NonPagedPool, sizeof(MONITOR_CONTEXT), CONTEXT_TAG);
    if (!g_context) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(g_context, sizeof(MONITOR_CONTEXT));
    g_context->IsMonitoring = FALSE;
    g_context->NotifyHandle = NULL;
    if (!g_context->IsMonitoring) {
        // 注册进程回调
        status = PsSetCreateProcessNotifyRoutineEx(ProcessNotifyCallback, FALSE);
        if (NT_SUCCESS(status)) {
            g_context->IsMonitoring = TRUE;
            KeQuerySystemTime(&g_context->StartTime);
            g_context->NotifyHandle = &ProcessNotifyCallback;
        }
        else {
        }
    }
    else {
        status = STATUS_SUCCESS;
    }
    return status;
}
NTSTATUS StopMonitorProcess()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    if (g_context->IsMonitoring && g_context->NotifyHandle) {
        // 注销进程回调
        status = PsSetCreateProcessNotifyRoutineEx(ProcessNotifyCallback, TRUE);
        if (NT_SUCCESS(status)) {
            g_context->IsMonitoring = FALSE;
            g_context->NotifyHandle = NULL;
        }
        else {
        }
    }
    else {
        status = STATUS_SUCCESS;
    }
    return status;
}
NTSTATUS GetProcEvents(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
    KIRQL oldIrql;
    ULONG eventsToRead = 0;
    ULONG i;
    PEVENT_PACKET packet = (PEVENT_PACKET)OutputBuffer;
    if (!g_context->EventBuffer.EventCount || packet->EventCount == 0) {
        return 0;
    }

    KeAcquireSpinLock(&g_context->EventBuffer.BufferLock, &oldIrql);

    // 计算要读取的事件数量
    eventsToRead = min(packet->EventCount, g_context->EventBuffer.TotalEvents);
    packet->EventCount = eventsToRead;
    // 读取事件
    for (i = 0; i < eventsToRead; i++) {
        ULONG index = (g_context->EventBuffer.ReadIndex + i) % MAX_EVENTS;
        RtlCopyMemory(&packet->Events[i], &g_context->EventBuffer.Events[i], sizeof(PROCESS_EVENT));
    }
    *ReturnValue = eventsToRead * sizeof(PROCESS_EVENT);
    // 更新读索引
    g_context->EventBuffer.ReadIndex = (g_context->EventBuffer.ReadIndex + eventsToRead) % MAX_EVENTS;
    g_context->EventBuffer.EventCount -= eventsToRead;

    KeReleaseSpinLock(&g_context->EventBuffer.BufferLock, oldIrql);

    return eventsToRead;
}
// 进程通知回调函数
VOID ProcessNotifyCallback(
    _Inout_ PEPROCESS Process,
    _In_ HANDLE ProcessId,
    _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo
)
{
    PROCESS_EVENT event = { 0 };

    if (!g_context || !g_context->IsMonitoring) {
        return;
    }

    // 填充事件基本信息
    if (CreateInfo != NULL) {
        // 进程创建事件
        event.Type = ProcessCreate;
        event.ProcessId = HandleToULong(ProcessId);
        event.ParentProcessId = HandleToULong(CreateInfo->ParentProcessId);
        //获取父进程名
        PEPROCESS parentProcess=NULL;
        PsLookupProcessByProcessId(event.ParentProcessId, &parentProcess);
        WCHAR parentProcessPath[520] = { 0 };
        UNICODE_STRING UniParentProcessPath = { 0 };
        GetProcessFullPathByEProcess(parentProcess, parentProcessPath, 520);
        RtlInitUnicodeString(&UniParentProcessPath, parentProcessPath);
        PUNICODE_STRING parentProcessName = GetNameByPath(&UniParentProcessPath);
        if (parentProcessName!=NULL&&parentProcessName->Buffer != NULL && parentProcessName->Length > 0)
        {
            RtlCopyMemory(event.ParentProcessName, parentProcessName->Buffer, parentProcessName->Length);
            
        }
        if (parentProcessName) ExFreePool(parentProcessName);
        // 获取当前时间
        LARGE_INTEGER systemTime;
        KeQuerySystemTime(&systemTime);
        event.CreateTime = systemTime.QuadPart;

        // 获取进程信息
        GetProcessInfo(ProcessId, Process, &event, TRUE);

        // 如果有命令行信息
        if (CreateInfo->CommandLine && CreateInfo->CommandLine->Buffer) {
            ULONG copyLength = min(CreateInfo->CommandLine->Length, sizeof(event.CommandLine) - sizeof(WCHAR));
            RtlCopyMemory(event.CommandLine, CreateInfo->CommandLine->Buffer, copyLength);
            event.CommandLine[copyLength / sizeof(WCHAR)] = L'\0';
        }

    }
    else {
        // 进程退出事件
        event.Type = ProcessExit;
        event.ProcessId = HandleToULong(ProcessId);

        // 获取当前时间
        LARGE_INTEGER systemTime;
        KeQuerySystemTime(&systemTime);
        event.ExitTime = systemTime.QuadPart;

        // 获取进程信息
        GetProcessInfo(ProcessId, Process, &event, FALSE);
    }

    // 添加到缓冲区
    InterlockedIncrement((LONG*)&g_context->EventBuffer.TotalEvents);
    event.EventId = g_context->EventBuffer.TotalEvents;
    AddEventToBuffer(&g_context->EventBuffer, &event);
}
// 获取进程信息
NTSTATUS GetProcessInfo(
    _In_ HANDLE ProcessId,
    _In_ PEPROCESS Process,
    _Inout_ PPROCESS_EVENT Event,
    _In_ BOOLEAN Create
)
{
    //__debugbreak();
    NTSTATUS status = STATUS_SUCCESS;
    PUNICODE_STRING UniImageName = NULL;
    WCHAR ImagePath[520] = { 0 };
    UNREFERENCED_PARAMETER(ProcessId);
    UNICODE_STRING uniPath = { 0 };
    GetProcessFullPathByEProcess(Process, ImagePath, 520);
    if (ImagePath) {
        RtlInitUnicodeString(&uniPath, ImagePath);
        RtlCopyMemory(Event->ImagePath, uniPath.Buffer, uniPath.Length);
    }
    else {
        return STATUS_UNSUCCESSFUL;
    }
    // 获取进程映像名称
    UniImageName = GetNameByPath(&uniPath);
    if (UniImageName) {
        RtlCopyMemory(Event->ImageName, UniImageName->Buffer, UniImageName->Length);
        Event->ImageName[UniImageName->Length / sizeof(WCHAR)] = L'\0';
        ExFreePool(UniImageName);
    }
    else {
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

// 添加事件到缓冲区
VOID AddEventToBuffer(PEVENT_BUFFER Buffer, PPROCESS_EVENT Event)
{
    KIRQL oldIrql;

    if (!Buffer || !Event) {
        return;
    }

    KeAcquireSpinLock(&Buffer->BufferLock, &oldIrql);

    // 复制事件到缓冲区
    RtlCopyMemory(&Buffer->Events[Buffer->WriteIndex], Event, sizeof(PROCESS_EVENT));

    // 更新写索引
    Buffer->WriteIndex = (Buffer->WriteIndex + 1) % MAX_EVENTS;

    // 如果缓冲区已满，覆盖最旧的事件
    if (Buffer->EventCount >= MAX_EVENTS) {
        Buffer->ReadIndex = (Buffer->ReadIndex + 1) % MAX_EVENTS;
    }
    else {
        Buffer->EventCount++;
    }

    KeReleaseSpinLock(&Buffer->BufferLock, oldIrql);
}

// 从缓冲区读取事件
ULONG ReadEventsFromBuffer(PEVENT_BUFFER Buffer, PPROCESS_EVENT OutputBuffer, ULONG MaxEvents)
{
    KIRQL oldIrql;
    ULONG eventsToRead = 0;
    ULONG i;

    if (!Buffer || !OutputBuffer || MaxEvents == 0) {
        return 0;
    }

    KeAcquireSpinLock(&Buffer->BufferLock, &oldIrql);

    // 计算要读取的事件数量
    eventsToRead = min(Buffer->EventCount, MaxEvents);

    // 读取事件
    for (i = 0; i < eventsToRead; i++) {
        ULONG index = (Buffer->ReadIndex + i) % MAX_EVENTS;
        RtlCopyMemory(&OutputBuffer[i], &Buffer->Events[index], sizeof(PROCESS_EVENT));
    }

    // 更新读索引
    Buffer->ReadIndex = (Buffer->ReadIndex + eventsToRead) % MAX_EVENTS;
    Buffer->EventCount -= eventsToRead;

    KeReleaseSpinLock(&Buffer->BufferLock, oldIrql);

    return eventsToRead;
}