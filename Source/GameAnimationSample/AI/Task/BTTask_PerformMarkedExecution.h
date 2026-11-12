#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_PerformMarkedExecution.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UBTTask_PerformMarkedExecution : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_PerformMarkedExecution();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
	// Brief snap-aim so it reads as a deliberate shot, not a teleporting hitscan
	UPROPERTY(EditAnywhere, Category = "Execution")
	float SnapAimDuration = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Execution")
	float ExecutionDamage = 99999.f;

	struct FExecutionMemory
	{
		float ElapsedTime = 0.f;
		bool bHasFired = false;
	};

	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FExecutionMemory); }
};