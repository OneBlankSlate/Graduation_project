#pragma once
#include<fltKernel.h>
//对象体得对象头
#define OBJECT_TO_OBJECT_HEADER(o) CONTAINING_RECORD((o),OBJECT_HEADER,Body)
typedef struct _OBJECT_TYPE_INITIALIZER
{
	USHORT Length;
	union
	{
		UINT8 ObjectTypeFlags;
		struct
		{
			UINT8 CaseInsensitive : 1;
			UINT8 UnnamedObjectsOnly : 1;
			UINT8 UseDefaultObject : 1;
			UINT8 SecurityRequired : 1;
			UINT8 MaintainHandleCount : 1;
			UINT8 MaintainTypeList : 1;
			UINT8 SupportsObjectCallbacks : 1;
		};
	};
	ULONG32 ObjectTypeCode;
	ULONG32 InvalidAttributes;
	GENERIC_MAPPING GenericMapping;
	ULONG ValidAccessMask;
	ULONG RetainAccess;
	enum POOL_TYPE PoolType;
	ULONG DefaultPagedPoolCharge;
	ULONG DefaultNonPagedPoolCharge;
	PVOID DumpProcedure;
	PVOID OpenProcedure;
	PVOID CloseProcedure;
	PVOID DeleteProcedure;
	PVOID ParseProcedure;
	PVOID SecurityProcedure;
	PVOID QueryNameProcedure;
	PVOID OkayToCloseProcedure;
} OBJECT_TYPE_INITIALIZER, * POBJECT_TYPE_INITIALIZER;

#ifdef _WIN64
typedef struct _OBJECT_TYPE_1
{
	struct _LIST_ENTRY TypeList;
	struct _UNICODE_STRING Name;
	VOID* DefaultObject;
	UINT8 Index;
	UINT8 _PADDINGO_[0x3];
	ULONG32 TotalNumberOfObjects;
	ULONG32 TotalNumberOfHandles;
	ULONG32 HighWaterNumberOfObjects;
	ULONG32 HighWaterNumberOfHandles;
	UINT8 _PADDING1_[0x4];
	struct _OBJECT_TYPE_INITIALIZER TypeInfo;
	ULONG64 TypeLock;
	ULONG32 Key;
	UINT8 _PADDING2_[0x4];
	struct _LIST_ENTRY CallbackList;
}OBJECT_TYPE_1,*POBJECT_TYPE_1;
#else
typedef struct _OBJECT_TYPE_1
{
	struct _LIST_ENTRY TypeList;
	struct _UNICODE_STRING Name;
	VOID* DefaultObject;
	UINT8 Index;
	ULONG32 TotalNumberOfObjects;
	ULONG32 TotalNumberOfHandles;
	ULONG32 HighWaterNumberOfObjects;
	ULONG32 HighWaterNumberOfHandles;
	struct _OBJECT_TYPE_INITIALIZER TypeInfo;
	ULONG64 TypeLock;
	ULONG32 Key;
	UINT8 _PADDING2_[0x4];
	struct _LIST_ENTRY CallbackList;
}OBJECT_TYPE_1, * POBJECT_TYPE_1;
#endif
//这个结构是在windbg中查到的，跟sourceinsight有点不一样
typedef struct _OBJECT_HEADER
{
	ULONG_PTR PointerCount;
	union
	{
		ULONG_PTR HandleCount;
		volatile PVOID NextToFree;
	};
	ULONG_PTR Lock;
	UCHAR TypeIndex;
	UCHAR TraceFlags;
	UCHAR InfoMask;
	UCHAR Flags[5];
	union
	{
		PVOID ObjectCreateInfo;
		PVOID QuotaBlockCharged;
	};
	PSECURITY_DESCRIPTOR SecurityDescriptor;
	QUAD Body;
} OBJECT_HEADER, * POBJECT_HEADER;

typedef ULONG_PTR(*LPFN_OBGETOBJECTTYPE)(PVOID ObjectBody);
extern LPFN_OBGETOBJECTTYPE __ObGetObjectType;
VOID InitializeObjectSource();