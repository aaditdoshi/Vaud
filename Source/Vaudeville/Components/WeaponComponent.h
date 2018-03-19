// Vaudeville 2018

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Weapon/Weapon.h"
#include "WeaponComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReloadComplete, AWeapon*, ReloadedWeapon);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VAUDEVILLE_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UWeaponComponent();

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<AWeapon> WeaponType;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable)
	void OnFire();

	UFUNCTION(BlueprintCallable)
	void OnFireStopped();

	UFUNCTION(BlueprintCallable)
	AWeapon* GetEquippedWeapon()	;

	UFUNCTION(BlueprintCallable)
	bool TryReload();
	
	UFUNCTION(BlueprintCallable)
	bool IsReloading();

	UPROPERTY(BlueprintAssignable, Category = "Reload")
	FOnReloadComplete OnReloadComplete;

	UFUNCTION()
	void HandleReloadFinishedCallBack();

	AWeapon* EquippedWeapon;
	
};
