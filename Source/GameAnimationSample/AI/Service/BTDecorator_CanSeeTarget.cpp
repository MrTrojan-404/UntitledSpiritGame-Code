#include "BTDecorator_CanSeeTarget.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTDecorator_CanSeeTarget::UBTDecorator_CanSeeTarget()
{
    NodeName = TEXT("Can See Target");

    // Aborts self if condition changes
    FlowAbortMode = EBTFlowAbortMode::Self;

    TargetActorKey.AddObjectFilter(
        this,
        GET_MEMBER_NAME_CHECKED(
            UBTDecorator_CanSeeTarget, TargetActorKey),
        AActor::StaticClass());

    CanSeeTargetKey.AddBoolFilter(
        this,
        GET_MEMBER_NAME_CHECKED(
            UBTDecorator_CanSeeTarget, CanSeeTargetKey));

    IsInGracePeriodKey.AddBoolFilter(
        this,
        GET_MEMBER_NAME_CHECKED(
            UBTDecorator_CanSeeTarget, IsInGracePeriodKey));
}

bool UBTDecorator_CanSeeTarget::CalculateRawConditionValue(
    UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory) const
{
    UBlackboardComponent* BB =
        OwnerComp.GetBlackboardComponent();
    if (!BB) return false;

    // Has a target
    AActor* Target = Cast<AActor>(
        BB->GetValueAsObject(
            TargetActorKey.SelectedKeyName));
    if (!Target) return false;

    // Can see it OR still in grace period
    bool bCanSee = BB->GetValueAsBool(
        CanSeeTargetKey.SelectedKeyName);
    bool bInGrace = BB->GetValueAsBool(
        IsInGracePeriodKey.SelectedKeyName);

    return bCanSee || bInGrace;
}

FString UBTDecorator_CanSeeTarget::GetStaticDescription() const
{
    return TEXT("Returns true if AI can see target\n"
                 "or is within grace period after losing sight.");
}
