#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IgnitionEffect.generated.h"

class AGASPBaseCharacter;

UCLASS()
class GAMEANIMATIONSAMPLE_API AIgnitionEffect : public AActor
{
	GENERATED_BODY()

public:
	AIgnitionEffect();

	void Initialize(AGASPBaseCharacter* InCaster, AGASPBaseCharacter* InTarget, float InDamagePerTick, float InTickInterval, float InExplosionDamage, float InExplosionRadius);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY()
	AGASPBaseCharacter* Caster;

	UPROPERTY()
	AGASPBaseCharacter* Target;

	// --- STATS ---
	float DamagePerTick;
	float TickInterval;
	float ExplosionDamage;
	float ExplosionRadius;

	bool bHasExploded = false;

	FTimerHandle DamageTickTimer;

	void DamageTick();
	void Explode();
};