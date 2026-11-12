#include "BTTask_SetMovementSpeed.h"
#include "AIController.h"
#include "GameAnimationSample/AI/Character/GASPAICharacterBase.h"

UBTTask_SetMovementSpeed::UBTTask_SetMovementSpeed()
{
	NodeName = TEXT("Set Movement Speed");
}

EBTNodeResult::Type UBTTask_SetMovementSpeed::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	if (!AICon) return EBTNodeResult::Failed;

	AGASPAICharacterBase* AIChar = Cast<AGASPAICharacterBase>(AICon->GetPawn());
	if (AIChar)
	{
		// Fire the event over to Blueprint!
		AIChar->SetAIMovementSpeed(DesiredSpeed);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}

FString UBTTask_SetMovementSpeed::GetStaticDescription() const
{
	FString SpeedString = TEXT("Walk");
	if (DesiredSpeed == EAIMovementSpeed::Jog) SpeedString = TEXT("Jog");
	if (DesiredSpeed == EAIMovementSpeed::Sprint) SpeedString = TEXT("Sprint");

	return FString::Printf(TEXT("Set AI Speed to: %s"), *SpeedString);
}