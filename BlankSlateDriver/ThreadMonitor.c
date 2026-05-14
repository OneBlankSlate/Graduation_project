// ThreadMonitor.c - 线程监控内核实现
#include "ThreadMonitor.h"
#include "ProcessPath.h"

// 声明线程信息查询相关
NTSYSCALLAPI NTSTATUS ZwQueryInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ THREADINFOCLASS ThreadInformationClass,
    _Out_ PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength,
    _Out_opt_ PULONG ReturnLength
);

// 线程查询访问权限
#ifndef THREAD_QUERY_INFORMATION
#define THREAD_QUERY_INFORMATION (0x0040)
#endif

// 线程基本信息结构
typedef struct _MY_THREAD_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    PVOID TebBaseAddress;
    CLIENT_ID ClientId;
    ULONG_PTR AffinityMask;
    LONG Priority;
    LONG BasePriority;
} MY_THREAD_BASIC_INFORMATION, * PMY_THREAD_BASIC_INFORMATION;

// 全局监控上下文
PTHREAD_MONITOR_CONTEXT g_ThreadContext = NULL;

// 开始线程监控
NTSTATUS MonitorThread()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    // 分配上下文内存
    g_ThreadContext = (PTHREAD_MONITOR_CONTEXT)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(THREAD_MONITOR_CONTEXT),
        THREAD_CONTEXT_TAG
    );

    if (!g_ThreadContext) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(g_ThreadContext, sizeof(THREAD_MONITOR_CONTEXT));

    // 初始化自旋锁
    KeInitializeSpinLock(&g_ThreadContext->EventBuffer.BufferLock);

    g_ThreadContext->IsMonitoring = FALSE;
    g_ThreadContext->EventBuffer.ReadIndex = 0;
    g_ThreadContext->EventBuffer.WriteIndex = 0;
    g_ThreadContext->EventBuffer.EventCount = 0;
    g_ThreadContext->EventBuffer.TotalEvents = 0;

    if (!g_ThreadContext->IsMonitoring) {
        // 注册线程通知回调
        status = PsSetCreateThreadNotifyRoutine(ThreadNotifyCallback);

        if (NT_SUCCESS(status)) {
            g_ThreadContext->IsMonitoring = TRUE;
            KeQuerySystemTime(&g_ThreadContext->StartTime);
            g_ThreadContext->NotifyHandle = ThreadNotifyCallback;
        }
        else {
            ExFreePoolWithTag(g_ThreadContext, THREAD_CONTEXT_TAG);
            g_ThreadContext = NULL;
        }
    }
    else {
        status = STATUS_SUCCESS;
    }

    return status;
}

// 停止线程监控
NTSTATUS StopMonitorThread()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (g_ThreadContext && g_ThreadContext->IsMonitoring && g_ThreadContext->NotifyHandle) {
        // 移除线程通知回调
        status = PsRemoveCreateThreadNotifyRoutine(g_ThreadContext->NotifyHandle);

        if (NT_SUCCESS(status)) {
            g_ThreadContext->IsMonitoring = FALSE;
            g_ThreadContext->NotifyHandle = NULL;

            // 清理缓冲区
            KeAcquireSpinLockAtDpcLevel(&g_ThreadContext->EventBuffer.BufferLock);
            g_ThreadContext->EventBuffer.ReadIndex = 0;
            g_ThreadContext->EventBuffer.WriteIndex = 0;
            g_ThreadContext->EventBuffer.EventCount = 0;
            KeReleaseSpinLockFromDpcLevel(&g_ThreadContext->EventBuffer.BufferLock);
        }
    }
    else {
        status = STATUS_SUCCESS;
    }

    return status;
}

