// Vaudeville 2018

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MantleComponent.generated.h"


UENUM(BlueprintType)
enum  FMantleState 
{
	Start,
	Climb,
	Push
};


UCLASS(Blueprintable,ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VAUDEVILLE_API UMantleComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMantleComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	UPROPERTY(BlueprintReadWrite)
	TEnumAsByte<FMantleState> CurrentState = FMantleState::Start;
};
