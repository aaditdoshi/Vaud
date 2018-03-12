// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/InputSettings.h"
#include "Components/InputComponent.h"
#include "BaseGameCharacter.generated.h"

UCLASS()
class VAUDEVILLE_API ABaseGameCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABaseGameCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
