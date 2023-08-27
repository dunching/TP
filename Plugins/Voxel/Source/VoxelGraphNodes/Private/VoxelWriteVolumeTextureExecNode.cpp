// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelWriteVolumeTextureExecNode.h"
#include "VoxelBufferUtilities.h"
#include "VoxelPositionQueryParameter.h"
#include "TextureResource.h"

TVoxelUniquePtr<FVoxelExecNodeRuntime> FVoxelWriteVolumeTextureExecNode::CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const
{
	return MakeVoxelUnique<FVoxelWriteVolumeTextureExecNodeRuntime>(SharedThis);
}

void FVoxelWriteVolumeTextureExecNodeRuntime::Create()
{
	VOXEL_FUNCTION_COUNTER();

	PinValuesProvider.Compute(this, [this](const FPinValues& PinValues)
	{
		Update(PinValues);
	});
}

void FVoxelWriteVolumeTextureExecNodeRuntime::Destroy()
{
	DistanceValue = {};
}

void FVoxelWriteVolumeTextureExecNodeRuntime::Update(const FPinValues& PinValues)
{
	VOXEL_FUNCTION_COUNTER();
	ensure(IsInGameThread());

	if (int64(PinValues.Size.X) *
		int64(PinValues.Size.Y) *
		int64(PinValues.Size.Z) > 1024 * 1024 * 1024)
	{
		ensure(false);
		return;
	}
	if (PinValues.Size.X <= 0 ||
		PinValues.Size.Y <= 0 ||
		PinValues.Size.Z <= 0)
	{
		ensure(false);
		return;
	}

	const TSharedRef<FVoxelQueryParameters> Parameters = MakeVoxelShared<FVoxelQueryParameters>();
	Parameters->Add<FVoxelGradientStepQueryParameter>().Step = PinValues.VoxelSize;
	Parameters->Add<FVoxelPositionQueryParameter>().InitializeGrid3D(FVector3f(PinValues.Start), PinValues.VoxelSize, PinValues.Size);

	DistanceValue = GetNodeRuntime().MakeDynamicValueFactory(Node.DistancePin).Compute(GetContext(), Parameters);
	DistanceValue.OnChanged_GameThread([WeakTexture = PinValues.Texture.Texture, Size = PinValues.Size](const FVoxelFloatBuffer& Distance)
	{
		UVolumeTexture* Texture = WeakTexture.Get();
		if (!Texture)
		{
			return;
		}

#if WITH_EDITOR
		const FTextureSource Source;
		Texture->Source = Source;
#endif

		FTexturePlatformData* PlatformData = new FTexturePlatformData();
		PlatformData->SizeX = Size.X;
		PlatformData->SizeY = Size.Y;
		PlatformData->SetNumSlices(Size.Z);
		PlatformData->PixelFormat = PF_R32_FLOAT;

		FTexture2DMipMap* Mip = new FTexture2DMipMap();
		Mip->SizeX = Size.X;
		Mip->SizeY = Size.Y;
		Mip->SizeZ = Size.Z;
		Mip->BulkData.Lock(LOCK_READ_WRITE);
		{
			void* Data = Mip->BulkData.Realloc(sizeof(float) * Size.X * Size.Y * Size.Z);
			const TVoxelArrayView<float> OutData(static_cast<float*>(Data), Size.X * Size.Y * Size.Z);
			FVoxelBufferUtilities::UnpackData(Distance.GetStorage(), OutData, Size);
		}
		Mip->BulkData.Unlock();
		PlatformData->Mips.Add(Mip);

		if (const FTexturePlatformData* ExistingPlatformData = Texture->GetPlatformData())
		{
			delete ExistingPlatformData;
			Texture->SetPlatformData(nullptr);
		}

		check(!Texture->GetPlatformData());
		Texture->SetPlatformData(PlatformData);
		Texture->UpdateResource();
	});
}