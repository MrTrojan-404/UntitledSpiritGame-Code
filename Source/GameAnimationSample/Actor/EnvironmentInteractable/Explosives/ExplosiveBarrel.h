#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExplosiveBarrel.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class UParticleSystem; 

UCLASS()
class GAMEANIMATIONSAMPLE_API AExplosiveBarrel : public AActor
{
	GENERATED_BODY()
	
public:	
	AExplosiveBarrel();
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Stasis")
	void SetStasis(bool bInStasis, float DilationFactor = 1.0f);

	virtual void Destroyed() override;
	
protected:
	virtual void BeginPlay() override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* BarrelMesh;

	// --- Explosion Stats ---
	UPROPERTY(EditDefaultsOnly, Category = "Explosion")
	float MaxHealth = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "Explosion")
	float BaseDamage = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Explosion")
	float DamageRadius = 600.f;

	// --- Fuse Settings ---
	UPROPERTY(EditDefaultsOnly, Category = "Explosion|Fuse")
	float FuseDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Explosion|Fuse")
	UNiagaraSystem* FuseVFX;

	UPROPERTY(EditDefaultsOnly, Category = "Explosion|VFX")
	UParticleSystem* ExplosionParticle;

	UPROPERTY(EditDefaultsOnly, Category = "Explosion|Audio")
	class USoundBase* ExplosionSound;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* DefaultFusePoint;
	
private:
	float CurrentHealth;
	bool bHasExploded = false;
	
	// Fuse state
	bool bIsFuseActive = false;
	float FuseTimeAlive = 0.f;
	
	UPROPERTY()
	UNiagaraComponent* ActiveFuseVFX;

	void Explode();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartFuse(FVector HitLocation, FVector HitNormal, bool bUseDefaultLocation);
};