// 获取线程事件
NTSTATUS GetThreadEvents(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
    KIRQL oldIrql;
    ULONG eventsToRead = 0;
    ULONG i;

    if (!g_ThreadContext || !g_ThreadContext->IsMonitoring) {
        return STATUS_UNSUCCESSFUL;
    }

    PTHREAD_EVENT_PACKET packet = (PTHREAD_EVENT_PACKET)OutputBuffer;

    if (!g_ThreadContext->EventBuffer.EventCount || packet->EventCount == 0) {
        packet->EventCount = 0;
        *ReturnValue = sizeof(ULONG);
        return STATUS_SUCCESS;
    }

    KeAcquireSpinLock(&g_ThreadContext->EventBuffer.BufferLock, &oldIrql);

    // 计算要读取的事件数
    eventsToRead = min(packet->EventCount, g_ThreadContext->EventBuffer.EventCount);
    packet->EventCount = eventsToRead;

    // 读取事件
    for (i = 0; i < eventsToRead; i++) {
        ULONG index = (g_ThreadContext->EventBuffer.ReadIndex + i) % THREAD_MAX_EVENTS;
        RtlCopyMemory(&packet->Events[i],
            &g_ThreadContext->EventBuffer.Events[index],
            sizeof(THREAD_EVENT));
    }

    // 更新缓冲区
    g_ThreadContext->EventBuffer.ReadIndex =
        (g_ThreadContext->EventBuffer.ReadIndex + eventsToRead) % THREAD_MAX_EVENTS;
    g_ThreadContext->EventBuffer.EventCount -= eventsToRead;

    KeReleaseSpinLock(&g_ThreadContext->EventBuffer.BufferLock, oldIrql);

    *ReturnValue = sizeof(THREAD_EVENT_PACKET) + (eventsToRead - 1) * sizeof(THREAD_EVENT);
    return STATUS_SUCCESS;
}

// 线程通知回调函数
VOID ThreadNotifyCallback(
    _In_ HANDLE ProcessId,
    _In_ HANDLE ThreadId,
    _In_ BOOLEAN Create
)
{
    THREAD_EVENT event = { 0 };

    if (!g_ThreadContext || !g_ThreadContext->IsMonitoring) {
        return;
    }

    // 设置事件基本信息
    event.Type = Create ? ThreadCreate : ThreadExit;
    event.ThreadId = HandleToULong(ThreadId);
    event.ProcessId = HandleToULong(ProcessId);

    // 获取当前系统时间
    LARGE_INTEGER systemTime;
    KeQuerySystemTime(&systemTime);

    if (Create) {
        event.CreateTime = systemTime.QuadPart;
    }
    else {
        event.ExitTime = systemTime.QuadPart;
    }

    // 获取线程详细信息
    GetThreadInfo(ThreadId, ProcessId, Create, &event);

    // 添加到缓冲区
    AddThreadEventToBuffer(&g_ThreadContext->EventBuffer, &event);
}

