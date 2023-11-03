// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include <GameFramework/CharacterMovementComponent.h>

#include "TPCharacterMovementComponent.generated.h"

UCLASS(config = Game)
class UTPCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:

    virtual void TickComponent(
        float DeltaTime,
        enum ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    virtual void SetGravityDirection(const FVector& GravityDir)override;

    virtual void CalcVelocity(
        float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration
    )override;


    virtual void MaintainHorizontalGroundVelocity()override;

    virtual FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const override;

    virtual FRotator ComputeOrientToMovementRotation(
        const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation
    ) const override;

    virtual void PhysicsRotation(float DeltaTime)override;

    virtual void PhysWalking(float deltaTime, int32 Iterations)override;

private:

    float VelocityScale = 1.f;

};