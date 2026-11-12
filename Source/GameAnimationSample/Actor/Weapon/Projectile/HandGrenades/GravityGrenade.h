#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/Actor/Weapon/Projectile/ProjectileGrenade.h"
#include "GravityGrenade.generated.h"

class AGASPBaseCharacter;
class UNiagaraSystem;
class UNiagaraComponent;

UCLASS()
class GAMEANIMATIONSAMPLE_API AGravityGrenade : public AProjectileGrenade
{
	GENERATED_BODY()

public:
	AGravityGrenade();

	virtual void SetStasis(bool bInStasis, float DilationFactor = 1.0f) override;
	
protected:
	virtual void OnDetonate() override;

	// This acts as Phase 2 (triggered when Destroy() is finally called)
	virtual void ExplodeDamage() override;

	// --- PHASE 1: THE VORTEX ---
	UPROPERTY(EditDefaultsOnly, Category = "Gravity|Phase 1")
	float VortexDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Gravity|Phase 1")
	float VortexTickRate = 0.1f; // Runs 10 times a second

	UPROPERTY(EditDefaultsOnly, Category = "Gravity|Phase 1")
	float PullRadius = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Gravity|Phase 1")
	float PullSpeed = 400.f; // Reduced speed since it's a continuous slide

	UPROPERTY(EditDefaultsOnly, Category = "Gravity|Phase 1")
	float PhysicsPullForce = 600.f; // Reduced force for continuous sucking

	UPROPERTY(EditDefaultsOnly, Category = "Gravity|Phase 1")
	float DoTDamage = 2.f; // 2 damage * 10 ticks a sec = 20 dps

	// --- PHASE 2: DETONATION ---
	UPROPERTY(EditDefaultsOnly, Category = "Gravity|Phase 2")
	float FinalBurstDamage = 150.f;

	// --- VFX and Audio ---
	UPROPERTY(EditDefaultsOnly, Category = "Gravity|VFX")
	UNiagaraSystem* VortexVFX;

	UPROPERTY(EditDefaultsOnly, Category = "Gravity|VFX")
	UNiagaraSystem* DetonationVFX;

	UPROPERTY(EditDefaultsOnly, Category = "Gravity|Audio")
	USoundBase* VortexSound;

	UPROPERTY(EditDefaultsOnly, Category = "Gravity|Audio")
	USoundBase* DetonationSound;

	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;
	
private:
	void VortexTick();
	void DetonateVortex();

	FTimerHandle VortexTimerHandle;
	FTimerHandle DetonationTimerHandle;
	
	UPROPERTY()
	UNiagaraComponent* ActiveVortexVFX;

	UPROPERTY()
	TArray<TWeakObjectPtr<AGASPBaseCharacter>> RagdolledTargets;
};