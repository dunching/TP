// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelMemoryScope.h"

#if VOXEL_ALLOC_DEBUG
extern bool GVoxelMallocDisableChecks;
extern thread_local bool GVoxelMallocIsAllowed;
extern thread_local bool GVoxelReallocIsAllowed;
#endif

DECLARE_VOXEL_MEMORY_STAT(VOXELCORE_API, STAT_VoxelMemoryWaste, "Memory Allocator Waste");
DEFINE_VOXEL_MEMORY_STAT(STAT_VoxelMemoryWaste);

DECLARE_VOXEL_MEMORY_STAT(VOXELCORE_API, STAT_VoxelMemoryTotal, "Total Memory Allocated");
DEFINE_VOXEL_MEMORY_STAT(STAT_VoxelMemoryTotal);

#if VOXEL_DEBUG
VOXEL_RUN_ON_STARTUP_GAME(TestVoxelMemory)
{
	for (int32 AlignmentLog2 = 0; AlignmentLog2 < 16; AlignmentLog2++)
	{
		const int32 Alignment = 1 << AlignmentLog2;
		void* Ptr = FVoxelMemory::Malloc(18, Alignment);
		check(IsAligned(Ptr, Alignment));
		FVoxelMemory::Free(Ptr);
	}
}
#endif

FVoxelMemoryScope::FVoxelMemoryScope()
{
	VOXEL_FUNCTION_COUNTER();

	check(!FPlatformTLS::GetTlsValue(GVoxelMemoryTLS));
	FPlatformTLS::SetTlsValue(GVoxelMemoryTLS, this);

#if VOXEL_ALLOC_DEBUG
	check(GVoxelMallocIsAllowed);
	GVoxelMallocIsAllowed = false;
#endif
}

FVoxelMemoryScope::~FVoxelMemoryScope()
{
	VOXEL_FUNCTION_COUNTER();
	check(ThreadId == FPlatformTLS::GetCurrentThreadId());

	Clear();

	check(FPlatformTLS::GetTlsValue(GVoxelMemoryTLS) == this);
	FPlatformTLS::SetTlsValue(GVoxelMemoryTLS, nullptr);

#if VOXEL_ALLOC_DEBUG
	check(!GVoxelMallocIsAllowed);
	GVoxelMallocIsAllowed = true;
#endif
}

