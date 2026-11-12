#include "BTTask_BossMissileVolley.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/SpellAbility/Projectile/MicroMissileProjectile.h"

UBTTask_BossMissileVolley::UBTTask_BossMissileVolley()
{
	NodeName = TEXT("Boss Missile Volley");
	bCreateNodeInstance = true; 
}

EBTNodeResult::Type UBTTask_BossMissileVolley::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* AICon = OwnerComp.GetAIOwner();
	if (!BB || !AICon) return EBTNodeResult::Failed;

	CachedBoss = Cast<AGASPBaseCharacter>(AICon->GetPawn());
	CachedTarget = Cast<AGASPBaseCharacter>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
	CachedOwnerComp = &OwnerComp;

	if (!CachedBoss || !CachedTarget) return EBTNodeResult::Failed;

	CurrentMissileIndex = 0;

	// 1. Instantly face the Target
	FVector Direction = (CachedTarget->GetActorLocation() - CachedBoss->GetActorLocation()).GetSafeNormal();
	AICon->SetControlRotation(Direction.Rotation());

	// 2. Start the firing sequence
	CachedBoss->GetWorldTimerManager().SetTimer(LaunchTimer, this, &UBTTask_BossMissileVolley::FireNextMissile, InitialLaunchDelay, false);

	return EBTNodeResult::InProgress;
}

void UBTTask_BossMissileVolley::FireNextMissile()
{
	if (!CachedBoss || !CachedTarget || !MissileClass)
	{
		FinishVolley();
		return;
	}

	float HorizontalOffset = (CurrentMissileIndex % 2 == 0) ? -40.f : 40.f;
	FVector SpawnLoc = CachedBoss->GetActorLocation() + FVector(0.f, 0.f, 100.f) + (CachedBoss->GetActorForwardVector() * 30.f) + (CachedBoss->GetActorRightVector() * HorizontalOffset);
	
	FTransform SpawnTransform(CachedBoss->GetActorRotation(), SpawnLoc);
	AMicroMissileProjectile* Missile = CachedBoss->GetWorld()->SpawnActorDeferred<AMicroMissileProjectile>(MissileClass, SpawnTransform);

	if (Missile)
	{
		Missile->Initialize(CachedBoss, CachedTarget);
		Missile->FinishSpawning(SpawnTransform);
	}

	CurrentMissileIndex++;

	if (CurrentMissileIndex >= MissilesToFire)
	{
		CachedBoss->GetWorldTimerManager().SetTimer(LaunchTimer, this, &UBTTask_BossMissileVolley::FinishVolley, 1.0f, false);
	}
	else
	{
		CachedBoss->GetWorldTimerManager().SetTimer(LaunchTimer, this, &UBTTask_BossMissileVolley::FireNextMissile, TimeBetweenMissiles, false);
	}
}

void UBTTask_BossMissileVolley::FinishVolley()
{
	if (CachedOwnerComp)
	{
		FinishLatentTask(*CachedOwnerComp, EBTNodeResult::Succeeded);
	}
	CachedOwnerComp = nullptr;
	CachedBoss = nullptr;
	CachedTarget = nullptr;
}

EBTNodeResult::Type UBTTask_BossMissileVolley::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (CachedBoss)
	{
		CachedBoss->GetWorldTimerManager().ClearTimer(LaunchTimer);
	}
	CachedOwnerComp = nullptr;
	CachedBoss = nullptr;
	CachedTarget = nullptr;
	return EBTNodeResult::Aborted;
}