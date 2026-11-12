#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/SpellAbility/SpellBase.h"
#include "InfernoWaveSpell.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UInfernoWaveSpell : public USpellBase
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(AGASPBaseCharacter* Caster) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Inferno Wave")
	float ConeRange = 700.f;

	UPROPERTY(EditDefaultsOnly, Category = "Inferno Wave")
	float ConeHalfAngleDegrees = 45.f;

	UPROPERTY(EditDefaultsOnly, Category = "Inferno Wave")
	float Damage = 40.f;

	UPROPERTY(EditDefaultsOnly, Category = "Inferno Wave")
	float LaunchSpeed = 900.f;

	UPROPERTY(EditDefaultsOnly, Category = "Inferno Wave")
	float LaunchDurationMs = 500.f;

	// Heavies still take a small forward shove — just far less than a full launch
	UPROPERTY(EditDefaultsOnly, Category = "Inferno Wave")
	float HeavyStaggerSpeedMultiplier = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Inferno Wave")
	float RagdollImpulseStrength = 4000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Inferno Wave")
	float RagdollRecoverDuration = 2.5f;
};