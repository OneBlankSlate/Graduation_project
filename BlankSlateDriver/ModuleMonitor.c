// ModuleMonitor.c
#include "ModuleMonitor.h"
#include "ProcessPath.h"

PMODULE_MONITOR_CONTEXT g_ModuleContext = NULL;

NTSTATUS MonitorModule()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    g_ModuleContext = (PMODULE_MONITOR_CONTEXT)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(MODULE_MONITOR_CONTEXT),
        MODULE_CONTEXT_TAG
    );

    if (!g_ModuleContext) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(g_ModuleContext, sizeof(MODULE_MONITOR_CONTEXT));
    KeInitializeSpinLock(&g_ModuleContext->EventBuffer.BufferLock);

    g_ModuleContext->IsMonitoring = FALSE;
    g_ModuleContext->EventBuffer.ReadIndex = 0;
    g_ModuleContext->EventBuffer.WriteIndex = 0;
    g_ModuleContext->EventBuffer.EventCount = 0;
    g_ModuleContext->EventBuffer.TotalEvents = 0;

    if (!g_ModuleContext->IsMonitoring) {
        status = PsSetLoadImageNotifyRoutine(LoadImageNotifyCallback);

        if (NT_SUCCESS(status)) {
            g_ModuleContext->IsMonitoring = TRUE;
            KeQuerySystemTime(&g_ModuleContext->StartTime);
            g_ModuleContext->NotifyHandle = LoadImageNotifyCallback;
        }
        else {
            ExFreePoolWithTag(g_ModuleContext, MODULE_CONTEXT_TAG);
            g_ModuleContext = NULL;
        }
    }
    else {
        status = STATUS_SUCCESS;
    }

    return status;
}

NTSTATUS StopMonitorModule()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (g_ModuleContext && g_ModuleContext->IsMonitoring && g_ModuleContext->NotifyHandle) {
        PsRemoveLoadImageNotifyRoutine(LoadImageNotifyCallback);

        g_ModuleContext->IsMonitoring = FALSE;
        g_ModuleContext->NotifyHandle = NULL;

        KeAcquireSpinLockAtDpcLevel(&g_ModuleContext->EventBuffer.BufferLock);
        g_ModuleContext->EventBuffer.ReadIndex = 0;
        g_ModuleContext->EventBuffer.WriteIndex = 0;
        g_ModuleContext->EventBuffer.EventCount = 0;
        KeReleaseSpinLockFromDpcLevel(&g_ModuleContext->EventBuffer.BufferLock);

        status = STATUS_SUCCESS;
    }
    else {
        status = STATUS_SUCCESS;
    }

    return status;
}

NTSTATUS GetModuleEvents(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
    KIRQL oldIrql;
    ULONG eventsToRead = 0;
    ULONG i;
    PMODULE_EVENT_PACKET packet;
    ULONG index;

    if (!g_ModuleContext || !g_ModuleContext->IsMonitoring) {
        return STATUS_UNSUCCESSFUL;
    }

    packet = (PMODULE_EVENT_PACKET)OutputBuffer;

    if (!g_ModuleContext->EventBuffer.EventCount || packet->EventCount == 0) {
        packet->EventCount = 0;
        *ReturnValue = sizeof(ULONG);
        return STATUS_SUCCESS;
    }

    KeAcquireSpinLock(&g_ModuleContext->EventBuffer.BufferLock, &oldIrql);

    eventsToRead = min(packet->EventCount, g_ModuleContext->EventBuffer.EventCount);
    packet->EventCount = eventsToRead;

    for (i = 0; i < eventsToRead; i++) {
        index = (g_ModuleContext->EventBuffer.ReadIndex + i) % MODULE_MAX_EVENTS;
        RtlCopyMemory(&packet->Events[i],
            &g_ModuleContext->EventBuffer.Events[index],
            sizeof(MODULE_EVENT));
    }

    g_ModuleContext->EventBuffer.ReadIndex =
        (g_ModuleContext->EventBuffer.ReadIndex + eventsToRead) % MODULE_MAX_EVENTS;
    g_ModuleContext->EventBuffer.EventCount -= eventsToRead;

    KeReleaseSpinLock(&g_ModuleContext->EventBuffer.BufferLock, oldIrql);

    *ReturnValue = sizeof(MODULE_EVENT_PACKET) + (eventsToRead - 1) * sizeof(MODULE_EVENT);
    return STATUS_SUCCESS;
}

