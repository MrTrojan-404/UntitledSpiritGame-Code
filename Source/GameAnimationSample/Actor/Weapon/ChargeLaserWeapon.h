#pragma once

#include "CoreMinimal.h"
#include "HitScanWeapon.h"
#include "ChargeLaserWeapon.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AChargeLaserWeapon : public AHitScanWeapon
{
	GENERATED_BODY()

public:
	AChargeLaserWeapon();

	virtual void Fire(const FVector& HitTarget) override;

	UPROPERTY(EditAnywhere, Category = "Weapon|Charge")
	float LaserRange = 10000.f;

	// How many seconds the trigger must be held before it can fire
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Charge")
	float RequiredChargeTime = 1.0f; 

	// --- Visuals & Audio ---
	UPROPERTY(EditAnywhere, Category = "Weapon|Charge")
	UParticleSystem* ChargeVFX;

	UPROPERTY(EditAnywhere, Category = "Weapon|Charge")
	USoundBase* ChargeSound;

	UPROPERTY(EditAnywhere, Category = "Weapon|Charge")
	UParticleSystem* DismissVFX;

	UPROPERTY(EditAnywhere, Category = "Weapon|Charge")
	USoundBase* DismissSound;

	void StartCharging();
	void CancelCharging();

protected:
	void ExecuteBeamSegment(const FVector& Start, const FVector& Direction, float Range, int32 BouncesRemaining, TArray<AActor*>& AlreadyHitActors);

	// Track the active components so we can stop them early
	UPROPERTY()
	UParticleSystemComponent* ActiveChargeVFX;

	UPROPERTY()
	UAudioComponent* ActiveChargeSound;
};