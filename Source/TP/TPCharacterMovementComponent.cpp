#include "TPCharacterMovementComponent.h"

#include <Engine/Engine.h>
#include <GameFramework/Character.h>
#include <IXRTrackingSystem.h>
#include <IXRCamera.h>
#include <Kismet/KismetMathLibrary.h>

void UTPCharacterMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    WorldToGravityTransform = UKismetMathLibrary::MakeRotFromZX(
        -GravityDirection,
        CharacterOwner->Controller->GetDesiredRotation().Vector()
    ).Quaternion();

    GravityToWorldTransform = WorldToGravityTransform.Inverse();

//     DrawDebugLine(
//         GetWorld(),
//         UpdatedComponent->GetComponentLocation() + (-GravityDirection * 50),
//         UpdatedComponent->GetComponentLocation() + (-GravityDirection * 50) + (WorldToGravityTransform.GetAxisX() * 100),
//         FColor::Yellow, false, 3.f
//     );
//     DrawDebugLine(
//         GetWorld(),
//         UpdatedComponent->GetComponentLocation() + (-GravityDirection * 50),
//         UpdatedComponent->GetComponentLocation() + (-GravityDirection * 50) + (WorldToGravityTransform.GetAxisY() * 100),
//         FColor::White, false, 3.f
//     );
}

void UTPCharacterMovementComponent::SetGravityDirection(const FVector& InNewGravityDir)
{
    FVector NewGravityDir = InNewGravityDir;
    if (ensure(!NewGravityDir.IsNearlyZero()))
    {
        if (!GravityDirection.Equals(NewGravityDir))
        {
            GravityDirection = NewGravityDir;

            bHasCustomGravity = !GravityDirection.Equals(DefaultGravityDirection);
        }
    }
}

void UTPCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
    // Do not update velocity when using root motion or when SimulatedProxy and not simulating root motion - SimulatedProxy are repped their Velocity
    if (!HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME || (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy && !bWasSimulatingRootMotion))
    {
        return;
    }

    Friction = FMath::Max(0.f, Friction);
    const float MaxAccel = GetMaxAcceleration();
    float MaxSpeed = GetMaxSpeed();

    // Check if path following requested movement
    bool bZeroRequestedAcceleration = true;
    FVector RequestedAcceleration = FVector::ZeroVector;
    float RequestedSpeed = 0.0f;
    if (ApplyRequestedMove(DeltaTime, MaxAccel, MaxSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
    {
        bZeroRequestedAcceleration = false;
    }

    if (bForceMaxAccel)
    {
        // Force acceleration at full speed.
        // In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
        if (Acceleration.SizeSquared() > UE_SMALL_NUMBER)
        {
            Acceleration = Acceleration.GetSafeNormal() * MaxAccel;
        }
        else
        {
            Acceleration = MaxAccel * (Velocity.SizeSquared() < UE_SMALL_NUMBER ? UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal());
        }

        AnalogInputModifier = 1.f;
    }

    // Path following above didn't care about the analog modifier, but we do for everything else below, so get the fully modified value.
    // Use max of requested speed and max speed if we modified the speed in ApplyRequestedMove above.
    const float MaxInputSpeed = FMath::Max(MaxSpeed * AnalogInputModifier, GetMinAnalogSpeed());
    MaxSpeed = FMath::Max(RequestedSpeed, MaxInputSpeed);

    // Apply braking or deceleration
    const bool bZeroAcceleration = Acceleration.IsZero();
    const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed);

    // Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
    if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
    {
        const FVector OldVelocity = Velocity;

        const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
        ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration);

        // Don't allow braking to lower us below max speed if we started above it.
        if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(Acceleration, OldVelocity) > 0.0f)
        {
            Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
        }
    }
    else if (!bZeroAcceleration)
    {
        // Friction affects our ability to change direction. This is only done for input acceleration, not path following.
        const FVector AccelDir = Acceleration.GetSafeNormal();
        const float VelSize = Velocity.Size();
        Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);
    }

    // Apply fluid friction
    if (bFluid)
    {
        Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
    }

    // Apply input acceleration
    if (!bZeroAcceleration)
    {
        const float NewMaxInputSpeed = IsExceedingMaxSpeed(MaxInputSpeed) ? Velocity.Size() : MaxInputSpeed;

        UE_LOG(LogTemp, Log, TEXT("Velocity %s"), *Velocity.ToString());
        UE_LOG(LogTemp, Log, TEXT("Acceleration %s"), *(Acceleration * DeltaTime).ToString());
        Velocity += Acceleration * DeltaTime;

        Velocity = Velocity.GetClampedToMaxSize(NewMaxInputSpeed * VelocityScale);
    }

    // Apply additional requested acceleration
    if (!bZeroRequestedAcceleration)
    {
        const float NewMaxRequestedSpeed = IsExceedingMaxSpeed(RequestedSpeed) ? Velocity.Size() : RequestedSpeed;
        Velocity += RequestedAcceleration * DeltaTime;
        Velocity = Velocity.GetClampedToMaxSize(NewMaxRequestedSpeed);
    }

    if (bUseRVOAvoidance)
    {
        CalcAvoidanceVelocity(DeltaTime);
    }
}

