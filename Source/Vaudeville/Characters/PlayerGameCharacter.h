// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/BaseGameCharacter.h"
#include "PlayerGameCharacter.generated.h"

/**
 * 
 */
UCLASS()
class VAUDEVILLE_API APlayerGameCharacter : public ABaseGameCharacter
{
	GENERATED_BODY()


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	/** Handles moving forward/backward */
	void MoveForward(float Val);

	/** Handles stafing movement, left and right */
	void MoveRight(float Val);

	/**
	* Called via input to turn at a given rate.
	* @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	*/
	void TurnAtRate(float Rate);

	/**
	* Called via input to turn look up/down at a given rate.
	* @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	*/
	void LookUpAtRate(float Rate);

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USpringArmComponent* CameraBoom;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Camera)
	float BaseLookUpRate;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	class USkeletalMeshComponent* FirstPersonMesh;

	APlayerGameCharacter();
};
