// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Projectile.h"
#include "ProjectileGrenade.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AProjectileGrenade : public AProjectile
{
	GENERATED_BODY()

public:
	AProjectileGrenade();
	virtual void Destroyed() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade Properties")
	bool bExplodeOnImpact = false;

	virtual void SetStasis(bool bInStasis, float DilationFactor = 1.0f) override;
	
protected:
	virtual void BeginPlay() override;
	void Tick(float DeltaSeconds) override;
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;

	UFUNCTION()
	void OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UFUNCTION()
	void EnableGrenadeCollision();

	FTimerHandle CollisionArmingTimer;

	// Controls how bright the material flashes
	UPROPERTY(EditDefaultsOnly, Category = "Grenade|Visuals")
	float MaxGlowIntensity = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Grenade|Audio")
	USoundBase* BeepSound;
	
private:
	UPROPERTY(EditAnywhere)
	USoundBase* BounceSound;

	// A tracker for the visual math (set this to your Fuse/Destroy Time in BeginPlay)
	float CurrentFuseTime = 0.f;

	// Cached material pointer so we can change the glow every frame
	UPROPERTY()
	UMaterialInstanceDynamic* DynamicGrenadeMat;

	void TriggerBeep();
	FTimerHandle BeepTimerHandle;
	float CurrentGlowPercent = 0.f;
	
};