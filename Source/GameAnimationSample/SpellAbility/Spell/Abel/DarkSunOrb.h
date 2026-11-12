#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarkSunOrb.generated.h"

class AGASPBaseCharacter;
class UProjectileMovementComponent;
class USphereComponent;

UCLASS()
class GAMEANIMATIONSAMPLE_API ADarkSunOrb : public AActor
{
	GENERATED_BODY()

public:	
	ADarkSunOrb();

	void Initialize(AGASPBaseCharacter* InCaster);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	UPROPERTY()
	AGASPBaseCharacter* Caster;

	// --- ORB STATS ---
	UPROPERTY(EditDefaultsOnly, Category = "Dark Sun|Stats")
	float OrbLifetime = 6.0f; // Time before it collapses and detonates

	UPROPERTY(EditDefaultsOnly, Category = "Dark Sun|Gravity")
	float GravityRadius = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dark Sun|Gravity")
	float PullStrength = 600.f;

	// --- BURN DOT ---
	UPROPERTY(EditDefaultsOnly, Category = "Dark Sun|Damage")
	float BurnDamagePerTick = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dark Sun|Damage")
	float TickInterval = 0.5f;
	
	// --- DETONATION ---
	UPROPERTY(EditDefaultsOnly, Category = "Dark Sun|Explosion")
	float ExplosionDamage = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dark Sun|Explosion")
	float ExplosionRadius = 1500.f;

private:
	FTimerHandle BurnTimerHandle;
	FTimerHandle DetonationTimerHandle;

	void ApplyGravityPull();
	void ApplyBurnTick();
	void Detonate();

	UPROPERTY()
	TArray<TWeakObjectPtr<AGASPBaseCharacter>> RagdolledTargets;
};