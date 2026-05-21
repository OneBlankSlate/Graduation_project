#include"MultiOpenPrevent.h"

PMULTI_OPEN_CONTEXT g_MultiOpenContext = NULL;

VOID InitializeMultiOpenPrevent()
{
	g_MultiOpenContext = (PMULTI_OPEN_CONTEXT)ExAllocatePoolWithTag(NonPagedPool, sizeof(MULTI_OPEN_CONTEXT), 'pmlt');
	if (g_MultiOpenContext) {
		RtlZeroMemory(g_MultiOpenContext, sizeof(MULTI_OPEN_CONTEXT));
		ExInitializeFastMutex(&g_MultiOpenContext->Mutex);
		g_MultiOpenContext->CallbackRegistered = FALSE;
	}
}

VOID UninitializeMultiOpenPrevent()
{
	if (g_MultiOpenContext) {
		if (g_MultiOpenContext->CallbackRegistered) {
			PsSetCreateProcessNotifyRoutineEx(MultiOpenProcessNotifyCallback, TRUE);
			g_MultiOpenContext->CallbackRegistered = FALSE;
		}
		ExFreePoolWithTag(g_MultiOpenContext, 'pmlt');
		g_MultiOpenContext = NULL;
	}
}

static BOOLEAN IsPreventedListEmpty()
{
	for (ULONG i = 0; i < MULTI_OPEN_MAX_ENTRIES; i++) {
		if (g_MultiOpenContext->Entries[i].Active) {
			return FALSE;
		}
	}
	return TRUE;
}

static BOOLEAN IsImageNameMatch(PCWSTR Name1, PCWSTR Name2)
{
	UNICODE_STRING s1, s2;
	RtlInitUnicodeString(&s1, Name1);
	RtlInitUnicodeString(&s2, Name2);
	return RtlEqualUnicodeString(&s1, &s2, TRUE);
}

static LONG FindEntryByName(PCWSTR ImageName)
{
	for (ULONG i = 0; i < MULTI_OPEN_MAX_ENTRIES; i++) {
		if (g_MultiOpenContext->Entries[i].Active) {
			if (IsImageNameMatch(g_MultiOpenContext->Entries[i].ImageName, ImageName)) {
				return (LONG)i;
			}
		}
	}
	return -1;
}

