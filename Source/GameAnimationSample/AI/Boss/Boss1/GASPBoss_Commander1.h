#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/AI/Boss/GASPBossBase.h"
#include "GASPBoss_Commander1.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPBoss_Commander1 : public AGASPBossBase 
{
	GENERATED_BODY()

public:
	AGASPBoss_Commander1();

	virtual void Tick(float DeltaTime) override;

	// Call this when the Cutscene finishes and the shield goes up
	UFUNCTION(BlueprintCallable, Category = "Boss Phase 2")
	void StartDownedRegen(bool bIsCoop);

	// Call this when the Arena Manager detects all generators are dead
	UFUNCTION(BlueprintCallable, Category = "Boss Phase 2")
	void StopDownedRegen();

	// Call this from Blueprints/Sequencer to wake up Commander 2
	UFUNCTION(BlueprintCallable, Category = "Boss Phase")
	void SetBossActiveState(bool bIsActive);

protected:
	virtual void BeginPlay() override;
	virtual int32 GetEnemyType() const override { return 4; }

	UPROPERTY(EditDefaultsOnly, Category = "Boss Phase 2")
	class UAnimMontage* DownedLoopMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Boss Phase 2")
	float BaseRegenRate = 30.f; // Health restored per second

	// Should this boss be fighting immediately, or wait hidden?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Phase")
	bool bStartsAwake = true;

	UPROPERTY()
	FTransform ArenaSpawnTransform;

private:
	bool bIsRegenerating = false;
	float CurrentRegenRate = 0.f;
	
};