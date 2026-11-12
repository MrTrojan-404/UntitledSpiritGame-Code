// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Projectile.h"
#include "ProjectileBullet.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AProjectileBullet : public AProjectile
{
	GENERATED_BODY()

public:
	AProjectileBullet();
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& Event) override;
#endif

protected:
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;
	virtual void BeginPlay() override;

	// --- CASCADE (Legacy) ---
	UPROPERTY(EditAnywhere, Category = "Impacts|Cascade")
	TMap<TEnumAsByte<EPhysicalSurface>, UParticleSystem*> SurfaceImpactParticles;

	UPROPERTY(EditAnywhere, Category = "Impacts|Cascade")
	UParticleSystem* DefaultImpactParticle;

	// --- NIAGARA (Modern) ---
	UPROPERTY(EditAnywhere, Category = "Impacts|Niagara")
	TMap<TEnumAsByte<EPhysicalSurface>, UNiagaraSystem*> SurfaceImpactNiagara;

	UPROPERTY(EditAnywhere, Category = "Impacts|Niagara")
	UNiagaraSystem* DefaultImpactNiagara;

	UPROPERTY(EditAnywhere, Category = "Projectile|Impacts")
	TMap<TEnumAsByte<EPhysicalSurface>, USoundBase*> SurfaceImpactSounds;

	UPROPERTY(EditAnywhere, Category = "Projectile|Impacts")
	USoundBase* DefaultImpactSound;

	// --- IMPACT DECALS ---
	UPROPERTY(EditAnywhere, Category = "Impacts|Decals")
	TMap<TEnumAsByte<EPhysicalSurface>, class UMaterialInterface*> SurfaceImpactDecals;

	UPROPERTY(EditAnywhere, Category = "Impacts|Decals")
	UMaterialInterface* DefaultImpactDecal;

	UPROPERTY(EditAnywhere, Category = "Impacts|Decals")
	FVector DecalSize = FVector(12.0f, 12.0f, 12.0f);

	UPROPERTY(EditAnywhere, Category = "Impacts|Decals")
	float DecalLifeSpan = 10.0f;
	
};