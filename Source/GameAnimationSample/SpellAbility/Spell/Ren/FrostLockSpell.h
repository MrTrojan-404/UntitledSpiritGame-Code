#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/SpellAbility/SpellBase.h"
#include "FrostLockSpell.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UFrostLockSpell : public USpellBase
{
	GENERATED_BODY()

public:
	UFrostLockSpell()
	{
		EnergySegmentCost = 1; // The upfront cost to start the spell
	}
	virtual void Execute_Implementation(AGASPBaseCharacter* Caster) override;
	virtual void OnReleased_Implementation(AGASPBaseCharacter* Caster) override;

	// Called by AFrostIcicleProjectile on impact — public so the projectile can reach back in.
	void ApplyFreezeToTarget(AGASPBaseCharacter* Target);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Frost Lock")
	int32 MaxLocks = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Frost Lock")
	float LockTraceRange = 2500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Frost Lock")
	float LockScanInterval = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Frost Lock")
	float FreezeDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Frost Lock")
	float DamagePerTarget = 25.f;

	UPROPERTY(EditDefaultsOnly, Category = "Frost Lock")
	float LaunchStaggerDelay = 0.15f; 

	UPROPERTY(EditDefaultsOnly, Category = "Frost Lock")
	TSubclassOf<class AFrostIcicleProjectile> IcicleProjectileClass;

	// --- VISUALS ---
	UPROPERTY(EditDefaultsOnly, Category = "Frost Lock|Visuals")
	class UMaterialInterface* FreezeMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Frost Lock|Visuals")
	class UNiagaraSystem* FrozenSkeletalVFX;

	// --- AUDIO ---
	UPROPERTY(EditDefaultsOnly, Category = "Frost Lock|Audio")
	class USoundBase* CastSound;

	UPROPERTY(EditDefaultsOnly, Category = "Frost Lock|Audio")
	class USoundBase* ShatterSound;

	UPROPERTY(EditDefaultsOnly, Category = "Frost Lock|Damage")
	float InitialDamage = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Frost Lock|Damage")
	float DamagePerTick = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Frost Lock|Damage")
	float TickInterval = 0.5f;
	
private:
	UPROPERTY()
	TArray<TWeakObjectPtr<AGASPBaseCharacter>> LockedTargets;

	UPROPERTY()
	AGASPBaseCharacter* CachedCaster = nullptr;

	FTimerHandle ScanTimer;

	void ScanForTarget();
	void LaunchIcicle(AGASPBaseCharacter* Target, int32 Index);
	void ShatterTarget(AGASPBaseCharacter* Target);

	// Maps the enemy character to their specific continuous damage timer
	UPROPERTY()
	TMap<TWeakObjectPtr<AGASPBaseCharacter>, FTimerHandle> ContinuousDamageTimers;

	void ContinuousDamageTick(AGASPBaseCharacter* Target);
	
	UPROPERTY(EditDefaultsOnly, Category = "Frost Lock|Outline")
	int32 FrostLockedStencil = 254;
};