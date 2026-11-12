#include "BTTask_PerformMarkedExecution.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"
#include "Kismet/GameplayStatics.h"

UBTTask_PerformMarkedExecution::UBTTask_PerformMarkedExecution()
{
	NodeName = TEXT("Perform Marked Execution");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_PerformMarkedExecution::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FExecutionMemory* Memory = reinterpret_cast<FExecutionMemory*>(NodeMemory);
	Memory->ElapsedTime = 0.f;
	Memory->bHasFired = false;

	AAIController* AICon = OwnerComp.GetAIOwner();
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!AICon || !BB) return EBTNodeResult::Failed;

	AActor* Target = Cast<AActor>(BB->GetValueAsObject(AGASPAIController::BB_MarkedAttackTarget));
	if (!Target) return EBTNodeResult::Failed;

	AICon->SetFocus(Target); // Turns to face the target using the existing focus system

	return EBTNodeResult::InProgress;
}

void UBTTask_PerformMarkedExecution::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FExecutionMemory* Memory = reinterpret_cast<FExecutionMemory*>(NodeMemory);
	Memory->ElapsedTime += DeltaSeconds;

	if (Memory->bHasFired)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	if (Memory->ElapsedTime >= SnapAimDuration)
	{
		AAIController* AICon = OwnerComp.GetAIOwner();
		UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
		if (!AICon || !BB)
		{
			FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
			return;
		}

		if (AActor* Target = Cast<AActor>(BB->GetValueAsObject(AGASPAIController::BB_MarkedAttackTarget)))
		{
			// TODO: trigger a one-shot fire/headshot montage on AICon->GetPawn() here if you have one

			UGameplayStatics::ApplyDamage(Target, ExecutionDamage, AICon, AICon->GetPawn(), UDamageType::StaticClass());
		}

		BB->ClearValue(AGASPAIController::BB_MarkedAttackTarget);
		AICon->ClearFocus(EAIFocusPriority::Gameplay);
		Memory->bHasFired = true;
	}
}