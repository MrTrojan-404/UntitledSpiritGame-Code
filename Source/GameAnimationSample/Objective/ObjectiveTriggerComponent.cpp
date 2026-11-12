#include "ObjectiveTriggerComponent.h"

#include "GameAnimationSample/AI/Character/GASPEnemyBase.h"
#include "GameAnimationSample/Objective/ObjectiveSubsystem.h"

UObjectiveTriggerComponent::UObjectiveTriggerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UObjectiveTriggerComponent::TriggerEvent()
{
	// Only the server is allowed to trigger objective progress
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    
	// Prevent false kills from triggering during Snapshot Time Rewinds!
	if (AGASPEnemyBase* Enemy = Cast<AGASPEnemyBase>(GetOwner()))
	{
		if (Enemy->bIsResetting) return;
	}

	if (EventTag == NAME_None) return;

	if (UObjectiveSubsystem* Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UObjectiveSubsystem>())
	{
		Subsystem->ReportObjectiveEvent(EventTag);
	}
}