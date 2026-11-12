#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/SpellAbility/SpellBase.h"
#include "StasisSpell.generated.h"

class AWeapon;
class AHitScanWeapon;

UCLASS()
class GAMEANIMATIONSAMPLE_API UStasisSpell : public USpellBase
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(AGASPBaseCharacter* Caster) override;

	void TickStasis(float RealDeltaSeconds);
	bool IsActive() const { return bStasisActive; }

	void LocalActivate(AGASPBaseCharacter* Caster, float Duration, float DilationFactor);
	void LocalDeactivate();

	UPROPERTY()
	TArray<TWeakObjectPtr<AWeapon>> FlaggedWeapons;

	void RegisterWeaponForStasis(AWeapon* NewWeapon);
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Stasis")
	float StasisDuration = 5.f;

	// Now acts as the slow amount applied to enemies and projectiles (0.02 = 2% speed)
	UPROPERTY(EditDefaultsOnly, Category = "Stasis")
	float GlobalDilationFactor = 0.02f; 

	UPROPERTY(EditDefaultsOnly, Category = "Stasis")
	float StasisRadius = 2000.f;

	// ==========================================
	// AUDIO
	// ==========================================
	UPROPERTY(EditDefaultsOnly, Category = "Stasis|Audio")
	class USoundBase* CastSound;

	UPROPERTY(EditDefaultsOnly, Category = "Stasis|Audio")
	class USoundBase* LoopSound;

	UPROPERTY(EditDefaultsOnly, Category = "Stasis|Audio")
	class USoundBase* EndSound;

private:
	UPROPERTY()
	AGASPBaseCharacter* CachedCaster = nullptr;

	UPROPERTY()
	TArray<TWeakObjectPtr<AGASPBaseCharacter>> AffectedTargets;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> AffectedProjectiles;

	float ElapsedRealTime = 0.f;
	bool bStasisActive = false;

	FVector StasisOrigin;
	void CatchNewProjectiles();

	UPROPERTY()
	class UAudioComponent* ActiveLoopSound;
};