#include "TPPlayerController.h"

#include <Engine/Engine.h>
#include <GameFramework/Character.h>
#include <IXRTrackingSystem.h>
#include <IXRCamera.h>
#include <Kismet/KismetMathLibrary.h>
#include "TPCharacter.h"

void ATPPlayerController::UpdateRotation(float DeltaTime)
{
	// Calculate Delta to be applied on ViewRotation
	FRotator DeltaRot(RotationInput);

	FRotator ViewRotation = GetControlRotation();

	if (PlayerCameraManager)
	{
		PlayerCameraManager->ProcessViewRotation(DeltaTime, ViewRotation, DeltaRot);
	}

	AActor* ViewTarget = GetViewTarget();
	if (!PlayerCameraManager || !ViewTarget || !ViewTarget->HasActiveCameraComponent() || ViewTarget->HasActivePawnControlCameraComponent())
	{
		if (IsLocalPlayerController() && GEngine->XRSystem.IsValid() && GetWorld() != nullptr && GEngine->XRSystem->IsHeadTrackingAllowedForWorld(*GetWorld()))
		{
			auto XRCamera = GEngine->XRSystem->GetXRCamera();
			if (XRCamera.IsValid())
			{
				XRCamera->ApplyHMDRotation(this, ViewRotation);
			}
		}
	}

	SetControlRotation(ViewRotation);

	APawn* const P = GetPawnOrSpectator();
	if (P)
	{
		P->FaceRotation(ViewRotation, DeltaTime);
	}
}

void ATPPlayerController::SetControlRotation(const FRotator& NewRotation)
{
	if (!IsValidControlRotation(NewRotation))
	{
		logOrEnsureNanError(TEXT("AController::SetControlRotation attempted to apply NaN-containing or NaN-causing rotation! (%s)"), *NewRotation.ToString());
		return;
	}

	ControlRotation = NewRotation;

	if (RootComponent && RootComponent->IsUsingAbsoluteRotation())
	{
		APawn* const P = GetPawnOrSpectator();
		if (P)
		{
			RootComponent->SetWorldRotation(
                P->GetGravityTransform() 
			);
		}
	}
}

FRotator ATPPlayerController::GetDesiredRotation() const
{
	if (RootComponent && RootComponent->IsUsingAbsoluteRotation())
	{
		return RootComponent->GetComponentRotation();
	}
	return FRotator::ZeroRotator;
}
