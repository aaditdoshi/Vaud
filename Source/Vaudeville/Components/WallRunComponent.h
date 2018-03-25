// Vaudeville 2018

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Components/ActorComponent.h"
#include "WallRunComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VAUDEVILLE_API UWallRunComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UWallRunComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float WallClimbCheckDistance = 50;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float WallClimbBufferDistance = 40;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float VerticalLaunchVelocity = 350;
};
