#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/AI/GASPAITypes.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GASPAICharacterBase.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPAICharacterBase : public AGASPBaseCharacter
{
	GENERATED_BODY()

public:
	AGASPAICharacterBase();

	virtual void OnDamageReceived(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser) override;

	virtual int32 GetEnemyType() const { return -1; }
	
	// --- UNIVERSAL PERCEPTION ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	float SightRadius = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	float PeripheralVisionAngle = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	float HearingRange = 800.f;

	// --- UNIVERSAL COMBAT ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat")
	float AimZOffset = 30.f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat")
	float AimErrorRadius = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
	float MeleeRange = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
	float EngagementRange = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
	float AttackCooldown = 1.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat")
	int32 CombatStyle = 1; // 0 = Melee, 1 = Ranged, 2 = Sniper, 3 = Heavy_Ranged
	
	bool IsAlive() const { return !bIsDead; }

	virtual bool CanPerformStealthTakedown_Implementation(AActor* Interactor) const override;
	virtual void ExecuteStealthTakedown_Implementation(AActor* Interactor) override;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	bool bIsHeavyEnemy = false;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Death")
	UAnimMontage* ElectrocuteDeathMontage;

	void TriggerElectrocutionDeath(AActor* InstigatorActor, class UNiagaraSystem* ZapVFX, class USoundBase* ZapSound);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayElectrocuteFX(class UNiagaraSystem* ZapVFX, class USoundBase* ZapSound, float Duration, bool bPlayDeathAnimation);

	// The Behavior Tree will call this, and you will hook it up to your GASP nodes in Blueprint!
	UFUNCTION(BlueprintImplementableEvent, Category = "AI|Movement")
	void SetAIMovementSpeed(EAIMovementSpeed SpeedState);
protected:
	virtual void BeginPlay() override;
    
	virtual void HandleDeath(AController* InstigatorController, AActor* DamageCauser);
	void NotifyControllerOfDeath();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastHandleDeath(FVector_NetQuantize HitDirection, FName HitBone);

	bool bIsDead = false;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExecuteStealthTakedown(AActor* Interactor);

	void StopElectrocuteFX();

	UPROPERTY()
	class UNiagaraComponent* ActiveElectrocuteVFX;

	UPROPERTY()
	class UAudioComponent* ActiveElectrocuteSound;

	FTimerHandle ElectrocuteFXTimer;
	
};