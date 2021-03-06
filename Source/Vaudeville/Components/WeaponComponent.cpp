// Vaudeville 2018

#include "WeaponComponent.h"


// Sets default values for this component's properties
UWeaponComponent::UWeaponComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UWeaponComponent::BeginPlay()
{
	FActorSpawnParameters spawnparams;
	spawnparams.Owner = GetOwner();
	Super::BeginPlay();
	EquippedWeapon = Cast<AWeapon>(GetWorld()->SpawnActor<AWeapon>(WeaponType,spawnparams));
	if (EquippedWeapon)
	{
		EquippedWeapon->OnEquipped();
	}
	
	// ...
	
}


// Called every frame
void UWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UWeaponComponent::OnFire()
{
	if(EquippedWeapon)
	{
		EquippedWeapon->OnFire();
	}
}

void UWeaponComponent::OnFireStopped()
{
	if (EquippedWeapon)
	{
		EquippedWeapon->OnFireStopped();
	}
}

AWeapon* UWeaponComponent::GetEquippedWeapon()
{
		if(EquippedWeapon)
		{

			return EquippedWeapon;
		}

	return nullptr;
}

bool UWeaponComponent::TryReload()
{
	if (EquippedWeapon)
	{
		if(EquippedWeapon->CanReload())
		{
			EquippedWeapon->OnReloadFinished.AddDynamic(this,&UWeaponComponent::HandleReloadFinishedCallBack);
			EquippedWeapon->StartReload();
			return true;
		}
	}
	return false;
}


bool UWeaponComponent::IsReloading()
{
	if(EquippedWeapon)
	{
		return EquippedWeapon->IsReloading();
	}

	return false;
}

void UWeaponComponent::HandleReloadFinishedCallBack()
{
	OnReloadComplete.Broadcast(EquippedWeapon);
}

