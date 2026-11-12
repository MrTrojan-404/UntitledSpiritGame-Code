#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/SpellAbility/SpellBase.h"
#include "HydroCageSpell.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UHydroCageSpell : public USpellBase
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(AGASPBaseCharacter* Caster) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Hydro Cage")
	float TraceRange = 2500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Hydro Cage")
	float CageDuration = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Hydro Cage")
	float DamagePerTick = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Hydro Cage")
	float TickInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Hydro Cage")
	float PullSpeed = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Hydro Cage")
	TSubclassOf<class AHydroCageEffect> EffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Hydro Cage|Audio")
	USoundBase* CastSound;
};