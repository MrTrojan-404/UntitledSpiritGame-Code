#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MeleeAttack.generated.h"

class AGASPAICharacterBase;
class AGASPMeleeEnemy;

UCLASS()
class GAMEANIMATIONSAMPLE_API UBTTask_MeleeAttack 
	: public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MeleeAttack();

	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	virtual FString GetStaticDescription() const override;

protected:
	virtual EBTNodeResult::Type AbortTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
	
private:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	// How close enemy must be to attack
	UPROPERTY(EditAnywhere, Category = "Melee")
	float AttackRange = 150.f;

	// How long the AI should stand still while punching before the BT node finishes
	UPROPERTY(EditAnywhere, Category = "Melee")
	float AttackDuration = 1.5f;
	
	FTimerHandle AttackTimer;

	UPROPERTY()
	AGASPAICharacterBase* CachedEnemy = nullptr;
	
	UPROPERTY()
	UBehaviorTreeComponent* CachedOwnerComp = nullptr;

	void OnAttackTimerFired();
};