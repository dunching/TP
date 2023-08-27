// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Stats/StatsMisc.h"
#include "HAL/LowLevelMemStats.h"
#include "VoxelMacros.h"

UE_TRACE_CHANNEL_EXTERN(VoxelChannel, VOXELCORE_API);

DECLARE_STATS_GROUP(TEXT("Voxel"), STATGROUP_Voxel, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Voxel Counters"), STATGROUP_VoxelCounters, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Voxel Memory"), STATGROUP_VoxelMemory, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Voxel GPU Memory"), STATGROUP_VoxelGpuMemory, STATCAT_Advanced);

#if ENABLE_LOW_LEVEL_MEM_TRACKER
DECLARE_LLM_MEMORY_STAT(TEXT("Voxel"), STAT_VoxelLLM, STATGROUP_LLMFULL);
#endif

#define VOXEL_LLM_TAG ELLMTag(int32(ELLMTag::ProjectTagEnd) - 1)
#define VOXEL_LLM_SCOPE() \
	LLM(if (!GVoxelLLMRegistered) { Voxel_RegisterLLM(); }) \
	LLM_SCOPE(VOXEL_LLM_TAG)

extern VOXELCORE_API bool GVoxelLLMRegistered;
VOXELCORE_API void Voxel_RegisterLLM();
VOXELCORE_API void Voxel_CheckLLMScopeImpl();

FORCEINLINE void Voxel_CheckLLMScope()
{
#if ENABLE_LOW_LEVEL_MEM_TRACKER
	if (!FLowLevelMemTracker::bIsDisabled)
	{
		Voxel_CheckLLMScopeImpl();
	}
#endif
}

#if CPUPROFILERTRACE_ENABLED
FORCEINLINE bool AreVoxelStatsEnabled()
{
	return VoxelChannel.IsEnabled();
}

#define VOXEL_EVENT_SPEC_ID VOXEL_APPEND_LINE(__CpuProfilerEventSpecId)
#define VOXEL_TRACE_ENABLED VOXEL_APPEND_LINE(__bTraceEnabled)

#define VOXEL_SCOPE_COUNTER_IMPL(Condition, Description) \
	const bool VOXEL_TRACE_ENABLED = AreVoxelStatsEnabled() && (Condition); \
	if (VOXEL_TRACE_ENABLED) \
	{ \
		VOXEL_ALLOW_MALLOC_SCOPE(); \
		static const FString StaticDescription = Description; \
		static const uint32 VOXEL_EVENT_SPEC_ID = FCpuProfilerTrace::OutputEventType(*StaticDescription, __FILE__, __LINE__); \
		FCpuProfilerTrace::OutputBeginEvent(VOXEL_EVENT_SPEC_ID); \
	} \
	ON_SCOPE_EXIT \
	{ \
		if (VOXEL_TRACE_ENABLED) \
		{ \
			VOXEL_ALLOW_MALLOC_SCOPE(); \
			FCpuProfilerTrace::OutputEndEvent(); \
		} \
	}; \
	VOXEL_DEBUG_ONLY(Voxel_CheckLLMScope());

#define VOXEL_SCOPE_COUNTER_FNAME_COND(Condition, Description) \
	const bool VOXEL_TRACE_ENABLED = AreVoxelStatsEnabled() && (Condition); \
	if (VOXEL_TRACE_ENABLED) \
	{ \
		VOXEL_ALLOW_MALLOC_SCOPE(); \
		FCpuProfilerTrace::OutputBeginDynamicEvent(Description, __FILE__, __LINE__); \
	} \
	ON_SCOPE_EXIT \
	{ \
		if (VOXEL_TRACE_ENABLED) \
		{ \
			VOXEL_ALLOW_MALLOC_SCOPE(); \
			FCpuProfilerTrace::OutputEndEvent(); \
		} \
	};

#else
FORCEINLINE bool AreVoxelStatsEnabled()
{
	return false;
}

#define VOXEL_SCOPE_COUNTER_IMPL(Condition, Description)
#define VOXEL_SCOPE_COUNTER_FNAME_COND(Condition, Description)
#endif

VOXELCORE_API FString VoxelStats_CleanupFunctionName(const FString& FunctionName);

