// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestingGameMode.h"
#include "TestingCharacter.h"
#include "UObject/ConstructorHelpers.h"

ATestingGameMode::ATestingGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
