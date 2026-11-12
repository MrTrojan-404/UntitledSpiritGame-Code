#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HydroCageEffect.generated.h"

class AGASPBaseCharacter;
class UNiagaraSystem;
class UNiagaraComponent;
class USoundBase;
class UAudioComponent;

UCLASS()
class GAMEANIMATIONSAMPLE_API AHydroCageEffect : public AActor
{
	GENERATED_BODY()

public:
	AHydroCageEffect();

	void Initialize(AGASPBaseCharacter* InCaster, AActor* InTarget, float InDuration, float InDamagePerTick, float InTickInterval, float InPullSpeed);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	AGASPBaseCharacter* Caster;

	UPROPERTY(Replicated)
	AActor* Target;

	float Duration;
	float DamagePerTick;
	float TickInterval;
	float PullSpeed;

	UPROPERTY(EditDefaultsOnly, Category = "Hydro Cage")
	float SpringStrength = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Hydro Cage")
	float SpringDamping = 5.f;

	// No longer replicated! Each machine calculates this locally so they never desync.
	float LocalHoverZ = 0.f;

	FTimerHandle DamageTickTimer;
	FTimerHandle ExpireTimer;

	void ApplyPull();
	void DamageTick();
	void EndCage();

	// ==========================================
	// VFX & AUDIO
	// ==========================================

	UPROPERTY(EditDefaultsOnly, Category = "Hydro Cage|VFX")
	UNiagaraSystem* WaterDomeVFX;

	UPROPERTY(EditDefaultsOnly, Category = "Hydro Cage|VFX")
	UNiagaraSystem* WaterBurstVFX;

	UPROPERTY(EditDefaultsOnly, Category = "Hydro Cage|Audio")
	USoundBase* LoopSound;

	UPROPERTY(EditDefaultsOnly, Category = "Hydro Cage|Audio")
	USoundBase* BurstSound;

	UPROPERTY(ReplicatedUsing = OnRep_Trapped)
	bool bIsTrapped = false;

	UFUNCTION()
	void OnRep_Trapped();

	void ApplyTrapVisualsAndPhysics();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ReleaseTarget();

private:
	UPROPERTY()
	UNiagaraComponent* ActiveDomeVFX;

	UPROPERTY()
	UAudioComponent* ActiveLoopSound;
};