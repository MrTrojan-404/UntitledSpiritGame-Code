#include "BTTask_GetNextWaypoint.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Components/SplineComponent.h"
#include "GameAnimationSample/GameAnimationSample.h"
#include "GameAnimationSample/AI/Character/GASPEnemyBase.h"
#include "GameAnimationSample/AI/Waypoint/GASPWaypointPath.h"

UBTTask_GetNextWaypoint::UBTTask_GetNextWaypoint()
{
    NodeName = TEXT("Get Next Waypoint");

    // Filter keys to correct types
    WaypointIndexKey.AddIntFilter(
        this, GET_MEMBER_NAME_CHECKED(
            UBTTask_GetNextWaypoint, WaypointIndexKey));

    PingPongForwardKey.AddBoolFilter(
        this, GET_MEMBER_NAME_CHECKED(
            UBTTask_GetNextWaypoint, PingPongForwardKey));

    TargetLocationKey.AddVectorFilter(
        this, GET_MEMBER_NAME_CHECKED(
            UBTTask_GetNextWaypoint, TargetLocationKey));

    WaitTimeKey.AddFloatFilter(
        this, GET_MEMBER_NAME_CHECKED(
            UBTTask_GetNextWaypoint, WaitTimeKey));
}

EBTNodeResult::Type UBTTask_GetNextWaypoint::ExecuteTask(
    UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory)
{
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return EBTNodeResult::Failed;

    AAIController* Controller = OwnerComp.GetAIOwner();
    if (!Controller) return EBTNodeResult::Failed;

    AGASPEnemyBase* Enemy = Cast<AGASPEnemyBase>(Controller->GetPawn());
    if (!Enemy) return EBTNodeResult::Failed;

    AGASPWaypointPath* Path = Enemy->WaypointPath;
    if (!Path || !Path->IsValid())
    {
        return EBTNodeResult::Failed;
    }
    
    int32 CurrentIndex = BB->GetValueAsInt(WaypointIndexKey.SelectedKeyName);
    bool bForward = BB->GetValueAsBool(PingPongForwardKey.SelectedKeyName);
    int32 NextIndex = Path->GetNextIndex(CurrentIndex, bForward);
    FVector NextLocation = Path->GetLocationAtIndex(NextIndex);

    if (NextLocation == FVector::ZeroVector)
    {
        return EBTNodeResult::Failed;
    }

    // Update blackboard...
    BB->SetValueAsInt(WaypointIndexKey.SelectedKeyName, NextIndex);
    BB->SetValueAsBool(PingPongForwardKey.SelectedKeyName, bForward);
    BB->SetValueAsVector(TargetLocationKey.SelectedKeyName, NextLocation);

    FWaypointSplinePoint PointData = Path->GetPointData(NextIndex);
    
    BB->SetValueAsFloat(WaitTimeKey.SelectedKeyName, PointData.WaitTime);
    
    return EBTNodeResult::Succeeded;
}

FString UBTTask_GetNextWaypoint::GetStaticDescription() const
{
    return TEXT("Gets next waypoint location from enemy path.\n"
                 "Updates WaypointIndex and TargetLocation.");
}