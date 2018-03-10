// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VaudevilleGameMode.generated.h"

UCLASS(minimalapi)
class AVaudevilleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVaudevilleGameMode();

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<APawn> StartingPawn;

	

};
