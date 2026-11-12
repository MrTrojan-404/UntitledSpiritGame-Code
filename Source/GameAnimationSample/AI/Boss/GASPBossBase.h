#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/AI/Character/GASPAICharacterBase.h"
#include "GASPBossBase.generated.h"

class AGASPArenaManager;
class AWeapon;

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPBossBase : public AGASPAICharacterBase
{
	GENERATED_BODY()

public:
	AGASPBossBase();

	virtual void BeginPlay() override;
	
	// Link this boss to the specific arena it belongs to
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Boss")
	AGASPArenaManager* ArenaManager;

	// Does this boss actually die at 0 HP, or just transition phases?
	UPROPERTY(EditDefaultsOnly, Category = "Boss")
	bool bIsFinalPhase = false;

	// The specific Behavior Tree this boss should use
	UPROPERTY(EditDefaultsOnly, Category = "Boss AI")
	UBehaviorTree* BossBehaviorTree;

	UBehaviorTree* GetBossBehaviorTree() const { return BossBehaviorTree; }

protected:
	// OVERRIDE: We intercept death so they don't dissolve!
	virtual void HandleDeath(AController* InstigatorController, AActor* DamageCauser) override;

	// Allows the Boss to reset its health and stand back up for Phase 2
	UFUNCTION(BlueprintCallable, Category = "Boss")
	void RecoverForNextPhase();
};