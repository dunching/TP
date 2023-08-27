// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelActor.h"
#include "VoxelRuntime.h"
#include "VoxelGraphInterface.h"
#include "VoxelParameterContainer.h"
#include "Point/VoxelPointStorage.h"
#include "Engine/Texture2D.h"
#include "Components/BillboardComponent.h"

AVoxelActor::AVoxelActor()
{
	RootComponent = CreateDefaultSubobject<UVoxelActorRootComponent>("Root");

	ParameterContainer = CreateDefaultSubobject<UVoxelParameterContainer>("ParameterContainerComponent");
	ParameterContainer->bAlwaysEnabled = true;
	ParameterContainer->OnProviderChanged.AddWeakLambda(this, [this]
	{
		if (IsRuntimeCreated())
		{
			QueueRecreate();
		}
	});

	PointStorageComponent = CreateDefaultSubobject<UVoxelPointStorageComponent>("VoxelPointStorageComponent");

	PrimaryActorTick.bCanEverTick = true;

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

#if WITH_EDITOR
	if (UBillboardComponent* SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite")))
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> SpriteFinder(TEXT("/Voxel/EditorAssets/T_VoxelIcon"));
		SpriteComponent->Sprite = SpriteFinder.Object;
		SpriteComponent->SetRelativeScale3D(FVector(0.5f));
		SpriteComponent->bHiddenInGame = true;
		SpriteComponent->bIsScreenSizeScaled = true;
		SpriteComponent->SpriteInfo.Category = TEXT("Voxel Actor");
		SpriteComponent->SpriteInfo.DisplayName = FText::FromString("Voxel Actor");
		SpriteComponent->SetupAttachment(RootComponent);
		SpriteComponent->bReceivesDecals = false;
	}
#endif
}

AVoxelActor::~AVoxelActor()
{
	ensure(!Runtime.IsValid());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AVoxelActor::BeginPlay()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	Super::BeginPlay();

	if (bCreateRuntimeOnBeginPlay &&
		!IsRuntimeCreated())
	{
		QueueCreateRuntime();
	}
}

void AVoxelActor::BeginDestroy()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	if (IsRuntimeCreated())
	{
		DestroyRuntime();
	}

	Super::BeginDestroy();
}

void AVoxelActor::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	VOXEL_FUNCTION_COUNTER_LLM();

	// In the editor, Destroyed is called but EndPlay isn't

	if (IsRuntimeCreated())
	{
		DestroyRuntime();
	}

	Super::EndPlay(EndPlayReason);
}

void AVoxelActor::Destroyed()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	if (IsRuntimeCreated())
	{
		DestroyRuntime();
	}

	Super::Destroyed();
}

void AVoxelActor::OnConstruction(const FTransform& Transform)
{
	VOXEL_FUNCTION_COUNTER_LLM();

	Super::OnConstruction(Transform);

#if WITH_EDITOR
	if (bCreateRuntimeOnConstruction_EditorOnly &&
		!IsRuntimeCreated() &&
		GetWorld() &&
		!GetWorld()->IsGameWorld() &&
		!HasAnyFlags(RF_ClassDefaultObject) &&
		!IsRunningCommandlet())
	{
		QueueCreateRuntime();
	}
#endif
}

void AVoxelActor::PostLoad()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	Super::PostLoad();

	if (!ensure(ParameterContainer))
	{
		ParameterContainer = NewObject<UVoxelParameterContainer>(this, "ParameterContainerComponent");
	}

	if (UVoxelGraphInterface* Graph = Graph_DEPRECATED.LoadSynchronous())
	{
		ensure(!ParameterContainer->Provider);
		ParameterContainer->Provider = Graph;
		Graph_DEPRECATED = {};

		ParameterCollection_DEPRECATED.MigrateTo(*ParameterContainer);
	}
}

void AVoxelActor::PostEditImport()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	Super::PostEditImport();

	if (IsRuntimeCreated())
	{
		QueueRecreate();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
bool AVoxelActor::Modify(const bool bAlwaysMarkDirty)
{
	if (bDisableModify)
	{
		return false;
	}

	return Super::Modify(bAlwaysMarkDirty);
}

void AVoxelActor::PostEditUndo()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	Super::PostEditUndo();

	if (IsValid(this))
	{
		if (!IsRuntimeCreated())
		{
			QueueCreateRuntime();
		}
	}
	else
	{
		if (IsRuntimeCreated())
		{
			DestroyRuntime();
		}
	}
}

