// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPGameMode.h"
#include "TPCharacter.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif

ATPGameMode::ATPGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