#define VOXEL_STATS_CLEAN_FUNCTION_NAME VoxelStats_CleanupFunctionName(__FUNCTION__)

#define VOXEL_SCOPE_COUNTER_COND(Condition, Description) VOXEL_SCOPE_COUNTER_IMPL(Condition, VOXEL_STATS_CLEAN_FUNCTION_NAME + TEXT(".") + FString(Description))
#define VOXEL_SCOPE_COUNTER_FORMAT_COND(Condition, Format, ...) VOXEL_SCOPE_COUNTER_FNAME_COND(Condition, FName(FString::Printf(TEXT(Format), ##__VA_ARGS__)))
#define VOXEL_FUNCTION_COUNTER_COND(Condition) VOXEL_SCOPE_COUNTER_IMPL(Condition, VOXEL_STATS_CLEAN_FUNCTION_NAME)
#define VOXEL_INLINE_COUNTER_COND(Condition, Name, ...) ([&]() -> decltype(auto) { VOXEL_SCOPE_COUNTER_IMPL(Condition, VOXEL_STATS_CLEAN_FUNCTION_NAME + TEXT(".") + FString(Name)); return __VA_ARGS__; }())

#define VOXEL_SCOPE_COUNTER(Description) VOXEL_SCOPE_COUNTER_COND(true, Description)
#define VOXEL_SCOPE_COUNTER_FNAME(Description) VOXEL_SCOPE_COUNTER_FNAME_COND(true, Description)
#define VOXEL_SCOPE_COUNTER_FORMAT(Format, ...) VOXEL_SCOPE_COUNTER_FORMAT_COND(true, Format, ##__VA_ARGS__)
#define VOXEL_FUNCTION_COUNTER() VOXEL_FUNCTION_COUNTER_COND(true)
#define VOXEL_INLINE_COUNTER(Name, ...) VOXEL_INLINE_COUNTER_COND(true, Name, ##__VA_ARGS__)

#define VOXEL_SCOPE_COUNTER_NUM(Name, Num, Threshold) VOXEL_SCOPE_COUNTER_FORMAT_COND(Num > Threshold, "%s Num=%d", *STATIC_FSTRING(Name), Num)
#define VOXEL_FUNCTION_COUNTER_NUM(Num, Threshold) VOXEL_SCOPE_COUNTER_NUM(VOXEL_STATS_CLEAN_FUNCTION_NAME, Num, Threshold)

#define VOXEL_FUNCTION_COUNTER_LLM() VOXEL_LLM_SCOPE() VOXEL_FUNCTION_COUNTER()
#define VOXEL_LOG_FUNCTION_STATS() FScopeLogTime PREPROCESSOR_JOIN(FScopeLogTime_, __LINE__)(*STATIC_FSTRING(VOXEL_STATS_CLEAN_FUNCTION_NAME));
#define VOXEL_LOG_SCOPE_STATS(Name) FScopeLogTime PREPROCESSOR_JOIN(FScopeLogTime_, __LINE__)(*STATIC_FSTRING(VOXEL_STATS_CLEAN_FUNCTION_NAME + "." + FString(Name)));
#define VOXEL_TRACE_BOOKMARK() TRACE_BOOKMARK(*STATIC_FSTRING(VOXEL_STATS_CLEAN_FUNCTION_NAME));

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define DECLARE_VOXEL_COUNTER(API, StatName, Name) DECLARE_VOXEL_COUNTER_WITH_CATEGORY(API, STATGROUP_VoxelCounters, StatName, Name)

#define DECLARE_VOXEL_COUNTER_WITH_CATEGORY(API, Category, StatName, Name) \
	VOXEL_DEBUG_ONLY(extern API FThreadSafeCounter64 StatName;) \
	DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT(Name), StatName ## _Stat, Category, API)


#define DECLARE_VOXEL_FRAME_COUNTER(API, StatName, Name) DECLARE_VOXEL_FRAME_COUNTER_WITH_CATEGORY(API, STATGROUP_VoxelCounters, StatName, Name)