// 获取线程详细信息
NTSTATUS GetThreadInfo(
    _In_ HANDLE ThreadId,
    _In_ HANDLE ProcessId,
    _In_ BOOLEAN Create,
    _Inout_ PTHREAD_EVENT Event
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PEPROCESS Process = NULL;
    PETHREAD Thread = NULL;
    WCHAR processPath[520] = { 0 };
    UNICODE_STRING uniPath = { 0 };

    // 通过进程ID获取进程对象
    status = PsLookupProcessByProcessId(ProcessId, &Process);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // 通过线程ID获取线程对象
    status = PsLookupThreadByThreadId(ThreadId, &Thread);
    if (!NT_SUCCESS(status)) {
        ObDereferenceObject(Process);
        return status;
    }

    // 获取进程完整路径
    GetProcessFullPathByEProcess(Process, processPath, sizeof(processPath) / sizeof(WCHAR));
    if (processPath[0] != L'\0') {
        RtlInitUnicodeString(&uniPath, processPath);
        ULONG copyLength = min(uniPath.Length, sizeof(Event->ImagePath) - sizeof(WCHAR));
        RtlCopyMemory(Event->ImagePath, uniPath.Buffer, copyLength);
        Event->ImagePath[copyLength / sizeof(WCHAR)] = L'\0';
    }

    // 从映像路径中提取模块名（取最后一个反斜杠后的部分）
    if (Event->ImagePath[0] != L'\0') {
        UNICODE_STRING imagePathStr;
        RtlInitUnicodeString(&imagePathStr, Event->ImagePath);
        ULONG i;
        ULONG lastSlash = 0;
        for (i = 0; i < imagePathStr.Length / sizeof(WCHAR); i++) {
            if (Event->ImagePath[i] == L'\\') {
                lastSlash = i;
            }
        }
        if (lastSlash > 0) {
            ULONG nameLen = (imagePathStr.Length / sizeof(WCHAR)) - lastSlash - 1;
            ULONG copyLen = min(nameLen, (sizeof(Event->ModuleName) / sizeof(WCHAR)) - 1);
            RtlCopyMemory(Event->ModuleName, &Event->ImagePath[lastSlash + 1], copyLen * sizeof(WCHAR));
            Event->ModuleName[copyLen] = L'\0';
        }
        else {
            ULONG copyLen = min(imagePathStr.Length, sizeof(Event->ModuleName) - sizeof(WCHAR));
            RtlCopyMemory(Event->ModuleName, Event->ImagePath, copyLen);
            Event->ModuleName[copyLen / sizeof(WCHAR)] = L'\0';
        }
    }

    if (Create) {
        // 获取线程动态优先级
        Event->Priority = KeQueryPriorityThread(Thread);
    }

    // 通过ZwQueryInformationThread获取线程基本信息（基本优先级、退出状态）
    HANDLE threadHandle = NULL;
    NTSTATUS objStatus = ObOpenObjectByPointer(
        Thread,
        OBJ_KERNEL_HANDLE,
        NULL,
        THREAD_QUERY_INFORMATION,
        NULL,
        KernelMode,
        &threadHandle
    );

    if (NT_SUCCESS(objStatus)) {
        MY_THREAD_BASIC_INFORMATION basicInfo = { 0 };
        ULONG returnLength = 0;
        NTSTATUS queryStatus = ZwQueryInformationThread(
            threadHandle,
            ThreadBasicInformation,
            &basicInfo,
            sizeof(basicInfo),
            &returnLength
        );

        if (NT_SUCCESS(queryStatus)) {
            Event->BasePriority = (ULONG)basicInfo.BasePriority;
            if (!Create) {
                Event->ExitStatus = (ULONG)basicInfo.ExitStatus;
            }
        }

        ZwClose(threadHandle);
    }

    // 减少引用计数
    ObDereferenceObject(Thread);
    ObDereferenceObject(Process);

    return STATUS_SUCCESS;
}

// 添加事件到缓冲区
VOID AddThreadEventToBuffer(PTHREAD_EVENT_BUFFER Buffer, PTHREAD_EVENT Event)
{
    KIRQL oldIrql;

    if (!Buffer || !Event) {
        return;
    }

    KeAcquireSpinLock(&Buffer->BufferLock, &oldIrql);

    // 复制事件到缓冲区
    RtlCopyMemory(&Buffer->Events[Buffer->WriteIndex], Event, sizeof(THREAD_EVENT));

    // 更新写索引
    Buffer->WriteIndex = (Buffer->WriteIndex + 1) % THREAD_MAX_EVENTS;

    // 如果缓冲区已满，移动读索引
    if (Buffer->EventCount >= THREAD_MAX_EVENTS) {
        Buffer->ReadIndex = (Buffer->ReadIndex + 1) % THREAD_MAX_EVENTS;
    }
    else {
        Buffer->EventCount++;
    }

    Buffer->TotalEvents++;

    KeReleaseSpinLock(&Buffer->BufferLock, oldIrql);
}

// 从缓冲区读取事件
ULONG ReadThreadEventsFromBuffer(PTHREAD_EVENT_BUFFER Buffer, PTHREAD_EVENT OutputBuffer, ULONG MaxEvents)
{
    KIRQL oldIrql;
    ULONG eventsToRead = 0;
    ULONG i;

    if (!Buffer || !OutputBuffer || MaxEvents == 0) {
        return 0;
    }

    KeAcquireSpinLock(&Buffer->BufferLock, &oldIrql);

    // 计算要读取的事件数
    eventsToRead = min(Buffer->EventCount, MaxEvents);

    // 读取事件
    for (i = 0; i < eventsToRead; i++) {
        ULONG index = (Buffer->ReadIndex + i) % THREAD_MAX_EVENTS;
        RtlCopyMemory(&OutputBuffer[i], &Buffer->Events[index], sizeof(THREAD_EVENT));
    }

    // 更新读索引
    Buffer->ReadIndex = (Buffer->ReadIndex + eventsToRead) % THREAD_MAX_EVENTS;
    Buffer->EventCount -= eventsToRead;

    KeReleaseSpinLock(&Buffer->BufferLock, oldIrql);

    return eventsToRead;
}