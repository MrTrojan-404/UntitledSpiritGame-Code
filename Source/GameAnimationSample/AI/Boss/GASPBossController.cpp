#include "GASPBossController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GASPBossBase.h"
#include "TimerManager.h"

void AGASPBossController::LockOntoTarget(AActor* NewTarget)
{
	if (!NewTarget) return;

	// 1. If the Blackboard doesn't exist, the brain is asleep. Wake it up!
	if (!GetBlackboardComponent())
	{
		if (AGASPBossBase* BossPawn = Cast<AGASPBossBase>(GetPawn()))
		{
			if (BossPawn->GetBossBehaviorTree())
			{
				RunBehaviorTree(BossPawn->GetBossBehaviorTree());
			}
		}
	}

	// 2. Now that the brain is guaranteed to be running, lock onto the player
	if (GetBlackboardComponent())
	{
		SetAlertPhase(EAlertPhase::Alert);
		SetTargetActor(NewTarget);
		SetAIState(EAIState::Engage);
	}
	else
	{
		// Fallback retry if the Behavior Tree takes a split second to load
		FTimerHandle RetryTimer;
		GetWorldTimerManager().SetTimer(RetryTimer, [this, NewTarget]()
		{
			LockOntoTarget(NewTarget);
		}, 0.1f, false);
	}
}