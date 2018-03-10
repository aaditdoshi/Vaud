// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerGameCharacter.h"


APlayerGameCharacter::APlayerGameCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
}

void APlayerGameCharacter::BeginPlay()
{
	Super::BeginPlay();

	if(CameraBoom)
	{
		CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoomComponent"));
	}
	if (FirstPersonCameraComponent)
	{
		FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
		FirstPersonCameraComponent->SetupAttachment(CameraBoom);
	}
}

void APlayerGameCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APlayerGameCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{

	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &APlayerGameCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &APlayerGameCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &APlayerGameCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &APlayerGameCharacter::LookUpAtRate);

}

void APlayerGameCharacter::MoveForward(float Val)
{

	if (Val != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Val);
	}
}

void APlayerGameCharacter::MoveRight(float Val)
{
	if (Val != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Val);
	}
}

void APlayerGameCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());

}

void APlayerGameCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}


