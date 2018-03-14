// Vaudeville 2018

#include "Weapon.h"


// Sets default values
AWeapon::AWeapon()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetRootComponent());
}

// Called when the game starts or when spawned
void AWeapon::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if(IsReloading())
	{
		ReloadTimer += DeltaTime;
		if(ReloadTimer>=TimeToReload)
		{
			FinishReloading();
		}
	}

}

void AWeapon::OnEquipped_Implementation()
{
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15, FColor::Red, TEXT("EQUIPPED"));
	}
}

void AWeapon::OnFire_Implementation()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15, FColor::Red, TEXT("Fire"));
	}
}

void AWeapon::OnFireStopped_Implementation()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15, FColor::Red, TEXT("FireStopped"));
	}
}

bool AWeapon::CanReload()
{
	if(IsReloading())
	{
		return false;
	}
	else
	{
		return true;
	}
}

void AWeapon::StartReload()
{
	CurrentlyReloading = true;
}

bool AWeapon::IsReloading()
{
	return CurrentlyReloading;
}

void AWeapon::FinishReloading()
{
	CurrentlyReloading = false;
	if(OnReloadFinished.IsBound())
	{
		OnReloadFinished.Broadcast();
	}
}