NTSTATUS PsPreventMultiOpen(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
	NTSTATUS Status = STATUS_SUCCESS;

	if (!InputBuffer || InputBufferLength < sizeof(COMMUNICATE_MULTI_OPEN_PREVENT)) {
		return STATUS_INVALID_PARAMETER;
	}

	PCOMMUNICATE_MULTI_OPEN_PREVENT input = (PCOMMUNICATE_MULTI_OPEN_PREVENT)InputBuffer;
	if (input->ImageName[0] == L'\0') {
		return STATUS_INVALID_PARAMETER;
	}

	if (!g_MultiOpenContext) {
		return STATUS_UNSUCCESSFUL;
	}

	ExAcquireFastMutex(&g_MultiOpenContext->Mutex);

	// Check if already in the list
	if (FindEntryByName(input->ImageName) >= 0) {
		ExReleaseFastMutex(&g_MultiOpenContext->Mutex);
		return STATUS_SUCCESS;
	}

	// Find a free slot
	ULONG slot = MULTI_OPEN_MAX_ENTRIES;
	for (ULONG i = 0; i < MULTI_OPEN_MAX_ENTRIES; i++) {
		if (!g_MultiOpenContext->Entries[i].Active) {
			slot = i;
			break;
		}
	}

	if (slot >= MULTI_OPEN_MAX_ENTRIES) {
		ExReleaseFastMutex(&g_MultiOpenContext->Mutex);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// Register callback if not already registered
	if (!g_MultiOpenContext->CallbackRegistered) {
		Status = PsSetCreateProcessNotifyRoutineEx(MultiOpenProcessNotifyCallback, FALSE);
		if (!NT_SUCCESS(Status)) {
			ExReleaseFastMutex(&g_MultiOpenContext->Mutex);
			return Status;
		}
		g_MultiOpenContext->CallbackRegistered = TRUE;
	}

	// Add to the list
	ULONG nameLen = wcslen(input->ImageName);
	ULONG copyLen = (nameLen < MULTI_OPEN_IMAGE_NAME_LEN - 1) ? nameLen : (MULTI_OPEN_IMAGE_NAME_LEN - 1);
	RtlCopyMemory(g_MultiOpenContext->Entries[slot].ImageName, input->ImageName, copyLen * sizeof(WCHAR));
	g_MultiOpenContext->Entries[slot].ImageName[copyLen] = L'\0';
	g_MultiOpenContext->Entries[slot].Active = TRUE;

	ExReleaseFastMutex(&g_MultiOpenContext->Mutex);

	return STATUS_SUCCESS;
}

NTSTATUS PsCancelMultiOpen(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
	if (!InputBuffer || InputBufferLength < sizeof(COMMUNICATE_MULTI_OPEN_PREVENT)) {
		return STATUS_INVALID_PARAMETER;
	}

	PCOMMUNICATE_MULTI_OPEN_PREVENT input = (PCOMMUNICATE_MULTI_OPEN_PREVENT)InputBuffer;
	if (input->ImageName[0] == L'\0') {
		return STATUS_INVALID_PARAMETER;
	}

	if (!g_MultiOpenContext) {
		return STATUS_UNSUCCESSFUL;
	}

	ExAcquireFastMutex(&g_MultiOpenContext->Mutex);

	LONG idx = FindEntryByName(input->ImageName);
	if (idx >= 0) {
		g_MultiOpenContext->Entries[idx].Active = FALSE;
		g_MultiOpenContext->Entries[idx].ImageName[0] = L'\0';
	}

	// If list is now empty, unregister callback
	if (IsPreventedListEmpty() && g_MultiOpenContext->CallbackRegistered) {
		PsSetCreateProcessNotifyRoutineEx(MultiOpenProcessNotifyCallback, TRUE);
		g_MultiOpenContext->CallbackRegistered = FALSE;
	}

	ExReleaseFastMutex(&g_MultiOpenContext->Mutex);

	return STATUS_SUCCESS;
}

static VOID ExtractFileNameFromPath(PCUNICODE_STRING FullPath, PWSTR FileName, ULONG FileNameMaxChars)
{
	if (!FullPath || !FullPath->Buffer || FullPath->Length == 0 || !FileName) {
		if (FileName) FileName[0] = L'\0';
		return;
	}

	PWCHAR start = FullPath->Buffer;
	PWCHAR lastSlash = NULL;
	ULONG charCount = FullPath->Length / sizeof(WCHAR);

	for (ULONG i = 0; i < charCount; i++) {
		if (start[i] == L'\\' || start[i] == L'/') {
			lastSlash = &start[i];
		}
	}

	PWCHAR nameStart = lastSlash ? (lastSlash + 1) : start;
	ULONG nameLen = charCount - (ULONG)(nameStart - start);

	if (nameLen >= FileNameMaxChars) {
		nameLen = FileNameMaxChars - 1;
	}

	RtlCopyMemory(FileName, nameStart, nameLen * sizeof(WCHAR));
	FileName[nameLen] = L'\0';
}

static BOOLEAN IsImageNamePrevented(PCWSTR ImageName)
{
	for (ULONG i = 0; i < MULTI_OPEN_MAX_ENTRIES; i++) {
		if (g_MultiOpenContext->Entries[i].Active) {
			if (IsImageNameMatch(g_MultiOpenContext->Entries[i].ImageName, ImageName)) {
				return TRUE;
			}
		}
	}
	return FALSE;
}

VOID MultiOpenProcessNotifyCallback(
	_Inout_ PEPROCESS Process,
	_In_ HANDLE ProcessId,
	_Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo
)
{
	UNREFERENCED_PARAMETER(Process);
	UNREFERENCED_PARAMETER(ProcessId);

	if (!g_MultiOpenContext || !g_MultiOpenContext->CallbackRegistered) {
		return;
	}

	// Only handle process creation
	if (CreateInfo == NULL) {
		return;
	}

	// Get the image filename from CreateInfo
	if (!CreateInfo->ImageFileName || !CreateInfo->ImageFileName->Buffer ||
		CreateInfo->ImageFileName->Length == 0) {
		return;
	}

	WCHAR fileName[MULTI_OPEN_IMAGE_NAME_LEN] = { 0 };
	ExtractFileNameFromPath(CreateInfo->ImageFileName, fileName, MULTI_OPEN_IMAGE_NAME_LEN);

	if (fileName[0] == L'\0') {
		return;
	}

	ExAcquireFastMutex(&g_MultiOpenContext->Mutex);

	if (IsImageNamePrevented(fileName)) {
		// Deny the process creation
		CreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
	}

	ExReleaseFastMutex(&g_MultiOpenContext->Mutex);
}
