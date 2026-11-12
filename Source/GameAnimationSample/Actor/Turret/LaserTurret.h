#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LaserTurret.generated.h"

class UNiagaraSystem;

UCLASS()
class GAMEANIMATIONSAMPLE_API ALaserTurret : public AActor
{
GENERATED_BODY()
	
public:	
	ALaserTurret();
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Laser Turret")
	void ActivateTurret(TArray<class AGASPBaseCharacter*> Targets);

protected:
	virtual void BeginPlay() override;

	// --- COMPONENTS ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USkeletalMeshComponent* TurretMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UNiagaraComponent* TrackingBeamComp;

	// --- STATS ---
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
	float Health;

	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	float MaxHealth = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	float LaserDamage = 35.f;

	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	float LaserRange = 5000.f;

	// --- SKELETAL MESH CONTROL ---
	// The AnimBP will read this variable every frame to physically rotate the bone
	UPROPERTY(BlueprintReadOnly, Category = "Turret Animation")
	FVector ClampedTargetLocation;

	FVector TargetAimLocation;

	// --- STATE ---
	UPROPERTY(Replicated)
	AGASPBaseCharacter* CurrentTarget;

	UPROPERTY(Replicated)
	bool bIsActive = false;

	UPROPERTY(Replicated)
	bool bIsLocked = false; 

	UPROPERTY(Replicated)
	bool bIsCoolingDown = false; // Hides the tracking beam between shots

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
	bool bIsDestroyed = false;

	// --- LOGIC ---
	FTimerHandle AttackTimer;
	void ChargeLaser();
	void LockLaser();
	void FireLaser();

	UPROPERTY(EditDefaultsOnly, Category = "Timing")
	float ChargeTime = 2.0f; 

	UPROPERTY(EditDefaultsOnly, Category = "Timing")
	float LockTime = 0.5f;   

	UPROPERTY(EditDefaultsOnly, Category = "Timing")
	float CooldownTime = 2.0f; 
	
	UPROPERTY(EditDefaultsOnly, Category = "Timing")
	float TargetLeadTime = 0.4f;

	UPROPERTY(BlueprintReadOnly, Category = "Turret Animation")
	FRotator GunBoneRotation;

	// The Turret stops tracking and resets to default if the player gets closer than this
	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	float DeadzoneRadius = 400.f;

	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	float TrackingSpeed = 25.0f; // Cranked up for snappy, aggressive tracking

	// Used to reset the attack cycle if the player runs underneath the turret
	bool bWasInDeadzone = false;

	UPROPERTY(Replicated)
	TArray<AGASPBaseCharacter*> AvailableTargets;

	FTimerHandle TargetEvalTimer;
	void EvaluateTarget();

	// --- VFX / SFX ---
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	UNiagaraSystem* FireBeamVFX;

	// CHANGED: Now a Niagara System!
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	UNiagaraSystem* ExplosionVFX;

	// NEW: The skeletal mesh sparking effect
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	UNiagaraSystem* ElectrocutionVFX;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	USoundBase* FireSound;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	USoundBase* DestroySound;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFireVFX(FVector EndPoint);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_DestroyTurret();

	// NEW: Timer logic to stop the sparks
	FTimerHandle ElectrocutionTimer;
	void StopElectrocution();

	UPROPERTY()
	UNiagaraComponent* ActiveElectrocuteVFX;
	
private:
	TArray<AActor*> IgnoredActors; 
};