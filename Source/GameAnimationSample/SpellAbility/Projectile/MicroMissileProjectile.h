#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MicroMissileProjectile.generated.h"

class AGASPBaseCharacter;
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UNiagaraSystem;
class UParticleSystem;
class USoundCue;

UCLASS()
class GAMEANIMATIONSAMPLE_API AMicroMissileProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	AMicroMissileProjectile();

	void Initialize(AGASPBaseCharacter* InCaster, AGASPBaseCharacter* InTarget);

	UFUNCTION(BlueprintCallable, Category = "Stasis")
	void SetStasis(bool bInStasis, float DilationFactor = 1.0f);
	
protected:
	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void Explode(AActor* HitActor);
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MissileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	// ==========================================
	// STATS & FLIGHT PROFILE
	// ==========================================
	UPROPERTY(EditDefaultsOnly, Category = "Micro Missile|Damage")
	float ExplosionDamage = 40.f;

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missile|Damage")
	float ExplosionRadius = 250.f;

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missile|Flight")
	float UpwardLaunchSpeed = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missile|Flight")
	float HomingDelay = 0.4f; // How long it flies up before locking on

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missile|Flight")
	float DiveSpeed = 3500.f; // Speed when it drops

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missile|Flight")
	float DiveTurnRate = 25000.f; // Massive turn rate ensures it never misses

	// ==========================================
	// VFX & AUDIO
	// ==========================================
	UPROPERTY(EditDefaultsOnly, Category = "Micro Missile|VFX")
	UNiagaraSystem* TrailSystem;

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missile|VFX")
	UParticleSystem* ExplosionParticle;
	/*
	UPROPERTY(EditDefaultsOnly, Category = "Micro Missile|VFX")
	UNiagaraSystem* ExplosionSystem;
	*/

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missile|Audio")
	class USoundBase* ExplosionSound;

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missile|Audio")
	class USoundBase* LaunchSound;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Explode(FVector ImpactLocation);
	
private:
	UPROPERTY(Replicated)
	AGASPBaseCharacter* Caster;

	UPROPERTY(Replicated)
	AGASPBaseCharacter* Target;

	UPROPERTY()
	class UNiagaraComponent* ActiveTrail;

	FTimerHandle HomingTimerHandle;

	// Triggers the aggressive downward dive
	void ActivateHoming(); 
};