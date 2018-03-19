// Vaudeville 2018

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Weapon.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReloadFinished);

UCLASS()
class VAUDEVILLE_API AWeapon : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWeapon();

	/** Gun mesh: 1st person view (seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Mesh)
	USkeletalMeshComponent* WeaponMesh;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo")
	float TimeToReload = 1;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void OnEquipped();

	void OnEquipped_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void OnFire();

	void OnFire_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void OnFireStopped();

	void OnFireStopped_Implementation();

	bool CanReload();

	UFUNCTION(BlueprintNativeEvent, Category = "Reload")
	void StartReload();

	void StartReload_Implementation();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool IsReloading();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void FinishReloading();

	FOnReloadFinished OnReloadFinished;

private:

	bool CurrentlyReloading;

	float ReloadTimer = 0;
};
