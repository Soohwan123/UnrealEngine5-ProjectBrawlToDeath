// Copyright Epic Games, Inc. All Rights Reserved.

#include "BrawlToDeathGameMode.h"
#include "BrawlToDeathCharacter.h"
#include "UObject/ConstructorHelpers.h"

ABrawlToDeathGameMode::ABrawlToDeathGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
