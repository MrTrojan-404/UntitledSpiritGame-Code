#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpellComponent.generated.h"

class AWeapon;
class USpellBase;
class AGASPBaseCharacter;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class GAMEANIMATIONSAMPLE_API USpellComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USpellComponent();

	UFUNCTION(BlueprintCallable, Category = "Spell")
	void TryCastSpell1();

	UFUNCTION(BlueprintCallable, Category = "Spell")
	void TryCastSpell2();

	UFUNCTION(BlueprintCallable, Category = "Spell")
	void TryCastUltimate();

	// Called from weapon/melee damage hooks and the kill-credit hook in HandleDeath.
	UFUNCTION(BlueprintCallable, Category = "Spell")
	void OnDamageDealt(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Spell")
	void OnKillScored(bool bWasStealthKill);

	UFUNCTION(BlueprintPure, Category = "Spell")
	int32 GetCurrentEnergySegments() const { return CurrentEnergySegments; }

	UFUNCTION(BlueprintPure, Category = "Spell")
	int32 GetMaxEnergySegments() const { return MaxEnergySegments; }

	UFUNCTION(BlueprintPure, Category = "Spell")
	float GetUltimateBarPercent() const { return UltimateBarMax > 0.f ? (UltimateBarCurrent / UltimateBarMax) : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Spell")
	bool IsUltimateReady() const { return UltimateBarCurrent >= UltimateBarMax; }

	UFUNCTION(BlueprintCallable, Category = "Spell")
	void ReleaseSpell2();

	UPROPERTY(EditDefaultsOnly, Category = "Spell|Ultimate")
	bool bStartWithFullUltimate = true;

	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ActivateStasis(float Duration, float DilationFactor);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_DeactivateStasis();

	// Refunds whole energy segments if a cast is aborted (e.g., target full health, spell on cooldown)
	UFUNCTION(BlueprintCallable, Category = "Spell|Energy")
	void RefundEnergySegments(int32 SegmentsToRefund);

	UPROPERTY(ReplicatedUsing = OnRep_EnergySegments, BlueprintReadOnly, Category = "Spell|Energy")
	int32 CurrentEnergySegments = 0;

	// --- ENERGY (shared 3-segment bar — identical system for Ren, Abel, and Sera) ---
	UPROPERTY(EditDefaultsOnly, Category = "Spell|Energy")
	int32 MaxEnergySegments = 3;

	// --- ULTIMATE BAR (universal — kills only, no damage contribution, matches GMD across all 3 characters) ---
	UPROPERTY(EditDefaultsOnly, Category = "Spell|Ultimate")
	float UltimateBarMax = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_UltimateBar, BlueprintReadOnly, Category = "Spell|Ultimate")
	float UltimateBarCurrent = 0.f;

	UFUNCTION(BlueprintCallable, Category = "Spell")
	void NotifyWeaponAcquired(AWeapon* NewWeapon);
	
protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	// --- SLOTS — data-driven, assign the concrete USpellBase subclass per character in the Blueprint ---
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Spell|Slots")
	USpellBase* Spell1;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Spell|Slots")
	USpellBase* Spell2;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Spell|Slots")
	USpellBase* Ultimate;
	
	UFUNCTION()
	void OnRep_EnergySegments();

	// How much raw damage dealt converts into a full Energy segment. Tune in editor.
	UPROPERTY(EditDefaultsOnly, Category = "Spell|Energy")
	float DamagePerEnergySegment = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spell|Energy")
	float EnergyPerCombatKill = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spell|Energy")
	float EnergyPerStealthKill = 2.f;

	// Fractional buildup from damage — only converts to a whole segment once it crosses 1.0.
	float FractionalEnergyAccumulator = 0.f;

	void AddEnergy(float Amount); // handles fractional accumulation + clamping + replication
	
	UFUNCTION()
	void OnRep_UltimateBar();

	UPROPERTY(EditDefaultsOnly, Category = "Spell|Ultimate")
	float UltimateGainPerCombatKill = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spell|Ultimate")
	float UltimateGainPerStealthKill = 25.f; 

	void CastSpell(USpellBase* Spell, bool bIsUltimateSlot);

	UFUNCTION(Server, Reliable)
	void Server_ReleaseSpell2();

	UPROPERTY(EditDefaultsOnly, Category = "Spell|Energy")
	bool bStartWithFullEnergy = true; // convenience for testing — set false once real gameplay pacing matters
	
private:
	UPROPERTY()
	AGASPBaseCharacter* CharacterOwner;

	UFUNCTION(Server, Reliable)
	void Server_CastSpell1();

	UFUNCTION(Server, Reliable)
	void Server_CastSpell2();

	UFUNCTION(Server, Reliable)
	void Server_CastUltimate();
};