void UTPCharacterMovementComponent::MaintainHorizontalGroundVelocity()
{
    FVector GravityRelativeVelocity = RotateWorldToGravity(Velocity);
    if (!FMath::IsNearlyZero(GravityRelativeVelocity.Z))
    {
        if (bMaintainHorizontalGroundVelocity)
        {
            // Ramp movement already maintained the velocity, so we just want to remove the vertical component.
            const auto Size = GravityRelativeVelocity.Size();

            VelocityScale = (Size - GravityRelativeVelocity.Z) / Size;

            GravityRelativeVelocity.Z = 0.f;
        }
        else
        {
            // Rescale velocity to be horizontal but maintain magnitude of last update.
            GravityRelativeVelocity = GravityRelativeVelocity.GetSafeNormal2D() * GravityRelativeVelocity.Size();
        }
    }

    Velocity = RotateGravityToWorld(GravityRelativeVelocity);
}

FVector UTPCharacterMovementComponent::ConstrainInputAcceleration(const FVector& InputAcceleration) const
{
    const auto ControlRotation = CharacterOwner->Controller->GetControlRotation().Quaternion();
    const auto Rot =  WorldToGravityTransform * ControlRotation;

//     DrawDebugLine(
//         GetWorld(),
//         UpdatedComponent->GetComponentLocation(),
//         UpdatedComponent->GetComponentLocation() + (Rot.GetAxisX() * 100),
//         FColor::Black, false, 3.f
//     );
// 
//     DrawDebugLine(
//         GetWorld(),
//         UpdatedComponent->GetComponentLocation(),
//         UpdatedComponent->GetComponentLocation() + (Rot.GetAxisY() * 100),
//         FColor::Blue, false, 3.f
//     );

    return Rot * InputAcceleration;
}

FRotator UTPCharacterMovementComponent::ComputeOrientToMovementRotation(
    const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation
) const
{
    if (Acceleration.SizeSquared() < UE_KINDA_SMALL_NUMBER)
    {
        // AI path following request can orient us in that direction (it's effectively an acceleration)
        if (bHasRequestedVelocity && RequestedVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER)
        {
            return RequestedVelocity.GetSafeNormal().Rotation();
        }

        // Don't change rotation if there is no acceleration.
        return CurrentRotation;
    }

    // Rotate toward direction of acceleration.
    return (GetGravityToWorldTransform() * Acceleration.ToOrientationQuat()).Rotator();
}

