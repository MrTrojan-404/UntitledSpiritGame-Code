// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameAnimationSample/Actor/Weapon.h"
#include "GameAnimationSample/GameVariables/CombatState.h"
#include "GameAnimationSample/GameVariables/WeaponType.h"
#include "GameAnimationSample/UI/HUD/GASPHUD.h"
#include "CombatComponent.generated.h"


class AWeapon;
class AItem;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAMEANIMATIONSAMPLE_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()


public:
	UCombatComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	

	void EquipItem(AItem* ItemToEquip);
	void EquipWeapon(AWeapon* WeaponToEquip);
	void SetAiming(bool bIsAiming);
	void HandleFire(bool bPressed);
	void ThrowGrenade();

	void DropEquippedWeapon();
	void HandleEquippedSwap();
	void EquippedSwap();
	void PlayLocalEquippedSwap();
	void OnEquippedSwapFinished();
	void JumpToShotgunEnd();
	void Reload();
	void ThrowDistraction();

#pragma region AnimNotify Callbacks

	void OnFinishReload();
	void OnShotgunShellReload();
	void OnThrowGrenadeFinished();
	void OnLaunchGrenade();

#pragma endregion AnimNotify Callbacks

	UPROPERTY(ReplicatedUsing = OnRep_CarriedAmmo)
	int32 CarriedAmmo;
	
	UFUNCTION(Server, Reliable)
	void ServerLaunchGrenade(const FVector_NetQuantize& Target);

	void PickupAmmo(EWeaponType WeaponType, int32 AmmoAmount);
	
	bool bLocallyReloading = false;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponInventory)
	TArray<AWeapon*> WeaponInventory;

	UFUNCTION()
	void OnRep_WeaponInventory(TArray<AWeapon*> OldInventory);

	void NotifySpellOfNewWeapon(AWeapon* NewWeapon);
	
	UFUNCTION(Server, Reliable)
	void ServerEquipFromMenuToPrimary(AWeapon* SelectedWeapon);

	UFUNCTION(Server, Reliable)
	void ServerEquipFromMenuToSecondary(AWeapon* SelectedWeapon);

	void AddWeaponToInventory(AWeapon* WeaponToAdd);

	void StoreWeapon(AWeapon* WeaponToStore);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	TArray<AWeapon*> GetWeaponsInCategory(EWeaponCategory Category);
	
#pragma region RPC
	UFUNCTION(Server, Reliable)
	void ServerEquip(AItem* ItemToEquip);
	
	UFUNCTION(Server, Reliable)
	void ServerDropWeapon();

	UFUNCTION(Server, Reliable)
	void ServerEquippedSwapWeapon();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastEquippedSwapWeapon();

	UFUNCTION(Server, Reliable)
	void ServerFire(const FVector_NetQuantize100& TraceHitTarget, float FireDelay);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastFire(const FVector_NetQuantize& TraceHitTarget);
	
	UFUNCTION(Server, Reliable)
	void ServerShotgunFire(const TArray<FVector_NetQuantize100>& TraceHitTargets, float FireDelay);
	
	UFUNCTION(NetMulticast, Reliable)
	void MulticastShotgunFire(const TArray<FVector_NetQuantize100>& TraceHitTargets);
	
	UFUNCTION(Server, Reliable)
	void ServerReload();

	UPROPERTY(EditAnywhere)
	float AimAlignThreshold = 70.f;

#pragma endregion RPC

	bool bFinishedSwapping = false;

	TMap<EWeaponType, int32> CarriedAmmoMap;

	UPROPERTY(ReplicatedUsing = OnRep_Grenades)
	int32 Grenades;

	void UpdateHUDGrenades();

	void ClearWeaponPointers();

	void Melee();
	void MeleeHit(); 

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ResetCombatState();

	float LastWeaponActionTime = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "UI")
	bool bShowCrosshairs = true;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ResetFireTimer();
	