VOID LoadImageNotifyCallback(
    _In_opt_ PUNICODE_STRING FullImageName,
    _In_ HANDLE ProcessId,
    _In_ PIMAGE_INFO ImageInfo
)
{
    MODULE_EVENT event;
    LARGE_INTEGER systemTime;

    if (!g_ModuleContext || !g_ModuleContext->IsMonitoring) {
        return;
    }

    RtlZeroMemory(&event, sizeof(MODULE_EVENT));

    event.Type = ImageLoad;
    event.ProcessId = HandleToULong(ProcessId);

    KeQuerySystemTime(&systemTime);
    event.LoadTime = systemTime.QuadPart;

    GetModuleInfo(ProcessId, FullImageName, ImageInfo, &event);

    AddModuleEventToBuffer(&g_ModuleContext->EventBuffer, &event);
}

NTSTATUS GetModuleInfo(
    _In_ HANDLE ProcessId,
    _In_opt_ PUNICODE_STRING FullImageName,
    _In_ PIMAGE_INFO ImageInfo,
    _Inout_ PMODULE_EVENT Event
)
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG copyLength;
    ULONG i;
    ULONG lastSlash;
    ULONG nameLen;
    ULONG copyLen;

    if (FullImageName && FullImageName->Buffer && FullImageName->Length > 0) {
        copyLength = min(FullImageName->Length, sizeof(Event->ImagePath) - sizeof(WCHAR));
        RtlCopyMemory(Event->ImagePath, FullImageName->Buffer, copyLength);
        Event->ImagePath[copyLength / sizeof(WCHAR)] = L'\0';

        lastSlash = 0;
        for (i = 0; i < copyLength / sizeof(WCHAR); i++) {
            if (Event->ImagePath[i] == L'\\') {
                lastSlash = i;
            }
        }
        if (lastSlash > 0) {
            nameLen = (copyLength / sizeof(WCHAR)) - lastSlash - 1;
            copyLen = min(nameLen, (sizeof(Event->ImageName) / sizeof(WCHAR)) - 1);
            RtlCopyMemory(Event->ImageName, &Event->ImagePath[lastSlash + 1], copyLen * sizeof(WCHAR));
            Event->ImageName[copyLen] = L'\0';
        }
        else {
            copyLen = min(copyLength, sizeof(Event->ImageName) - sizeof(WCHAR));
            RtlCopyMemory(Event->ImageName, Event->ImagePath, copyLen);
            Event->ImageName[copyLen / sizeof(WCHAR)] = L'\0';
        }
    }

    Event->ImageBase = (ULONG64)ImageInfo->ImageBase;
    Event->ImageSize = ImageInfo->ImageSize;

    return status;
}

VOID AddModuleEventToBuffer(PMODULE_EVENT_BUFFER Buffer, PMODULE_EVENT Event)
{
    KIRQL oldIrql;

    if (!Buffer || !Event) {
        return;
    }

    KeAcquireSpinLock(&Buffer->BufferLock, &oldIrql);

    RtlCopyMemory(&Buffer->Events[Buffer->WriteIndex], Event, sizeof(MODULE_EVENT));

    Buffer->WriteIndex = (Buffer->WriteIndex + 1) % MODULE_MAX_EVENTS;

    if (Buffer->EventCount >= MODULE_MAX_EVENTS) {
        Buffer->ReadIndex = (Buffer->ReadIndex + 1) % MODULE_MAX_EVENTS;
    }
    else {
        Buffer->EventCount++;
    }

    Buffer->TotalEvents++;

    KeReleaseSpinLock(&Buffer->BufferLock, oldIrql);
}

ULONG ReadModuleEventsFromBuffer(PMODULE_EVENT_BUFFER Buffer, PMODULE_EVENT OutputBuffer, ULONG MaxEvents)
{
    KIRQL oldIrql;
    ULONG eventsToRead = 0;
    ULONG i;
    ULONG index;

    if (!Buffer || !OutputBuffer || MaxEvents == 0) {
        return 0;
    }

    KeAcquireSpinLock(&Buffer->BufferLock, &oldIrql);

    eventsToRead = min(Buffer->EventCount, MaxEvents);

    for (i = 0; i < eventsToRead; i++) {
        index = (Buffer->ReadIndex + i) % MODULE_MAX_EVENTS;
        RtlCopyMemory(&OutputBuffer[i], &Buffer->Events[index], sizeof(MODULE_EVENT));
    }

    Buffer->ReadIndex = (Buffer->ReadIndex + eventsToRead) % MODULE_MAX_EVENTS;
    Buffer->EventCount -= eventsToRead;

    KeReleaseSpinLock(&Buffer->BufferLock, oldIrql);

    return eventsToRead;
}
