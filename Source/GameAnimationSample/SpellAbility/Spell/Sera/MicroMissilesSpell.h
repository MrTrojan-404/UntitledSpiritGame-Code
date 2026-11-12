#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/SpellAbility/SpellBase.h"
#include "MicroMissilesSpell.generated.h"

class AMicroMissileProjectile;

UCLASS()
class GAMEANIMATIONSAMPLE_API UMicroMissilesSpell : public USpellBase
{
	GENERATED_BODY()

public:
	UMicroMissilesSpell()
	{
		EnergySegmentCost = 2; 
		SpellDisplayName = TEXT("Micro Missiles");
	}

	virtual void Execute_Implementation(AGASPBaseCharacter* Caster) override;
	virtual void OnReleased_Implementation(AGASPBaseCharacter* Caster) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Micro Missiles")
	int32 MaxLocks = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missiles")
	float LockTraceRange = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missiles")
	float ScanInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missiles")
	float LaunchStaggerDelay = 0.15f; 

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missiles")
	TSubclassOf<AMicroMissileProjectile> MissileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missiles|Outline")
	int32 LockStencil = 252; 

private:
	UPROPERTY()
	AGASPBaseCharacter* CachedCaster = nullptr;

	UPROPERTY()
	TArray<TWeakObjectPtr<AGASPBaseCharacter>> LockedTargets;

	FTimerHandle ScanTimer;

	void ScanForTarget();
	
	void LaunchMissile(AGASPBaseCharacter* Caster, AGASPBaseCharacter* Target, int32 Index);
};