#include "BTTask_SetAimState.h"
#include "AIController.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/Component/CombatComponent.h"

UBTTask_SetAimState::UBTTask_SetAimState()
{
	NodeName = TEXT("Set Aim State");
}

EBTNodeResult::Type UBTTask_SetAimState::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	if (!AICon) return EBTNodeResult::Failed;

	AGASPBaseCharacter* AIChar = Cast<AGASPBaseCharacter>(AICon->GetPawn());
	if (!AIChar) return EBTNodeResult::Failed;

	if (UCombatComponent* Combat = AIChar->GetCombatComponent())
	{
		Combat->SetAiming(bShouldAim);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}

FString UBTTask_SetAimState::GetStaticDescription() const
{
	return FString::Printf(TEXT("Sets AI Aiming to: %s"), bShouldAim ? TEXT("TRUE") : TEXT("FALSE"));
}