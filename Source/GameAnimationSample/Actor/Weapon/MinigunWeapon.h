#pragma once

#include "CoreMinimal.h"
#include "ProjectileWeapon.h"
#include "MinigunWeapon.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AMinigunWeapon : public AProjectileWeapon
{
	GENERATED_BODY()

public:
	AMinigunWeapon();

	virtual void Fire(const FVector& HitTarget) override;

protected:
	// Tracks which barrel is up next
	int32 CurrentBarrelIndex = 1;

	// The prefix for your skeletal mesh sockets (e.g., MuzzleFlash_1, MuzzleFlash_2, MuzzleFlash_3)
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Minigun")
	FString BarrelSocketPrefix = TEXT("MuzzleFlash_");

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Minigun")
	int32 TotalBarrels = 3;
};