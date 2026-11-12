#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/SpellAbility/SpellBase.h"
#include "MendSpell.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UMendSpell : public USpellBase
{
	GENERATED_BODY()

public:
	UMendSpell();

	virtual void Execute_Implementation(AGASPBaseCharacter* Caster) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Mend")
	float HealAmount = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "Mend")
	float CooldownSeconds = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Mend")
	float TargetingRange = 1500.f;

	// --- VFX & AUDIO ---
	UPROPERTY(EditDefaultsOnly, Category = "Mend|VFX")
	class UNiagaraSystem* HealNiagaraVFX;

	UPROPERTY(EditDefaultsOnly, Category = "Mend|Audio")
	USoundBase* HealSound;

	// How long the looping particle stays on the character before fading out
	UPROPERTY(EditDefaultsOnly, Category = "Mend|VFX")
	float HealVFXDuration = 2.0f;
	
private:
	float LastCastTime = -100.f;
};