#define DECLARE_VOXEL_FRAME_COUNTER_WITH_CATEGORY(API, Category, StatName, Name) \
	VOXEL_DEBUG_ONLY(extern API FThreadSafeCounter64 StatName;) \
	DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT(Name), StatName ## _Stat, Category, API)


#define DEFINE_VOXEL_COUNTER(StatName) \
	VOXEL_DEBUG_ONLY(FThreadSafeCounter64 StatName;) \
	DEFINE_STAT(StatName ## _Stat)

#if STATS
#define INC_VOXEL_COUNTER_BY(StatName, Amount) \
	VOXEL_DEBUG_ONLY(StatName.Add(Amount);) \
	VOXEL_ALLOW_MALLOC_INLINE(FThreadStats::AddMessage(GET_STATFNAME(StatName ## _Stat), EStatOperation::Add, int64(Amount)));

#define DEC_VOXEL_COUNTER_BY(StatName, Amount) \
	VOXEL_DEBUG_ONLY(StatName.Subtract(Amount);) \
	VOXEL_DEBUG_ONLY(ensure(StatName.GetValue() >= 0);) \
	VOXEL_ALLOW_MALLOC_INLINE(FThreadStats::AddMessage(GET_STATFNAME(StatName ## _Stat), EStatOperation::Subtract, int64(Amount)));
#else
#define INC_VOXEL_COUNTER_BY(StatName, Amount)
#define DEC_VOXEL_COUNTER_BY(StatName, Amount)
#endif

#define DEC_VOXEL_COUNTER(StatName) DEC_VOXEL_COUNTER_BY(StatName, 1)
#define INC_VOXEL_COUNTER(StatName) INC_VOXEL_COUNTER_BY(StatName, 1)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define DECLARE_VOXEL_MEMORY_STAT(API, StatName, Name) DECLARE_VOXEL_MEMORY_STAT_WITH_CATEGORY(API, STATGROUP_VoxelMemory, StatName, Name)
#define DECLARE_VOXEL_GPU_MEMORY_STAT(API, StatName, Name) DECLARE_VOXEL_MEMORY_STAT_WITH_CATEGORY(API, STATGROUP_VoxelGpuMemory, StatName, Name)

#define DECLARE_VOXEL_MEMORY_STAT_WITH_CATEGORY(API, Category, StatName, Name) \
	VOXEL_DEBUG_ONLY(extern API FThreadSafeCounter64 StatName;) \
	DECLARE_MEMORY_STAT_EXTERN(TEXT(Name), StatName ## _Stat, Category, API)

#define DEFINE_VOXEL_MEMORY_STAT(StatName) \
	VOXEL_DEBUG_ONLY(FThreadSafeCounter64 StatName;) \
	DEFINE_STAT(StatName ## _Stat)

#define INC_VOXEL_MEMORY_STAT_BY(StatName, Amount) INC_VOXEL_COUNTER_BY(StatName, Amount)
#define DEC_VOXEL_MEMORY_STAT_BY(StatName, Amount) DEC_VOXEL_COUNTER_BY(StatName, Amount)

#if STATS
#define GET_VOXEL_MEMORY_STAT(Name) (INTELLISENSE_ONLY((void)&Name,) StatPtr_ ## Name ## _Stat)
#else
#define GET_VOXEL_MEMORY_STAT(Name)
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define FVoxelStatsRefHelper VOXEL_APPEND_LINE(__FVoxelStatsRefHelper)

#if STATS
#define VOXEL_ALLOCATED_SIZE_TRACKER_IMPL(StatName, GetAllocatedSize, UpdateStats, EnsureStatsAreUpToDate) \
	struct FVoxelStatsRefHelper \
	{ \
		int64 AllocatedSize = 0; \
		\
		FVoxelStatsRefHelper() = default; \
		FVoxelStatsRefHelper(FVoxelStatsRefHelper&& Other) \
		{ \
			AllocatedSize = Other.AllocatedSize; \
			Other.AllocatedSize = 0; \
		} \
		FVoxelStatsRefHelper& operator=(FVoxelStatsRefHelper&& Other) \
		{ \
			AllocatedSize = Other.AllocatedSize; \
			Other.AllocatedSize = 0; \
			return *this; \
		} \
		FVoxelStatsRefHelper(const FVoxelStatsRefHelper& Other) \
		{ \
			AllocatedSize = Other.AllocatedSize; \
			INC_VOXEL_MEMORY_STAT_BY(StatName, AllocatedSize); \
		} \
		FVoxelStatsRefHelper& operator=(const FVoxelStatsRefHelper& Other) \
		{ \
			AllocatedSize = Other.AllocatedSize; \
			INC_VOXEL_MEMORY_STAT_BY(StatName, AllocatedSize); \
			return *this; \
		} \
		FORCEINLINE ~FVoxelStatsRefHelper() \
		{ \
			DEC_VOXEL_MEMORY_STAT_BY(StatName, AllocatedSize); \
		} \
	}; \
	mutable FVoxelStatsRefHelper VOXEL_APPEND_LINE(__VoxelStatsRefHelper); \
	void UpdateStats() const \
	{ \
		int64& AllocatedSize = VOXEL_APPEND_LINE(__VoxelStatsRefHelper).AllocatedSize; \
		DEC_VOXEL_MEMORY_STAT_BY(StatName, AllocatedSize); \
		AllocatedSize = GetAllocatedSize(); \
		INC_VOXEL_MEMORY_STAT_BY(StatName, AllocatedSize); \
	} \
	void EnsureStatsAreUpToDate() const \
	{ \
		ensure(VOXEL_APPEND_LINE(__VoxelStatsRefHelper).AllocatedSize == GetAllocatedSize()); \
	}

#define VOXEL_TYPE_SIZE_TRACKER(Type, StatName) \
	struct FVoxelStatsRefHelper \
	{ \
		FVoxelStatsRefHelper() \
		{ \
			INC_VOXEL_MEMORY_STAT_BY(StatName, sizeof(Type)); \
		} \
		FORCEINLINE ~FVoxelStatsRefHelper() \
		{ \
			DEC_VOXEL_MEMORY_STAT_BY(StatName, sizeof(Type)); \
		} \
		FVoxelStatsRefHelper(FVoxelStatsRefHelper&&) \
			: FVoxelStatsRefHelper() \
		{ \
		} \
		FVoxelStatsRefHelper(const FVoxelStatsRefHelper&) \
			: FVoxelStatsRefHelper() \
		{ \
		} \
		FVoxelStatsRefHelper& operator=(FVoxelStatsRefHelper&&) \
		{ \
			return *this; \
		} \
		FVoxelStatsRefHelper& operator=(const FVoxelStatsRefHelper&) \
		{ \
			return *this; \
		} \
	}; \
	FVoxelStatsRefHelper VOXEL_APPEND_LINE(__VoxelStatsRefHelper);

#define VOXEL_ALLOCATED_SIZE_TRACKER_CUSTOM(StatName, Name) \
	class FVoxelStatsRefHelper \
	{ \
	public:\
		FVoxelStatsRefHelper() = default; \
		FVoxelStatsRefHelper(FVoxelStatsRefHelper&& Other) \
		{ \
			AllocatedSize = Other.AllocatedSize; \
			Other.AllocatedSize = 0; \
		} \
		FVoxelStatsRefHelper& operator=(FVoxelStatsRefHelper&& Other) \
		{ \
			AllocatedSize = Other.AllocatedSize; \
			Other.AllocatedSize = 0; \
			return *this; \
		} \
		FVoxelStatsRefHelper(const FVoxelStatsRefHelper& Other) \
		{ \
			AllocatedSize = Other.AllocatedSize; \
			INC_VOXEL_MEMORY_STAT_BY(StatName, AllocatedSize); \
		} \
		FVoxelStatsRefHelper& operator=(const FVoxelStatsRefHelper& Other) \
		{ \
			AllocatedSize = Other.AllocatedSize; \
			INC_VOXEL_MEMORY_STAT_BY(StatName, AllocatedSize); \
			return *this; \
		} \
		FORCEINLINE ~FVoxelStatsRefHelper() \
		{ \
			DEC_VOXEL_MEMORY_STAT_BY(StatName, AllocatedSize); \
		} \
		FVoxelStatsRefHelper& operator=(int64 InAllocatedSize) \
		{ \
			DEC_VOXEL_MEMORY_STAT_BY(StatName, AllocatedSize); \
			AllocatedSize = InAllocatedSize; \
			INC_VOXEL_MEMORY_STAT_BY(StatName, AllocatedSize); \
			return *this; \
		} \
	private: \
		int64 AllocatedSize = 0; \
	}; \
	mutable FVoxelStatsRefHelper Name;

#define VOXEL_COUNTER_HELPER(StatName, Name) \
	class FVoxelStatsRefHelper \
	{ \
	public:\
		FVoxelStatsRefHelper() = default; \
		FVoxelStatsRefHelper(FVoxelStatsRefHelper&& Other) \
		{ \
			Value = Other.Value; \
			Other.Value = 0; \
		} \
		FVoxelStatsRefHelper& operator=(FVoxelStatsRefHelper&& Other) \
		{ \
			Value = Other.Value; \
			Other.Value = 0; \
			return *this; \
		} \
		FVoxelStatsRefHelper(const FVoxelStatsRefHelper& Other) \
		{ \
			Value = Other.Value; \
			INC_VOXEL_COUNTER_BY(StatName, Value); \
		} \
		FVoxelStatsRefHelper& operator=(const FVoxelStatsRefHelper& Other) \
		{ \
			Value = Other.Value; \
			INC_VOXEL_COUNTER_BY(StatName, Value); \
			return *this; \
		} \
		FORCEINLINE ~FVoxelStatsRefHelper() \
		{ \
			DEC_VOXEL_COUNTER_BY(StatName, Value); \
		} \
		FVoxelStatsRefHelper& operator=(int64 NewValue) \
		{ \
			DEC_VOXEL_COUNTER_BY(StatName, Value); \
			Value = NewValue; \
			INC_VOXEL_COUNTER_BY(StatName, Value); \
			return *this; \
		} \
	private: \
		int64 Value = 0; \
	}; \
	mutable FVoxelStatsRefHelper Name;

VOXELCORE_API void Voxel_AddAmountToDynamicStat(FName Name, int64 Amount);

#else
#define VOXEL_ALLOCATED_SIZE_TRACKER_IMPL(StatName, GetAllocatedSize, UpdateStats, EnsureStatsAreUpToDate) \
	void UpdateStats() const \
	{ \
	} \
	void EnsureStatsAreUpToDate() const \
	{ \
	}

#define VOXEL_ALLOCATED_SIZE_TRACKER_CUSTOM(StatName, Name) \
	class FVoxelStatsRefHelper \
	{ \
	public:\
		FVoxelStatsRefHelper() = default; \
		void UpdateStats() \
		{ \
		} \
		FVoxelStatsRefHelper& operator+=(int64 InAllocatedSize) \
		{ \
			return *this; \
		} \
		FVoxelStatsRefHelper& operator=(int64 InAllocatedSize) \
		{ \
			return *this; \
		} \
	}; \
	mutable FVoxelStatsRefHelper Name;

#define VOXEL_COUNTER_HELPER(StatName, Name) \
	class FVoxelStatsRefHelper \
	{ \
	public:\
		FVoxelStatsRefHelper() = default; \
		FVoxelStatsRefHelper& operator=(int64 NewValue) \
		{ \
			return *this; \
		} \
	}; \
	mutable FVoxelStatsRefHelper Name;

#define VOXEL_TYPE_SIZE_TRACKER(Type, StatName)

FORCEINLINE void Voxel_AddAmountToDynamicStat(FName Name, int64 Amount)
{
}

#endif

#define VOXEL_ALLOCATED_SIZE_TRACKER(StatName) \
	VOXEL_ALLOCATED_SIZE_TRACKER_IMPL(StatName, GetAllocatedSize, UpdateStats, EnsureStatsAreUpToDate)

#define VOXEL_ALLOCATED_SIZE_GPU_TRACKER(StatName) \
	VOXEL_ALLOCATED_SIZE_TRACKER_IMPL(StatName, GetGpuAllocatedSize, UpdateGpuStats, EnsureGpuStatsAreUpToDate)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define VOXEL_TRACK_INSTANCES_SLOW(Struct) \
	struct FVoxelInstanceTrackerSlow \
	{ \
	public: \
		FVoxelInstanceTrackerSlow(Struct* This); \
		~FVoxelInstanceTrackerSlow(); \
		UE_NONCOPYABLE(FVoxelInstanceTrackerSlow); \
	\
	private: \
		int32 InstanceIndex = -1; \
	}; \
	const FVoxelInstanceTrackerSlow VOXEL_APPEND_LINE(InstanceTracker){ this }; \

#define DEFINE_VOXEL_INSTANCE_TRACKER_SLOW(Struct) \
	struct FVoxelInstanceTrackerSlowData ## Struct \
	{ \
		FVoxelFastCriticalSection CriticalSection; \
		TVoxelSparseArray<Struct*> InstanceIndexToThis; \
	}; \
	FVoxelInstanceTrackerSlowData ## Struct GVoxelInstanceTrackerData ## Struct; \
	\
	Struct::FVoxelInstanceTrackerSlow::FVoxelInstanceTrackerSlow(Struct* This) \
	{ \
		VOXEL_SCOPE_LOCK_NO_STATS(GVoxelInstanceTrackerData ## Struct.CriticalSection); \
		InstanceIndex = GVoxelInstanceTrackerData ## Struct.InstanceIndexToThis.Add(This); \
	} \
	Struct::FVoxelInstanceTrackerSlow::~FVoxelInstanceTrackerSlow() \
	{ \
		VOXEL_SCOPE_LOCK_NO_STATS(GVoxelInstanceTrackerData ## Struct.CriticalSection); \
		GVoxelInstanceTrackerData ## Struct.InstanceIndexToThis.RemoveAt(InstanceIndex); \
	} \
	\
	class FVoxelTicker_TrackInstancesSlow_ ## Struct : public FVoxelTicker { virtual void Tick() override {} }; \
	FVoxelTicker_TrackInstancesSlow_ ## Struct GVoxelTicker_TrackInstancesSlow_ ## Struct;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if STATS
#define VOXEL_COUNT_INSTANCES() \
	static FThreadSafeCounter64 VoxelInstanceCount; \
	struct FVoxelStatsRefHelper \
	{ \
		FORCEINLINE FVoxelStatsRefHelper() \
		{ \
			ensureVoxelSlow(VoxelInstanceCount.Increment() > 0); \
		} \
		FORCEINLINE ~FVoxelStatsRefHelper() \
		{ \
			ensureVoxelSlow(VoxelInstanceCount.Decrement() >= 0); \
		} \
		FORCEINLINE FVoxelStatsRefHelper(FVoxelStatsRefHelper&&) \
			: FVoxelStatsRefHelper() \
		{ \
		} \
		FORCEINLINE FVoxelStatsRefHelper(const FVoxelStatsRefHelper&) \
			: FVoxelStatsRefHelper() \
		{ \
		} \
		FORCEINLINE FVoxelStatsRefHelper& operator=(FVoxelStatsRefHelper&&) \
		{ \
			return *this; \
		} \
		FORCEINLINE FVoxelStatsRefHelper& operator=(const FVoxelStatsRefHelper&) \
		{ \
			return *this; \
		} \
	}; \
	FVoxelStatsRefHelper VOXEL_APPEND_LINE(__VoxelStatsRefHelper);

VOXELCORE_API void RegisterVoxelInstanceCounter(FName StatName, const FThreadSafeCounter64& Counter);

#define DEFINE_VOXEL_INSTANCE_COUNTER(Struct) \
	DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num " #Struct), STAT_Num ## Struct, STATGROUP_VoxelCounters); \
	FThreadSafeCounter64 Struct::VoxelInstanceCount; \
	\
	VOXEL_RUN_ON_STARTUP_GAME(RegisterInstanceCounter_ ## Struct) \
	{ \
		RegisterVoxelInstanceCounter(GET_STATFNAME(STAT_Num ## Struct), Struct::VoxelInstanceCount); \
	}
#else
#define VOXEL_COUNT_INSTANCES()
#define DEFINE_VOXEL_INSTANCE_COUNTER(Struct)
#endif