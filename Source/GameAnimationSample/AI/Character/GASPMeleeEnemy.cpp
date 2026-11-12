#include "GASPMeleeEnemy.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"

AGASPMeleeEnemy::AGASPMeleeEnemy()
{
	SightRadius = 3500.f;
	HearingRange = 5000.f;
	MeleeRange = 150.f;
	MaxHealth = 150.f;
	Health = 150.f;
}

void AGASPMeleeEnemy::BeginPlay()
{
	Super::BeginPlay();

	AGASPAIController* AIController = Cast<AGASPAIController>(GetController());
	if (AIController && AIController->GetBlackboardComponent())
	{
		AIController->GetBlackboardComponent()->SetValueAsInt(FName("EnemyType"), 0);
		AIController->GetBlackboardComponent()->SetValueAsFloat(FName("PreferredRange"), MeleeRange);
	}
}