void AVoxelActor::PreEditChange(FProperty* PropertyThatWillChange)
{
	// Temporarily remove runtime components to avoid expensive re-registration
	if (Runtime)
	{
		for (const TWeakObjectPtr<USceneComponent>& Component : Runtime->GetComponents())
		{
			ensure(Component.IsValid());
			ensure(ConstCast(GetComponents()).Remove(Component.Get()));
		}
	}

	Super::PreEditChange(PropertyThatWillChange);

	if (Runtime)
	{
		for (const TWeakObjectPtr<USceneComponent>& Component : Runtime->GetComponents())
		{
			ensure(Component.IsValid());
			ConstCast(GetComponents()).Add(Component.Get());
		}
	}
}

void AVoxelActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Temporarily remove runtime components to avoid expensive re-registration
	if (Runtime)
	{
		for (const TWeakObjectPtr<USceneComponent>& Component : Runtime->GetComponents())
		{
			ensure(Component.IsValid());
			ensure(ConstCast(GetComponents()).Remove(Component.Get()));
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (Runtime)
	{
		for (const TWeakObjectPtr<USceneComponent>& Component : Runtime->GetComponents())
		{
			ensure(Component.IsValid());
			ConstCast(GetComponents()).Add(Component.Get());
		}
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AVoxelActor::Tick(const float DeltaTime)
{
	VOXEL_FUNCTION_COUNTER_LLM();

	// We don't want to tick the BP in preview
	if (GetWorld()->IsGameWorld())
	{
		Super::Tick(DeltaTime);
	}

	if (bRuntimeCreateQueued &&
		FVoxelRuntime::CanBeCreated(false))
	{
		CreateRuntime();
	}

	if (bRuntimeRecreateQueued &&
		FVoxelRuntime::CanBeCreated(false))
	{
		bRuntimeRecreateQueued = false;

		DestroyRuntime();
		CreateRuntime();
	}

	if (Runtime)
	{
		Runtime->Tick();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AVoxelActor::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	const TSharedPtr<FVoxelRuntime> Runtime = CastChecked<AVoxelActor>(InThis)->Runtime;
	if (!Runtime)
	{
		return;
	}

	Runtime->AddReferencedObjects(Collector);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelRuntimeParameters AVoxelActor::GetRuntimeParameters() const
{
	FVoxelRuntimeParameters Parameters = DefaultRuntimeParameters;

	const TSharedRef<FVoxelRuntimeParameter_PointStorage> Parameter = MakeVoxelShared<FVoxelRuntimeParameter_PointStorage>();
	Parameter->Data = PointStorageComponent->GetData();
	Parameters.Add(Parameter);

	return Parameters;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AVoxelActor::QueueCreateRuntime()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	if (FVoxelRuntime::CanBeCreated(false))
	{
		CreateRuntime();
	}
	else
	{
		bRuntimeCreateQueued = true;
	}
}

void AVoxelActor::CreateRuntime()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	if (!FVoxelRuntime::CanBeCreated(true))
	{
		VOXEL_MESSAGE(Error, "{0}: Cannot create runtime: not ready. See log for more info", this);
		return;
	}

	bRuntimeCreateQueued = false;

	if (IsRuntimeCreated())
	{
		return;
	}

	USceneComponent* Component = GetRootComponent();
	if (!ensure(Component))
	{
		return;
	}

	Runtime = FVoxelRuntime::Create(
		*this,
		*Component,
		GetRuntimeParameters(),
		*ParameterContainer);
}

void AVoxelActor::DestroyRuntime()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	// Clear RuntimeRecreate queue
	bRuntimeRecreateQueued = false;

	if (!IsRuntimeCreated())
	{
		return;
	}

	Runtime->Destroy();
	Runtime.Reset();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelGraphInterface* AVoxelActor::GetGraph() const
{
	return ParameterContainer->GetTypedProvider<UVoxelGraphInterface>();
}

void AVoxelActor::SetGraph(UVoxelGraphInterface* NewGraph)
{
	ParameterContainer->SetProvider(NewGraph);
}