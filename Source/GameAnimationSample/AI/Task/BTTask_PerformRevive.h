#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_PerformRevive.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UBTTask_PerformRevive : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_PerformRevive();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	// The Blackboard Key holding the downed player (e.g., TargetActor or LeaderActor)
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetToReviveKey;
	
};