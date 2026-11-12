#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SetAimState.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UBTTask_SetAimState : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SetAimState();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Combat")
	bool bShouldAim = true;
};