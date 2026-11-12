#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_CanSeeTarget.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UBTDecorator_CanSeeTarget
	: public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_CanSeeTarget();

protected:
	virtual bool CalculateRawConditionValue(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) const override;

	virtual FString GetStaticDescription() const override;

private:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector CanSeeTargetKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector IsInGracePeriodKey;
};