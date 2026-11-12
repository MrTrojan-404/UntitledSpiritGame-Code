#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/SpellAbility/SpellBase.h"
#include "IgnitionSpell.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UIgnitionSpell : public USpellBase
{
	GENERATED_BODY()

public:
	UIgnitionSpell()
	{
		EnergySegmentCost = 2; 
	}

	virtual void Execute_Implementation(AGASPBaseCharacter* Caster) override;
	virtual void OnReleased_Implementation(AGASPBaseCharacter* Caster) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Ignition")
	float TraceRange = 2500.f;

	// How often the spell sweeps to find a new target while holding the button
	UPROPERTY(EditDefaultsOnly, Category = "Ignition")
	float ScanInterval = 0.1f;

	// --- BURN DOT ---
	UPROPERTY(EditDefaultsOnly, Category = "Ignition|Damage")
	float BaseDamagePerTick = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ignition|Damage")
	float TickInterval = 0.5f;

	// --- CONSTANT EXPLOSION ---
	UPROPERTY(EditDefaultsOnly, Category = "Ignition|Explosion")
	float ExplosionDamage = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ignition|Explosion")
	float ExplosionRadius = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ignition")
	TSubclassOf<class AActor> IgnitionEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ignition|Outline")
	int32 IgnitionStencil = 254; 

private:
	UPROPERTY()
	AGASPBaseCharacter* CachedCaster = nullptr;

	UPROPERTY()
	TWeakObjectPtr<AGASPBaseCharacter> CachedTarget;

	FTimerHandle ScanTimer;

	void ScanForTarget();
};