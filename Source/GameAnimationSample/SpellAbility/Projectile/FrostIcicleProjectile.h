#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrostIcicleProjectile.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class AGASPBaseCharacter;
class UFrostLockSpell;
class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class GAMEANIMATIONSAMPLE_API AFrostIcicleProjectile : public AActor
{
	GENERATED_BODY()

public:
	AFrostIcicleProjectile();

	void Initialize(AGASPBaseCharacter* InCaster, AGASPBaseCharacter* InTarget, UFrostLockSpell* InOwningSpell);

protected:
	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	// Server-only overlap detection
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, Category = "Frost Icicle")
	float HomingSpeed = 2500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Frost Icicle")
	float MaxLifetime = 5.f; 

	UPROPERTY(EditDefaultsOnly, Category = "Frost Icicle|VFX")
	UNiagaraSystem* TrailSystem;

	// --- AUDIO ---
	UPROPERTY(EditDefaultsOnly, Category = "Frost Icicle|Audio")
	class USoundBase* TravelSound;

	UPROPERTY(EditDefaultsOnly, Category = "Frost Icicle|Audio")
	class USoundBase* HitSound;

	UPROPERTY()
	class UAudioComponent* ActiveTravelSound;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_HitTarget();
	
private:
	UPROPERTY() 
	AGASPBaseCharacter* Caster;

	UPROPERTY()
	AGASPBaseCharacter* Target;

	UPROPERTY()
	UFrostLockSpell* OwningSpell;

	UPROPERTY()
	UNiagaraComponent* ActiveTrail;

	// Safety flag to prevent double-hits
	bool bHasHit = false;

	// Tracks enemies we've already frozen so we don't freeze them twice
	UPROPERTY()
	TArray<AActor*> HitActors;
};