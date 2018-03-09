// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VaudevilleGameMode.h"
#include "VaudevilleHUD.h"
#include "VaudevilleCharacter.h"
#include "UObject/ConstructorHelpers.h"

AVaudevilleGameMode::AVaudevilleGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = AVaudevilleHUD::StaticClass();
}
