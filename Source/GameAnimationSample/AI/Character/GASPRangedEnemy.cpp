#include "GASPRangedEnemy.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"

AGASPRangedEnemy::AGASPRangedEnemy()
{
	SightRadius = 4500.f;
	HearingRange = 5500.f;
	EngagementRange = 3500.f;

	MaxHealth = 100.f;
	Health = 100.f;
}

void AGASPRangedEnemy::BeginPlay()
{
	Super::BeginPlay();

	AGASPAIController* AIController = Cast<AGASPAIController>(GetController());
	if (AIController && AIController->GetBlackboardComponent())
	{
		AIController->GetBlackboardComponent()->SetValueAsInt(FName("EnemyType"), 1);
		
		// Push EngagementRange to the Blackboard
		AIController->GetBlackboardComponent()->SetValueAsFloat(FName("PreferredRange"), EngagementRange);
	}
}