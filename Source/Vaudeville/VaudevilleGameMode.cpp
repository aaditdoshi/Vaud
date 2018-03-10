// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VaudevilleGameMode.h"
#include "VaudevilleHUD.h"
#include "VaudevilleCharacter.h"
#include "UObject/ConstructorHelpers.h"

AVaudevilleGameMode::AVaudevilleGameMode()
	: Super()
{
	DefaultPawnClass = StartingPawn;

}