protected:
	virtual void BeginPlay() override;

	void TraceUnderCrosshairs(FHitResult& TraceHitResults);
	
	void SetHUDCrosshairs(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ReloadEmptyWeapon();
	
	void HandleReload();
	int32 AmountToReload();
	void UpdateCarriedAmmo();

	void ShowAttachedGrenade(bool bShowGrenade);

	UFUNCTION(Server, Reliable)
	void ServerThrowGrenade();
	
	UPROPERTY(EditAnywhere)
	TSubclassOf<class AProjectile> GrenadeClass;

	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bIsAiming);

	// --- VISUAL RECOIL ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Recoil")
	FRotator VisualRecoilRotation;

	// How long to wait before launching a bullet when firing from idle (not combat ready)
	UPROPERTY(EditAnywhere, Category = "Combat|HipFire")
	float HipFireRaiseDelay = 0.15f;

	// --- CROSSHAIR TUNING ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Crosshairs")
	float BaseSpread = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Crosshairs")
	float SpreadMaxWalkSpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Crosshairs")
	float MaxVelocitySpread = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Crosshairs")
	float InAirSpreadPenalty = 2.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Crosshairs")
	float AimSpreadBonus = 0.58f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Crosshairs")
	float CrouchSpreadBonus = 0.25f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Crosshairs")
	float ShootingSpreadPenalty = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Crosshairs")
	float SpreadRecoverySpeed = 10.f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Crosshairs")
	float AimRecoverySpeed = 15.f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Crosshairs")
	float MaxShootingSpread = 2.0f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Crosshairs")
	float MinimumSpread = 0.05f; 

	float CrosshairCrouchFactor;
	
	UPROPERTY()
	float CurrentSpreadMultiplier = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Movement State")
	bool bIsCrouching = false;

	UFUNCTION(Server, Reliable)
	void ServerMelee();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastMelee();

	UPROPERTY(EditAnywhere, Category = "Combat|Melee")
	float MeleeDamage = 50.f;

	UPROPERTY(EditAnywhere, Category = "Combat|Melee")
	float MeleeRange = 150.f;

	UPROPERTY(EditAnywhere, Category = "Combat|Melee")
	float MeleeRadius = 45.f;
	
	UPROPERTY(BlueprintReadWrite, Category = "Movement State")
	bool bIsTraversing = false;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Distraction")
	TSubclassOf<class AThrowableDistraction> DistractionClass;
private:
	UPROPERTY()
	AGASPCorePlayerController* Controller;
	
	UPROPERTY()
	AActor* OwnerActor;

	UPROPERTY()
	APawn* OwnerPawn;

	UPROPERTY()
	AGASPHUD* HUD;
   
	APawn* GetOwnerPawn();
	bool IsOwnerLocallyControlled() const;
	
	UPROPERTY(Replicated)
	AWeapon* PrimaryEquippedWeapon;
	
	UPROPERTY(Replicated)
	AWeapon* SecondaryEquippedWeapon;
	
	UPROPERTY(ReplicatedUsing = OnRep_CombatState)
	ECombatState CombatState = ECombatState::ECS_Unoccupied;

	UFUNCTION()
	void OnRep_CombatState();

	EPlayerWeaponState GetPlayerWeaponState() const;
	AGASPCorePlayerController* GetCachedController();

	bool bPredictedSwap = false;
	bool bDropSwap = false;

	UPROPERTY(ReplicatedUsing = OnRep_Aiming)
	bool bAiming = false;

	UFUNCTION()
	void OnRep_Aiming();

	UPROPERTY()
	bool bFireButtonPressed = false;

	UFUNCTION()
	void OnRep_CarriedAmmo();
	
	UPROPERTY(EditAnywhere)
	int32 MaxCarriedAmmo = 500;

	UPROPERTY(EditAnywhere)
	int32 StartingARAmmo = 30;

	UPROPERTY(EditAnywhere)
	int32 StartingRocketAmmo = 0;

	UPROPERTY(EditAnywhere)
	int32 StartingPistolAmmo = 7;

	UPROPERTY(EditAnywhere)
	int32 StartingSMGAmmo = 0;

	UPROPERTY(EditAnywhere)
	int32 StartingShotgunAmmo = 0;

	UPROPERTY(EditAnywhere)
	int32 StartingSniperAmmo = 0;

	UPROPERTY(EditAnywhere)
	int32 StartingGrenadeLauncherAmmo = 0;

	void InitializeCarriedAmmo();

	void Fire();
	void FireProjectileWeapon();
	void FireHitScanWeapon();
	void FireShotgun();
	void LocalFire(const FVector_NetQuantize& TraceHitTarget);
	void ShotgunLocalFire(const TArray<FVector_NetQuantize100>& TraceHitTargets);
	
	FTimerHandle FireTimer;
	bool bCanFire = true;

	void StartFireTimer();
	void FireTimerFinished();
	bool CanFire();

	void UpdateAmmoValues();
	void UpdateShotgunAmmoValues();
	
	UPROPERTY(EditDefaultsOnly)
	int32 StartingGrenades = 2;
	
	UFUNCTION()
	void OnRep_Grenades();

	UPROPERTY(EditAnywhere)
	int32 MaxGrenades = 2;