void UTPCharacterMovementComponent::PhysicsRotation(float DeltaTime)
{
    if (!(bOrientRotationToMovement || bUseControllerDesiredRotation))
    {
        return;
    }

    if (!HasValidData() || (!CharacterOwner->Controller && !bRunPhysicsWithNoController))
    {
        return;
    }

    FRotator DeltaRot = GetDeltaRotation(DeltaTime);
    DeltaRot.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): GetDeltaRotation"));

    FRotator DesiredRotation = FRotator::ZeroRotator;
    FRotator CurrentRotation = FRotator::ZeroRotator;

    CurrentRotation = UpdatedComponent->GetComponentRotation();
    CurrentRotation = (GetGravityToWorldTransform() * CurrentRotation.Quaternion()).Rotator();

    if (bOrientRotationToMovement)
    {
        DesiredRotation = ComputeOrientToMovementRotation(CurrentRotation, DeltaTime, DeltaRot);
    }
    else if (CharacterOwner->Controller && bUseControllerDesiredRotation)
    {
        DesiredRotation = CharacterOwner->Controller->GetControlRotation();
    }
    else if (!CharacterOwner->Controller && bRunPhysicsWithNoController && bUseControllerDesiredRotation)
    {
        if (AController* ControllerOwner = Cast<AController>(CharacterOwner->GetOwner()))
        {
            DesiredRotation = ControllerOwner->GetDesiredRotation();
        }
    }
    else
    {
        return;
    }

    const bool bWantsToBeVertical = ShouldRemainVertical();

    if (bWantsToBeVertical)
    {
        if (HasCustomGravity())
        {
        }
        else
        {
            DesiredRotation.Pitch = 0.f;
            DesiredRotation.Yaw = FRotator::NormalizeAxis(DesiredRotation.Yaw);
            DesiredRotation.Roll = 0.f;
        }
    }
    else
    {
        DesiredRotation.Normalize();
    }

    // Accumulate a desired new rotation.
    const float AngleTolerance = 1e-3f;

    //	if (!CurrentRotation.Equals(DesiredRotation, AngleTolerance))
    {
        // If we'd be prevented from becoming vertical, override the non-yaw rotation rates to allow the character to snap upright
        // PITCH
        if (!FMath::IsNearlyEqual(CurrentRotation.Pitch, DesiredRotation.Pitch, AngleTolerance))
        {
            DesiredRotation.Pitch = FMath::FixedTurn(CurrentRotation.Pitch, DesiredRotation.Pitch, DeltaRot.Pitch);
        }

        // YAW
        if (!FMath::IsNearlyEqual(CurrentRotation.Yaw, DesiredRotation.Yaw, AngleTolerance))
        {
            DesiredRotation.Yaw = FMath::FixedTurn(CurrentRotation.Yaw, DesiredRotation.Yaw, DeltaRot.Yaw);
        }

        // ROLL
        if (!FMath::IsNearlyEqual(CurrentRotation.Roll, DesiredRotation.Roll, AngleTolerance))
        {
            DesiredRotation.Roll = FMath::FixedTurn(CurrentRotation.Roll, DesiredRotation.Roll, DeltaRot.Roll);
        }

        if (HasCustomGravity())
        {
            FRotator GravityRelativeDesiredRotation = (GetWorldToGravityTransform() * DesiredRotation.Quaternion()).Rotator();
            GravityRelativeDesiredRotation.Pitch = FRotator::NormalizeAxis(GravityRelativeDesiredRotation.Pitch);
            GravityRelativeDesiredRotation.Yaw = FRotator::NormalizeAxis(GravityRelativeDesiredRotation.Yaw);
            GravityRelativeDesiredRotation.Roll = FRotator::NormalizeAxis(GravityRelativeDesiredRotation.Roll);
            
            DesiredRotation = UKismetMathLibrary::MakeRotFromZX(-GetGravityDirection(), GravityRelativeDesiredRotation.Vector());
        }

//         DrawDebugLine(
//             GetWorld(),
//             UpdatedComponent->GetComponentLocation(),
//             UpdatedComponent->GetComponentLocation() + (DesiredRotation.Vector() * 100),
//             FColor::Green, false, 3.f
//         );
        // Set the new rotation.
        DesiredRotation.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): DesiredRotation"));

        MoveUpdatedComponent(FVector::ZeroVector, DesiredRotation, /*bSweep*/ false);
    }
}

void UTPCharacterMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
    if (deltaTime < MIN_TICK_TIME)
    {
        return;
    }

    if (!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)))
    {
        Acceleration = FVector::ZeroVector;
        Velocity = FVector::ZeroVector;
        return;
    }

    if (!UpdatedComponent->IsQueryCollisionEnabled())
    {
        SetMovementMode(MOVE_Walking);
        return;
    }

    bJustTeleported = false;
    bool bCheckedFall = false;
    bool bTriedLedgeMove = false;
    float remainingTime = deltaTime;

    // Perform the move
    while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) && CharacterOwner && (CharacterOwner->Controller || bRunPhysicsWithNoController || HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity() || (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)))
    {
        Iterations++;
        bJustTeleported = false;
        const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
        remainingTime -= timeTick;

        // Save current values
        UPrimitiveComponent* const OldBase = GetMovementBase();
        const FVector PreviousBaseLocation = (OldBase != NULL) ? OldBase->GetComponentLocation() : FVector::ZeroVector;
        const FVector OldLocation = UpdatedComponent->GetComponentLocation();
        const FFindFloorResult OldFloor = CurrentFloor;

        RestorePreAdditiveRootMotionVelocity();

        // Ensure velocity is horizontal.
        MaintainHorizontalGroundVelocity();
        const FVector OldVelocity = Velocity;

        // Apply acceleration
        if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
        {
            CalcVelocity(timeTick, GroundFriction, false, GetMaxBrakingDeceleration());
        }

        ApplyRootMotionToVelocity(timeTick);

        if (IsFalling())
        {
            // Root motion could have put us into Falling.
            // No movement has taken place this movement tick so we pass on full time/past iteration count
            StartNewPhysics(remainingTime + timeTick, Iterations - 1);
            return;
        }

        // Compute move parameters
        const FVector MoveVelocity = Velocity;
        const FVector Delta = timeTick * MoveVelocity;
        const bool bZeroDelta = Delta.IsNearlyZero();
        FStepDownResult StepDownResult;

        if (bZeroDelta)
        {
            remainingTime = 0.f;
        }
        else
        {
            // try to move forward
            MoveAlongFloor(MoveVelocity, timeTick, &StepDownResult);

            if (IsFalling())
            {
                // pawn decided to jump up
                const float DesiredDist = Delta.Size();
                if (DesiredDist > UE_KINDA_SMALL_NUMBER)
                {
                    const float ActualDist = (UpdatedComponent->GetComponentLocation() - OldLocation).Size2D();
                    remainingTime += timeTick * (1.f - FMath::Min(1.f, ActualDist / DesiredDist));
                }
                StartNewPhysics(remainingTime, Iterations);
                return;
            }
            else if (IsSwimming()) //just entered water
            {
                StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
                return;
            }
        }

        // Update floor.
        // StepUp might have already done it for us.
        if (StepDownResult.bComputedFloor)
        {
            CurrentFloor = StepDownResult.FloorResult;
        }
        else
        {
            FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, bZeroDelta, NULL);
        }

        // check for ledges here
        const bool bCheckLedges = !CanWalkOffLedges();
        if (bCheckLedges && !CurrentFloor.IsWalkableFloor())
        {
            // calculate possible alternate movement
            const FVector GravDir = GetGravityDirection();
            const FVector NewDelta = bTriedLedgeMove ? FVector::ZeroVector : GetLedgeMove(OldLocation, Delta, GravDir);
            if (!NewDelta.IsZero())
            {
                // first revert this move
                RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, false);

                // avoid repeated ledge moves if the first one fails
                bTriedLedgeMove = true;

                // Try new movement direction
                Velocity = NewDelta / timeTick;
                remainingTime += timeTick;
                continue;
            }
            else
            {
                // see if it is OK to jump
                // @todo collision : only thing that can be problem is that oldbase has world collision on
                bool bMustJump = bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
                if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump))
                {
                    return;
                }
                bCheckedFall = true;

                // revert this move
                RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, true);
                remainingTime = 0.f;
                break;
            }
        }
        else
        {
            // Validate the floor check
            if (CurrentFloor.IsWalkableFloor())
            {
                if (ShouldCatchAir(OldFloor, CurrentFloor))
                {
                    HandleWalkingOffLedge(OldFloor.HitResult.ImpactNormal, OldFloor.HitResult.Normal, OldLocation, timeTick);
                    if (IsMovingOnGround())
                    {
                        // If still walking, then fall. If not, assume the user set a different mode they want to keep.
                        StartFalling(Iterations, remainingTime, timeTick, Delta, OldLocation);
                    }
                    return;
                }

                AdjustFloorHeight();
                SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
            }
            else if (CurrentFloor.HitResult.bStartPenetrating && remainingTime <= 0.f)
            {
                // The floor check failed because it started in penetration
                // We do not want to try to move downward because the downward sweep failed, rather we'd like to try to pop out of the floor.
                FHitResult Hit(CurrentFloor.HitResult);
                Hit.TraceEnd = Hit.TraceStart + RotateGravityToWorld(FVector(0.f, 0.f, MAX_FLOOR_DIST));
                const FVector RequestedAdjustment = GetPenetrationAdjustment(Hit);
                ResolvePenetration(RequestedAdjustment, Hit, UpdatedComponent->GetComponentQuat());
                bForceNextFloorCheck = true;
            }

            // check if just entered water
            if (IsSwimming())
            {
                StartSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
                return;
            }

            // See if we need to start falling.
            if (!CurrentFloor.IsWalkableFloor() && !CurrentFloor.HitResult.bStartPenetrating)
            {
                const bool bMustJump = bJustTeleported || bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
                if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump))
                {
                    return;
                }
                bCheckedFall = true;
            }
        }


        // Allow overlap events and such to change physics state and velocity
        if (IsMovingOnGround())
        {
            // Make velocity reflect actual move
            if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && timeTick >= MIN_TICK_TIME)
            {
                // TODO-RootMotionSource: Allow this to happen during partial override Velocity, but only set allowed axes?
                Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick;
                MaintainHorizontalGroundVelocity();
            }
        }

        // If we didn't move at all this iteration then abort (since future iterations will also be stuck).
        if (UpdatedComponent->GetComponentLocation() == OldLocation)
        {
            remainingTime = 0.f;
            break;
        }
    }

    if (IsMovingOnGround())
    {
        MaintainHorizontalGroundVelocity();
    }
}