void FVoxelMemoryScope::Clear()
{
	VOXEL_ALLOW_MALLOC_SCOPE();
	check(ThreadId == FPlatformTLS::GetCurrentThreadId());

	int64 NumAllocations = 0;
	int64 AllocatedSize = 0;
	for (const auto& It : AlignmentToPools)
	{
		for (const FPool& Pool : It)
		{
			NumAllocations += Pool.Allocations.Num();

			if (AreVoxelStatsEnabled())
			{
				for (void* Allocation : Pool.Allocations)
				{
					const uint64 Size = StaticGetAllocSize(Allocation);
					if (!ensure(Size != 0))
					{
						continue;
					}

					AllocatedSize += Size;
				}
			}
		}
	}

	if (NumAllocations == 0)
	{
		return;
	}

	VOXEL_SCOPE_COUNTER_FORMAT("FMemory::Free %fMB %lld allocations", AllocatedSize / double(1 << 20), NumAllocations);

	for (auto& It : AlignmentToPools)
	{
		for (FPool& Pool : It)
		{
			for (void* Allocation : Pool.Allocations)
			{
				StaticFree(Allocation);
			}
			Pool.Allocations.Reset();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if VOXEL_DEBUG
FVoxelFastCriticalSection GVoxelValidAllocationsCriticalSection;
TSet<void*> GVoxelValidAllocations;
#endif

FORCEINLINE FVoxelMemoryScope::FBlock& FVoxelMemoryScope::GetBlock(void* Original)
{
#if VOXEL_DEBUG
	if (GIsVoxelCoreModuleLoaded)
	{
		VOXEL_SCOPE_LOCK(GVoxelValidAllocationsCriticalSection);
		check(GVoxelValidAllocations.Contains(Original));
	}
#endif

	return *reinterpret_cast<FBlock*>(static_cast<uint8*>(Original) - sizeof(FBlock));
}

uint64 FVoxelMemoryScope::StaticGetAllocSize(void* Original)
{
	const FBlock& Block = GetBlock(Original);
	checkVoxelSlow(IsAligned(Original, Block.Alignment));
	return Block.Size;
}

void* FVoxelMemoryScope::StaticMalloc(const uint64 Count, uint32 Alignment)
{
	VOXEL_SCOPE_COUNTER_FORMAT_COND(Count > 1024, "StaticMalloc %lldB", Count);

	if (Alignment < 16)
	{
		Alignment = 16;
	}
	checkStatic(sizeof(FBlock) == 16);

	const int32 PoolIndex = GetPoolIndex(Count);
	const uint64 AllocationSize = PoolIndex == -1 ? Count : GetPoolSize(PoolIndex);

	const int32 Padding = sizeof(FBlock) + Alignment;

	// NEVER pass custom alignment to Malloc, as anything other than 16B will
	// force BinnedMalloc2 to allocate 4096B
	void* UnalignedPtr = FMemory::Malloc(Padding + AllocationSize);
	INC_VOXEL_MEMORY_STAT_BY(STAT_VoxelMemoryWaste, Padding);
	INC_VOXEL_MEMORY_STAT_BY(STAT_VoxelMemoryTotal, Padding + AllocationSize);

#if VOXEL_DEBUG
	FMemory::Memset(UnalignedPtr, 0xDE, Padding + AllocationSize);
#endif

	void* Result = static_cast<uint8*>(UnalignedPtr) + sizeof(FBlock);
	Result = Align(Result, Alignment);
	check(static_cast<uint8*>(Result) - static_cast<uint8*>(UnalignedPtr) <= Padding);

#if VOXEL_DEBUG
	{
		VOXEL_SCOPE_LOCK(GVoxelValidAllocationsCriticalSection);
		check(!GVoxelValidAllocations.Contains(Result));
		GVoxelValidAllocations.Add(Result);
	}
#endif

	FBlock& Block = GetBlock(Result);
	Block.Size = AllocationSize;
	Block.Alignment = Alignment;
	Block.UnalignedPtr = UnalignedPtr;

	checkVoxelSlow(IsAligned(Result, Alignment));
	return Result;
}

void* FVoxelMemoryScope::StaticRealloc(void* Original, const uint64 OriginalCount, const uint64 Count, const uint32 Alignment)
{
	if (Count == 0)
	{
		StaticFree(Original);
		return nullptr;
	}

	if (!Original)
	{
		return StaticMalloc(Count, Alignment);
	}

	const uint64 CountToCopy = FMath::Min(Count, OriginalCount);
	void* NewPtr = StaticMalloc(Count, Alignment);
	{
		VOXEL_SCOPE_COUNTER_COND(CountToCopy > 8192, "FMemory::Memcpy");
		FMemory::Memcpy(NewPtr, Original, CountToCopy);
	}
	StaticFree(Original);

	return NewPtr;
}

void FVoxelMemoryScope::StaticFree(void* Original)
{
	const FBlock& Block = GetBlock(Original);
	checkVoxelSlow(IsAligned(Original, Block.Alignment));
	const int32 Padding = sizeof(FBlock) + Block.Alignment;
	const uint64 AllocationSize = Block.Size;

#if VOXEL_DEBUG
	if (GIsVoxelCoreModuleLoaded)
	{
		VOXEL_SCOPE_LOCK(GVoxelValidAllocationsCriticalSection);
		verify(GVoxelValidAllocations.Remove(Original));
	}
#endif

	FMemory::Free(Block.UnalignedPtr);

	DEC_VOXEL_MEMORY_STAT_BY(STAT_VoxelMemoryWaste, Padding);
	DEC_VOXEL_MEMORY_STAT_BY(STAT_VoxelMemoryTotal, Padding + AllocationSize);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void* FVoxelMemoryScope::Malloc(const uint64 Count, const uint32 Alignment)
{
	if (!ensureVoxelSlow(Count > 0))
	{
		return nullptr;
	}

	const int32 PoolIndex = GetPoolIndex(Count);
	if (PoolIndex == -1 ||
		!ensureVoxelSlow(Alignment <= 128))
	{
		VOXEL_SCOPE_COUNTER_FORMAT("FMemory::Malloc (too large) %fMB", Count / double(1 << 20));
		return StaticMalloc(Count, Alignment);
	}

	const uint64 PoolSize = GetPoolSize(PoolIndex);
	checkVoxelSlow(PoolSize >= Count);

	for (int32 AlignmentIndex = GetAlignmentIndex(Alignment); AlignmentIndex < MaxAlignmentIndex; AlignmentIndex++)
	{
		FPool& Pool = AlignmentToPools[AlignmentIndex][PoolIndex];
		if (Pool.Allocations.Num() > 0)
		{
			void* Result = Pool.Allocations.Pop(false);
#if VOXEL_DEBUG
			FMemory::Memset(Result, 0xDE, PoolSize);
#endif
			return Result;
		}
	}

	return StaticMalloc(PoolSize, Alignment);
}

void* FVoxelMemoryScope::Realloc(void* Original, const uint64 OriginalCount, const uint64 Count, const uint32 Alignment)
{
	if (Count == 0)
	{
		Free(Original);
		return nullptr;
	}

	if (!Original)
	{
		return Malloc(Count, Alignment);
	}

	const uint64 CountToCopy = FMath::Min(Count, OriginalCount);

#if VOXEL_ALLOC_DEBUG
	if (!GVoxelMallocDisableChecks &&
		!GVoxelReallocIsAllowed &&
		CountToCopy > 64)
	{
		UE_DEBUG_BREAK();
	}
#endif

	void* NewPtr = Malloc(Count, Alignment);
	{
		VOXEL_SCOPE_COUNTER_COND(CountToCopy > 8192, "FMemory::Memcpy");
		FMemory::Memcpy(NewPtr, Original, CountToCopy);
	}
	Free(Original);

	return NewPtr;
}

void FVoxelMemoryScope::Free(void* Original)
{
	if (!Original)
	{
		return;
	}

	const FBlock& Block = GetBlock(Original);
	checkVoxelSlow(IsAligned(Original, Block.Alignment));

#if VOXEL_DEBUG
	FMemory::Memset(Original, 0xFE, Block.Size);
#endif

	const int32 PoolIndex = GetPoolIndex(Block.Size);
	if (PoolIndex == -1)
	{
		VOXEL_SCOPE_COUNTER("FMemory::Free");
		StaticFree(Original);
		return;
	}
	const uint64 PoolSize = GetPoolSize(PoolIndex);

#if VOXEL_DEBUG
	FMemory::Memset(Original, 0xDE, PoolSize);
#endif

	FPool& Pool = AlignmentToPools[GetAlignmentIndex(Block.Alignment)][PoolIndex];
	if (Pool.Allocations.Num() == 0)
	{
		Pool.Allocations.Reserve(1024);
	}
	Pool.Allocations.Add(Original);
}