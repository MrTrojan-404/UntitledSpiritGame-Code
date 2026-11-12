#include "BTTask_FireAtTarget.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

#include "GameAnimationSample/AI/Character/GASPAICharacterBase.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/Component/CombatComponent.h"

UBTTask_FireAtTarget::UBTTask_FireAtTarget()
{
    NodeName = TEXT("Fire At Target");
    bNotifyTick = true;
    bCreateNodeInstance = true; 
    
    TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FireAtTarget, TargetActorKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_FireAtTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return EBTNodeResult::Failed;

    // Never shoot if trapped
    if (BB && (BB->GetValueAsBool(AGASPAIController::BB_bIsFrozen) || BB->GetValueAsBool(AGASPAIController::BB_bIsCaged) || BB->GetValueAsBool(AGASPAIController::BB_bIsInRagdoll)))
    {
        return EBTNodeResult::Failed; 
    }
    
    AAIController* Controller = OwnerComp.GetAIOwner();
    if (!Controller) return EBTNodeResult::Failed;

    AGASPBaseCharacter* AICharacter = Cast<AGASPBaseCharacter>(Controller->GetPawn());
    if (!AICharacter) return EBTNodeResult::Failed;

    AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
    if (!Target) return EBTNodeResult::Failed;

    UCombatComponent* Combat = AICharacter->GetCombatComponent();
    if (!Combat || !Combat->GetPrimaryWeapon() || Combat->GetPrimaryWeapon()->IsEmpty())
    {
        return EBTNodeResult::Failed;
    }

    // Cache everything
    CachedCombat = Combat;
    CachedOwnerComp = &OwnerComp;
    CachedAICharacter = AICharacter;

    // Reset error so they track perfectly while aiming
    CurrentBurstError = FVector::ZeroVector;

    // Face the target instantly to begin tracking
    float AimZOffset = 30.f;
    if (AGASPAICharacterBase* AsAI = Cast<AGASPAICharacterBase>(AICharacter)) AimZOffset = AsAI->AimZOffset;
    
    FVector TargetLocation = Target->GetActorLocation();
    TargetLocation.Z += AimZOffset;
    Combat->SetHitTarget(TargetLocation);
    FVector Direction = (TargetLocation - AICharacter->GetActorLocation()).GetSafeNormal();
    Controller->SetControlRotation(Direction.Rotation());

    // START AIMING (Raises weapon)
    Combat->SetAiming(true);

    // FETCH DELAY: Use Sniper's custom duration if it exists, otherwise use Grunt default
    float AimDelay = DefaultAimDelay;
    if (BB->IsValidKey(BB->GetKeyID(FName("AimDuration"))))
    {
        float BBAimDelay = BB->GetValueAsFloat(FName("AimDuration"));
		
        // ONLY override the DefaultAimDelay if the Blackboard value is actually set!
        if (BBAimDelay > 0.f)
        {
            AimDelay = BBAimDelay;
        }
    }
    if (AimDelay <= 0.05f) AimDelay = 0.05f;

    // Wait for the Aiming Phase to finish before firing!
    AICharacter->GetWorldTimerManager().SetTimer(AimTimer, this, &UBTTask_FireAtTarget::BeginFire, AimDelay, false);

    return EBTNodeResult::InProgress;
}

void UBTTask_FireAtTarget::BeginFire()
{
    if (!CachedCombat || !CachedOwnerComp || !CachedAICharacter) return;

    UBlackboardComponent* BB = CachedOwnerComp->GetBlackboardComponent();
    AActor* Target = BB ? Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;
    if (!Target)
    {
        StopFiring();
        return;
    }

    // 1. Generate Error Mistake right as they pull the trigger
    float AimErrorRad = 30.f;
    if (AGASPAICharacterBase* AsAI = Cast<AGASPAICharacterBase>(CachedAICharacter)) AimErrorRad = AsAI->AimErrorRadius;
    CurrentBurstError = FMath::VRand() * FMath::RandRange(0.f, AimErrorRad);

    // 2. THE FIX: Dynamic Burst Logic respects the boolean!
    float CalculatedFireDuration = FireDuration;

    if (bUseDynamicBurst)
    {
        float DistanceToTarget = FVector::Dist(CachedAICharacter->GetActorLocation(), Target->GetActorLocation());
        if (DistanceToTarget < 600.f) CalculatedFireDuration = FMath::RandRange(1.5f, 3.0f);
        else CalculatedFireDuration = FMath::RandRange(0.3f, 0.6f);
    }

    // 3. START FIRING
    CachedCombat->HandleFire(true);

    // 4. Schedule the Stop Firing event
    CachedAICharacter->GetWorldTimerManager().SetTimer(FireStopTimer, this, &UBTTask_FireAtTarget::StopFiring, CalculatedFireDuration, false);
}

void UBTTask_FireAtTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

    if (!CachedCombat || !CachedAICharacter) return;

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    AAIController* Controller = OwnerComp.GetAIOwner();
    
    if (BB && Controller)
    {
        AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));

        if (Target)
        {
            float AimZOffset = 30.f;
            if (AGASPAICharacterBase* AsAI = Cast<AGASPAICharacterBase>(CachedAICharacter)) AimZOffset = AsAI->AimZOffset;

            // Track Target Location continuously
            FVector TargetLocation = Target->GetActorLocation();
            TargetLocation.Z += AimZOffset;
            TargetLocation += CurrentBurstError; // Error is 0,0,0 while aiming, then shifts when firing!

            CachedCombat->SetHitTarget(TargetLocation);

            FVector Direction = (TargetLocation - CachedAICharacter->GetActorLocation()).GetSafeNormal();
            Controller->SetControlRotation(Direction.Rotation());
        }
    }
}

void UBTTask_FireAtTarget::StopFiring()
{
    if (CachedCombat)
    {
        CachedCombat->SetAiming(false);
        CachedCombat->HandleFire(false);
        CachedCombat->SetHitTarget(FVector::ZeroVector);
        CachedCombat = nullptr;
    }

    if (CachedOwnerComp)
    {
        FinishLatentTask(*CachedOwnerComp, EBTNodeResult::Succeeded);
        CachedOwnerComp = nullptr;
    }
    CachedAICharacter = nullptr;
}

EBTNodeResult::Type UBTTask_FireAtTarget::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    if (CachedAICharacter)
    {
        CachedAICharacter->GetWorldTimerManager().ClearTimer(AimTimer);
        CachedAICharacter->GetWorldTimerManager().ClearTimer(FireStopTimer);
    }
    if (CachedCombat)
    {
        CachedCombat->SetAiming(false);
        CachedCombat->HandleFire(false);
        CachedCombat->SetHitTarget(FVector::ZeroVector);
    }
    CachedCombat = nullptr;
    CachedAICharacter = nullptr;
    CachedOwnerComp = nullptr;
    
    return EBTNodeResult::Aborted;
}

FString UBTTask_FireAtTarget::GetStaticDescription() const
{
    return TEXT("Aims for AimDuration (or 0.5s), then fires a burst.");
}