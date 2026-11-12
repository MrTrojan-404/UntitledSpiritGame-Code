#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_CalculateDistance.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UBTService_CalculateDistance : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_CalculateDistance();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector DistanceKey; // Floats the result to the Blackboard
};