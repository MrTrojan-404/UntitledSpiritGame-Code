#include "GASPSniperEnemy.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"

AGASPSniperEnemy::AGASPSniperEnemy()
{
	// Extended perception
	SightRadius = 5000.f;
	HearingRange = 6000.f;
	EngagementRange = 5000.f;
	PreferredCombatDistance = 3000.f;
	MinCombatDistance = 1000.f;

	// Lower health — fragile at range
	MaxHealth = 75.f;
	Health = 75.f;

	// Slower attack
	AttackCooldown = 3.f;
}

void AGASPSniperEnemy::BeginPlay()
{
	Super::BeginPlay();

	AGASPAIController* AIController = Cast<AGASPAIController>(GetController());
	if (AIController && AIController->GetBlackboardComponent())
	{
		AIController->GetBlackboardComponent()->SetValueAsInt(FName("EnemyType"), 2);
		AIController->GetBlackboardComponent()->SetValueAsFloat(FName("AimDuration"), AimDuration);
		
		//Push PreferredCombatDistance to the Blackboard
		AIController->GetBlackboardComponent()->SetValueAsFloat(FName("PreferredRange"), PreferredCombatDistance);
	}
}