// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/Actor/Weapon.h"
#include "HitScanWeapon.generated.h"

class ABulletTrailActor;

USTRUCT(BlueprintType)
struct FBufferedShot
{
	GENERATED_BODY()

	UPROPERTY()
	FHitResult HitResult;

	UPROPERTY()
	FVector TraceStart;

	UPROPERTY()
	AController* InstigatorController;
	
	UPROPERTY()
	TWeakObjectPtr<ABulletTrailActor> TrailActor;

	UPROPERTY() TWeakObjectPtr<USkeletalMeshComponent> HitMeshComp;
	UPROPERTY() FName HitBoneName = NAME_None;
	UPROPERTY() FVector LocalHitOffset = FVector::ZeroVector; // ImpactPoint in the hit bone's local space
};

UCLASS()
class GAMEANIMATIONSAMPLE_API AHitScanWeapon : public AWeapon
{
	GENERATED_BODY()

public:
	virtual void Fire(const FVector& HitTarget) override;

	UPROPERTY(EditAnywhere)
	UParticleSystem* BeamParticles;

	UFUNCTION(BlueprintCallable, Category = "Weapon|Stasis")
	virtual void ReleaseBufferedShots();
	
protected:
	void WeaponTraceHit(const FVector& TraceStart, const FVector& HitTarget, FHitResult& OutHit, ABulletTrailActor** OutTrailActor = nullptr);

	UPROPERTY()
	TArray<FBufferedShot> BufferedShots;

	void ProcessShotImpact(const FHitResult& FireHit, const FVector& Start, AController* InstigatorController);
	
	UPROPERTY(EditAnywhere)
	USoundBase* HitSound;
	
	UPROPERTY(EditAnywhere)
	UParticleSystem* ImpactParticles;

	// Weapon|Effects — optional Niagara smoke trail (leave null in BP to skip)
	UPROPERTY(EditAnywhere, Category = "Weapon|Effects")
	class UNiagaraSystem* BulletTrailSystem;

	// How fast the visual trail travels (units/sec) — tune per weapon
	UPROPERTY(EditAnywhere, Category = "Weapon|Effects")
	float BulletTrailSpeed = 40000.f;

	// --- CASCADE (Legacy) ---
	UPROPERTY(EditAnywhere, Category = "Impacts|Cascade")
	TMap<TEnumAsByte<EPhysicalSurface>, UParticleSystem*> SurfaceImpactParticles;

	UPROPERTY(EditAnywhere, Category = "Weapon|Impacts")
	UParticleSystem* DefaultImpactParticles;

	// --- NIAGARA (Modern) ---
	UPROPERTY(EditAnywhere, Category = "Impacts|Niagara")
	TMap<TEnumAsByte<EPhysicalSurface>, UNiagaraSystem*> SurfaceImpactNiagara;

	UPROPERTY(EditAnywhere, Category = "Impacts|Niagara")
	UNiagaraSystem* DefaultImpactNiagara;

	UPROPERTY(EditAnywhere, Category = "Weapon|Impacts")
	TMap<TEnumAsByte<EPhysicalSurface>, USoundBase*> SurfaceImpactSounds;

	// Keep these as fallbacks if a surface isn't found in the map
	UPROPERTY(EditAnywhere, Category = "Weapon|Impacts")
	USoundBase* DefaultImpactSound;

	// --- IMPACT DECALS ---
	UPROPERTY(EditAnywhere, Category = "Impacts|Decals")
	TMap<TEnumAsByte<EPhysicalSurface>, class UMaterialInterface*> SurfaceImpactDecals;

	UPROPERTY(EditAnywhere, Category = "Impacts|Decals")
	UMaterialInterface* DefaultImpactDecal;

	UPROPERTY(EditAnywhere, Category = "Impacts|Decals")
	FVector DecalSize = FVector(16.0f, 16.0f, 16.0f);

	UPROPERTY(EditAnywhere, Category = "Impacts|Decals")
	float DecalLifeSpan = 10.0f;
	
};