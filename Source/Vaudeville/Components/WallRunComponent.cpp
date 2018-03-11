// Vaudeville 2018

#include "WallRunComponent.h"
#include "DrawDebugHelpers.h"
#include "Characters/BaseGameCharacter.h"


TAutoConsoleVariable<int32> CVarWallRunDebug(TEXT("WallRun.Debug"),0,TEXT("Show all Debug info for wall running component"));
// Sets default values for this component's properties
UWallRunComponent::UWallRunComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UWallRunComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UWallRunComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	FHitResult Hit;
	FVector StartPoint = GetOwner()->GetActorLocation();
	FVector EndPoint = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector()* WallClimbCheckDistance;
	if(GetWorld()->LineTraceSingleByChannel(Hit,StartPoint,EndPoint,ECC_Visibility))
	{
		if (CVarWallRunDebug->GetInt() == 1)
		{
			DrawDebugPoint(GetWorld(), Hit.Location, 20, FColor::Red, false, 0.3);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Yellow, Hit.GetActor()->GetName());
			}
		}

		if(Hit.Distance< WallClimbBufferDistance)
		{
			ABaseGameCharacter* Character = Cast<ABaseGameCharacter>(GetOwner());
			if(Character)
			{
				Character->LaunchCharacter(FVector(0, 0, 350), true, true);
			}
		}
	}
	// ...
}

