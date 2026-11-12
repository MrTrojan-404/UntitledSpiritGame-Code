#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/Actor/Weapon.h"
#include "FlamethrowerWeapon.generated.h"

UCLASS(HideCategories=("Weapon|Scatter", "Weapon|Recoil", "Weapon|Suppressor"))
class GAMEANIMATIONSAMPLE_API AFlamethrowerWeapon : public AWeapon
{
	GENERATED_BODY()

public:
	AFlamethrowerWeapon();

	virtual void Tick(float DeltaTime) override;
	virtual void Fire(const FVector& HitTarget) override;

protected:
	virtual void BeginPlay() override;

	// --- Flame Stats ---
	UPROPERTY(EditDefaultsOnly, Category = "Flamethrower")
	float FlameRange = 1200.f; // How far the fire reaches

	UPROPERTY(EditDefaultsOnly, Category = "Flamethrower")
	float FlameRadius = 150.f; // How thick the fire stream is

	UPROPERTY(EditDefaultsOnly, Category = "Flamethrower")
	float FlameTickDamage = 5.f; // Damage dealt every 0.1 seconds

	// --- FX & Audio ---
	UPROPERTY(EditDefaultsOnly, Category = "Flamethrower|VFX")
	class UNiagaraSystem* FlameStreamVFX;

	UPROPERTY()
	class UNiagaraComponent* ActiveFlameVFX;

	UPROPERTY(EditDefaultsOnly, Category = "Flamethrower|Audio")
	USoundBase* FlameStartSound;

	UPROPERTY(EditDefaultsOnly, Category = "Flamethrower|Audio")
	USoundBase* FlameLoopSound;

	UPROPERTY(EditDefaultsOnly, Category = "Flamethrower|Audio")
	USoundBase* FlameEndSound;

	UPROPERTY()
	UAudioComponent* ActiveFlameAudio;
	
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Debug")
	bool bShowDebugHitbox = false;

private:
	bool bIsSpraying = false;
	float TimeSinceLastFired = 0.f;

	void StartSpraying();
	void StopSpraying();
};