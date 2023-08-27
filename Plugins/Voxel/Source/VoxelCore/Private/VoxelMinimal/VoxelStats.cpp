// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelMinimal.h"
#include "MemPro/MemProProfiler.h"
#include "Runtime/Core/Private/HAL/LowLevelMemTrackerPrivate.h"

UE_TRACE_CHANNEL_DEFINE(VoxelChannel);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if !IS_MONOLITHIC
namespace UE
{
	namespace LLMPrivate
	{
		ELLMTag FTagData::GetEnumTag() const
		{
			return EnumTag;
		}
	}
}
#endif

bool GVoxelLLMRegistered = false;

void Voxel_RegisterLLM()
{
	check(!GVoxelLLMRegistered);
	GVoxelLLMRegistered = true;

	LLM(FLowLevelMemTracker::Get().RegisterProjectTag(int32(VOXEL_LLM_TAG), TEXT("Voxel"), GET_STATFNAME(STAT_VoxelLLM), NAME_None));

#if MEMPRO_ENABLED && ENABLE_LOW_LEVEL_MEM_TRACKER
	// Ensure tags are correctly tracked, in case the voxel tag was not registered when TrackTagsByName was called
	// To use MemPro: -LLM -MemPro -MemProTags="Voxel", and open Saved/Profiling/MemPro/XXX.mempro_dump
	FString LLMTagsStr;
	if (FParse::Value(FCommandLine::Get(), TEXT("MemProTags="), LLMTagsStr))
	{
		FMemProProfiler::TrackTagsByName(*LLMTagsStr);
	}
#endif
}

void Voxel_CheckLLMScopeImpl()
{
#if ENABLE_LOW_LEVEL_MEM_TRACKER
	if (FLowLevelMemTracker::bIsDisabled ||
		IsEngineExitRequested())
	{
		return;
	}

	const UE::LLMPrivate::FTagData* TagData = FLowLevelMemTracker::Get().GetActiveTagData(ELLMTracker::Default);
	if (!ensure(TagData))
	{
		return;
	}

	if (TagData->GetEnumTag() == VOXEL_LLM_TAG)
	{
		return;
	}

	ensure(false);
#endif
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString VoxelStats_CleanupFunctionName(const FString& FunctionName)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_VoxelStats_CleanupFunctionName);

#if PLATFORM_WINDOWS
	TArray<FString> Array;
	FunctionName.ParseIntoArray(Array, TEXT("::"));

	if (Array.Num() > 0 && Array[0] == "Voxel")
	{
		// Remove the Voxel:: prefix in namespace functions
		Array.RemoveAt(0);
	}

	// Remove lambdas
	for (int32 Index = 0; Index < Array.Num(); Index++)
	{
		if (Array[Index].StartsWith(TEXT("<lambda")) &&
			ensure(Index + 1 < Array.Num()) &&
			ensure(Array[Index + 1] == TEXT("operator ()") || Array[Index + 1] == TEXT("()")))
		{
			Array.RemoveAt(Index, 2);
			Index--;
		}
	}

	const FString FunctionNameWithoutLambda = FString::Join(Array, TEXT("::"));

	FString CleanFunctionName;
	if (FunctionNameWithoutLambda.EndsWith("operator <<"))
	{
		CleanFunctionName = FunctionNameWithoutLambda;
	}
	else
	{
		// Tricky case: TVoxelClusteredWriterBase<struct `public: static void __cdecl FVoxelSurfaceEditToolsImpl::EditVoxelValues()'::`2'::FStorage>
		int32 TemplateDepth = 0;
		for (const TCHAR& Char : FunctionNameWithoutLambda)
		{
			if (Char == TEXT('<'))
			{
				TemplateDepth++;
				continue;
			}
			if (Char == TEXT('>'))
			{
				TemplateDepth--;
				ensure(TemplateDepth >= 0);
				continue;
			}

			if (TemplateDepth == 0)
			{
				CleanFunctionName.AppendChar(Char);
			}
		}
		ensure(TemplateDepth == 0);
	}

	return CleanFunctionName;
#else
	return FunctionName;
#endif
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if STATS
FName Voxel_GetDynamicStatName(const FName Name)
{
	static FVoxelFastCriticalSection CriticalSection;
	static TMap<FName, FName> Map;

	VOXEL_SCOPE_LOCK(CriticalSection);

	if (const FName* StatName = Map.Find(Name))
	{
		return *StatName;
	}

	using Group = FStatGroup_STATGROUP_VoxelMemory;

	FStartupMessages::Get().AddMetadata(
		Name,
		*Name.ToString(),
		Group::GetGroupName(),
		Group::GetGroupCategory(),
		Group::GetDescription(),
		false,
		EStatDataType::ST_int64,
		false,
		Group::GetSortByName(),
		FPlatformMemory::MCR_Physical);

	const FName StatName =
		IStatGroupEnableManager::Get().GetHighPerformanceEnableForStat(
			Name,
			Group::GetGroupName(),
			Group::GetGroupCategory(),
			true,
			false,
			EStatDataType::ST_int64,
			*Name.ToString(),
			false,
			Group::GetSortByName(),
			FPlatformMemory::MCR_Physical).GetName();

	return Map.Add(Name, StatName);
}

void Voxel_AddAmountToDynamicStat(const FName Name, const int64 Amount)
{
	VOXEL_FUNCTION_COUNTER();

	if (Amount == 0)
	{
		return;
	}

	const FName StatName = Voxel_GetDynamicStatName(Name);

	if (Amount > 0)
	{
		FThreadStats::AddMessage(StatName, EStatOperation::Add, Amount);
	}
	else
	{
		FThreadStats::AddMessage(StatName, EStatOperation::Subtract, -Amount);
	}

	TRACE_STAT_ADD(Name, Amount);
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if STATS
TMap<FName, const FThreadSafeCounter64*> GVoxelStatNameToInstanceCounter;

void RegisterVoxelInstanceCounter(const FName StatName, const FThreadSafeCounter64& Counter)
{
	check(IsInGameThread());
	check(!GVoxelStatNameToInstanceCounter.Contains(StatName));
	GVoxelStatNameToInstanceCounter.Add(StatName, &Counter);
}

class FVoxelInstanceCounterTicker : public FVoxelTicker
{
public:
	//~ Begin FVoxelTicker Interface
	virtual void Tick() override
	{
		VOXEL_FUNCTION_COUNTER();

		for (const auto& It : GVoxelStatNameToInstanceCounter)
		{
			FThreadStats::AddMessage(It.Key, EStatOperation::Set, It.Value->GetValue());
		}
	}
	//~ End FVoxelTicker Interface
};

VOXEL_RUN_ON_STARTUP_GAME(RegisterVoxelInstanceCounters)
{
	new FVoxelInstanceCounterTicker();

	GOnVoxelModuleUnloaded.AddLambda([]
	{
		for (const auto& It : GVoxelStatNameToInstanceCounter)
		{
			checkfVoxelSlow(It.Value->GetValue() == 0, TEXT("Leaking %lld instances of class %s"), It.Value->GetValue(), *It.Key.ToString());
		}
	});
}
#endif