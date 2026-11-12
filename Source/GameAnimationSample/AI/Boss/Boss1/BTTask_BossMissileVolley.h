#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_BossMissileVolley.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UBTTask_BossMissileVolley : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_BossMissileVolley();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Missiles")
	TSubclassOf<class AMicroMissileProjectile> MissileClass;

	UPROPERTY(EditAnywhere, Category = "Missiles")
	int32 MissilesToFire = 2;

	UPROPERTY(EditAnywhere, Category = "Missiles")
	float TimeBetweenMissiles = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Missiles")
	float InitialLaunchDelay = 0.5f;

private:
	FTimerHandle LaunchTimer;
	int32 CurrentMissileIndex = 0;

	UPROPERTY()
	class AGASPBaseCharacter* CachedBoss = nullptr;

	UPROPERTY()
	class AGASPBaseCharacter* CachedTarget = nullptr;

	UBehaviorTreeComponent* CachedOwnerComp = nullptr;

	void FireNextMissile();
	void FinishVolley();
};