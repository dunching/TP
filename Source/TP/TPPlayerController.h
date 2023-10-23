// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include <GameFramework/PlayerController.h>

#include "TPPlayerController.generated.h"

UCLASS(config = Game)
class ATPPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void UpdateRotation(float DeltaTime);

};