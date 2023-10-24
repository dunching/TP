// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMeshActor.h"
#include <Runtime/Engine/Classes/Kismet/KismetMathLibrary.h>

#include "VoxelMinimal.h"
#include "VoxelActor.h"
#include "VoxelChunkSpawner.h"
#include "VoxelRuntime.h"
#include "TPCharacterMovementComponent.h"


//////////////////////////////////////////////////////////////////////////
// ATPCharacter

ATPCharacter::ATPCharacter(const FObjectInitializer& ObjectInitializer) :
    Super(
        ObjectInitializer.
        SetDefaultSubobjectClass<UTPCharacterMovementComponent>(ACharacter::CharacterMovementComponentName)
    )
{
    // Set size for collision capsule
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

    // Don't rotate when the controller rotates. Let that just affect the camera.
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    // Configure character movement
    GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

    // Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
    // instead of recompiling to adjust them
    GetCharacterMovement()->JumpZVelocity = 700.f;
    GetCharacterMovement()->AirControl = 0.35f;
    GetCharacterMovement()->MaxWalkSpeed = 500.f;
    GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
    GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

    // Create a camera boom (pulls in towards the player if there is a collision)
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
    CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

    // Create a follow camera
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
    FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

    // Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
    // are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

FQuat ATPCharacter::GetWorldTransform() const
{
    FQuat WorldTransform = FQuat::Identity;
    const UCharacterMovementComponent* const MovementComponent = GetCharacterMovement();
    if (MovementComponent)
    {
        WorldTransform = MovementComponent->GetGravityToWorldTransform();
    }

    return WorldTransform;
}

bool ATPCharacter::HasCustomGravity() const
{
    const UCharacterMovementComponent* const MovementComponent = GetCharacterMovement();
    if (MovementComponent)
    {
        return MovementComponent->HasCustomGravity();
    }

    return false;
}

void TestTrans(UWorld*WorldPtr)
{
    // Need at least 4 segments
    auto Segments = 100;
    FVector Center(0);
    float Radius = 500;

    const float AngleInc = 2.f * UE_PI / Segments;
    int32 NumSegmentsY = Segments;
    float Latitude = AngleInc;
    float SinY1 = 0.0f, CosY1 = 1.0f;

    TArray<FVector>Pts;
    while (NumSegmentsY--)
    {
        const float SinY2 = FMath::Sin(Latitude);
        const float CosY2 = FMath::Cos(Latitude);

        FVector Vertex1 = FVector(SinY1, 0.0f, CosY1) * Radius + Center;
        FVector Vertex3 = FVector(SinY2, 0.0f, CosY2) * Radius + Center;
        float Longitude = AngleInc;

        int32 NumSegmentsX = Segments;
        while (NumSegmentsX--)
        {
            const float SinX = FMath::Sin(Longitude);
            const float CosX = FMath::Cos(Longitude);

            const FVector Vertex2 = FVector((CosX * SinY1), (SinX * SinY1), CosY1) * Radius + Center;
            const FVector Vertex4 = FVector((CosX * SinY2), (SinX * SinY2), CosY2) * Radius + Center;

            Pts.Add(Vertex1);
            Pts.Add(Vertex2);

            Vertex1 = Vertex2;
            Vertex3 = Vertex4;
            Longitude += AngleInc;
        }
        SinY1 = SinY2;
        CosY1 = CosY2;
        Latitude += AngleInc;
    }

    FVector Dir = FVector::RightVector;
    FlushPersistentDebugLines(WorldPtr);
    for (auto& ITer : Pts)
    {
        DrawDebugLine(WorldPtr, Center, ITer, FColor::Red, true);

        DrawDebugLine(WorldPtr, ITer, ITer + (ITer.Rotation().RotateVector(Dir) * 100), FColor::Yellow, true);
    }
}

void ATPCharacter::BeginPlay()
{
    // Call the base class  
    Super::BeginPlay();
 //   TestTrans(GetWorld());
    //Add Input Mapping Context
    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }

    TArray<AActor*>ActorAry;
    UGameplayStatics::GetAllActorsOfClassWithTag(this, AVoxelActor::StaticClass(), TEXT("Main"), ActorAry);
    for (auto Iter : ActorAry)
    {
        auto VoxelActor = Cast<AVoxelActor>(Iter);
        if (VoxelActor)
        {
            auto VRTSPtr = VoxelActor->GetRuntime();
            VRTSPtr->OnChunkChangedMap.Add(0, std::bind(
                &ThisClass::OnChunkChanged, this,
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3
            ));
        }
    }
}

void ATPCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    GetCharacterMovement()->SetGravityDirection(-GetActorLocation().GetSafeNormal());
}

FRotator ATPCharacter::GetViewRotation() const
{
    if (Controller != nullptr)
    {
        const auto PCRot = Controller->GetControlRotation();

        const FRotator GravityRelativeDesiredRotation = (PCRot.Quaternion()).Rotator();

        return GravityRelativeDesiredRotation;
    }
    else if (GetLocalRole() < ROLE_Authority)
    {
        // check if being spectated
        for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
        {
            APlayerController* PlayerController = Iterator->Get();
            if (PlayerController &&
                PlayerController->PlayerCameraManager &&
                PlayerController->PlayerCameraManager->GetViewTargetPawn() == this)
            {
                return PlayerController->BlendedTargetViewRotation;
            }
        }
    }

    return GetActorRotation();
}

void ATPCharacter::OnChunkChanged(const FBox& Box, int32 LOD, int32 Size)
{
    switch (LOD)
    {
    case 0:
    {
  //      DrawDebugBox(GetWorld(), Box.GetCenter(), Box.GetExtent(), FColor::Blue, false, 3.f);
    }
    case 1:
    {
   //     DrawDebugBox(GetWorld(), Box.GetCenter(), Box.GetExtent(), FColor::Green, false, 3.f);
    }
    case 2:
    {
   //     DrawDebugBox(GetWorld(), Box.GetCenter(), Box.GetExtent(), FColor::Purple, false, 3.f);
    }
    case 3:
    {
  //      DrawDebugBox(GetWorld(), Box.GetCenter(), Box.GetExtent(), FColor::Orange, false, 3.f);
    }
    case 4:
    {
   //     DrawDebugBox(GetWorld(), Box.GetCenter(), Box.GetExtent(), FColor::Red, false, 3.f);
        const auto Box_ = FCollisionShape::MakeBox(Box.GetExtent());

        FCollisionObjectQueryParams ObjectQueryParams;

        TArray<struct FOverlapResult> OutOverlaps;
        GetWorld()->OverlapMultiByObjectType(OutOverlaps, Box.GetCenter(), FQuat::Identity, ObjectQueryParams, Box_);

        for (auto Iter : OutOverlaps)
        {
            if (Iter.GetActor()->IsA(AStaticMeshActor::StaticClass()))
            {
                Iter.GetActor()->SetActorHiddenInGame(false);
            }
        }
    }
    break;
    default:
    {
        const auto Box_ = FCollisionShape::MakeBox(Box.GetExtent());

        FCollisionObjectQueryParams ObjectQueryParams;

        TArray<struct FOverlapResult> OutOverlaps;
        GetWorld()->OverlapMultiByObjectType(OutOverlaps, Box.GetCenter(), FQuat::Identity, ObjectQueryParams, Box_);

        for (auto Iter : OutOverlaps)
        {
            if (Iter.GetActor()->IsA(AStaticMeshActor::StaticClass()))
            {
                Iter.GetActor()->SetActorHiddenInGame(true);
            }
        }
 //       DrawDebugBox(GetWorld(), Box.GetCenter(), Box.GetExtent(), FColor::Yellow, false, 3.f);
    }
    break;
    };
}

//////////////////////////////////////////////////////////////////////////
// Input

void ATPCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
    // Set up action bindings
    if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) {

        //Jumpings
        EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
        EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

        //Moving
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATPCharacter::Move);

        //Looking
        EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATPCharacter::Look);
    }
}

void ATPCharacter::Move(const FInputActionValue& Value)
{
    // input is a Vector2D
    FVector2D MovementVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        // find out which way is forward
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);

//         // get forward vector
//         const FVector ForwardDirection = CameraBoom->GetForwardVector();
// 
//         // get right vector 
//         const FVector RightDirection = CameraBoom->GetRightVector();

        // get forward vector
        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

        // get right vector 
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

        // add movement 
        AddMovementInput(ForwardDirection, MovementVector.Y);
        AddMovementInput(RightDirection, MovementVector.X);
    }
}

void ATPCharacter::Look(const FInputActionValue& Value)
{
    // input is a Vector2D
    FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        // add yaw and pitch input to controller
        AddControllerYawInput(LookAxisVector.X);
        AddControllerPitchInput(LookAxisVector.Y);
 //       CameraBoom->AddRelativeRotation(FRotator(LookAxisVector.Y, 0.f, 0.f));
    }
}