#pragma region HUD
	
	float CrosshairVelocityFactor;
	float CrosshairInAirFactor;
	float CrosshairAimFactor;
	float CrosshairShootingFactor;

	UPROPERTY(Replicated)
	FVector HitTarget;

	FHUDPackage HUDPackage;

#pragma endregion HUD

	UPROPERTY()
	AActor* LastFocusedActor = nullptr;

	UFUNCTION(Server, Reliable)
	void ServerFinishReload();
	
	bool bAimButtonPressed = false;

	UFUNCTION(Server, Reliable)
	void ServerInterruptShotgunReload();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastInterruptShotgunReload();

	UFUNCTION(Server, Unreliable)
	void ServerSyncHitTarget(FVector_NetQuantize100 Target);

	// Tracks which weapon was being reloaded so a mid-reload swap can't credit the wrong gun
	UPROPERTY()
	AWeapon* WeaponBeingReloaded = nullptr;

	// --- HIP FIRE DELAY ---
	FTimerHandle HipFireDelayTimer;
	FVector DelayedHitTarget;
	TArray<FVector_NetQuantize100> DelayedShotgunTargets;

	// Extracted shot execution — called immediately or after delay
	void ExecuteShot(const FVector_NetQuantize& TraceHitTarget);
	void ExecuteDelayedShot();
	void ExecuteShotgunShot(const TArray<FVector_NetQuantize100>& TraceHitTargets);
	void ExecuteDelayedShotgunShot();

	// Tracks the exact world time the player started holding the trigger
	float ChargeStartTime = 0.f;

	// Track if a charge is actively building
	bool bIsCharging = false;
	
public:
	FORCEINLINE ECombatState GetCombatState() const { return CombatState; }
	UFUNCTION(BlueprintCallable)
	FORCEINLINE AWeapon* GetPrimaryWeapon() const {return  PrimaryEquippedWeapon;}
	FORCEINLINE AWeapon* GetSecondaryWeapon() const {return SecondaryEquippedWeapon; }
	FORCEINLINE int32 GetGrenades() const { return Grenades; }
	FORCEINLINE EWeaponType GetEquippedWeaponType() const;
	FORCEINLINE bool GetFireButtonPressed() const { return bFireButtonPressed; }
	FORCEINLINE bool GetIsAiming() const { return bAiming; }
	FORCEINLINE FVector GetHitTarget() const { return HitTarget; }
	FORCEINLINE void SetHitTarget(const FVector& Target) {HitTarget = Target;}
	FORCEINLINE FRotator GetVisualRecoil() const { return VisualRecoilRotation; }
	FORCEINLINE void SetCombatState(ECombatState State) { CombatState = State; }
	FORCEINLINE bool GetIsTraversing() const { return bIsTraversing; }
	
};