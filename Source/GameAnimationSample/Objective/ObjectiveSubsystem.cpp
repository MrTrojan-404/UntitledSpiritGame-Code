#include "ObjectiveSubsystem.h"

void UObjectiveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    AllObjectives.Empty();
    ActiveMainObjectiveID = NAME_None;
    CurrentMainObjectiveActor = nullptr; // Initialize the new pointer
}

void UObjectiveSubsystem::Deinitialize()
{
    AllObjectives.Empty();
    Super::Deinitialize();
}

void UObjectiveSubsystem::UpdateObjective(const FObjectiveInfo& Info)
{
    if (Info.ObjectiveID == NAME_None) return;

    const bool bAlreadyExists = AllObjectives.Contains(Info.ObjectiveID);
    const EObjectiveState PreviousState = bAlreadyExists
        ? AllObjectives[Info.ObjectiveID].State
        : EObjectiveState::Inactive;

    // Preserve local waypoint toggle if already registered
    FObjectiveInfo UpdatedInfo = Info;
    if (bAlreadyExists)
    {
        UpdatedInfo.bShowWaypoint = 
            AllObjectives[Info.ObjectiveID].bShowWaypoint;
    }

    AllObjectives.Emplace(Info.ObjectiveID, UpdatedInfo);

    // Handle state transitions
    if (Info.State != PreviousState)
    {
        if (Info.State == EObjectiveState::Completed)
        {
            OnObjectiveCompleted.Broadcast(UpdatedInfo);

            if (Info.ObjectiveID == ActiveMainObjectiveID)
            {
                ActiveMainObjectiveID = NAME_None;
            }
        }
        else if (Info.State == EObjectiveState::Failed)
        {
            OnObjectiveFailed.Broadcast(UpdatedInfo);

            if (Info.ObjectiveID == ActiveMainObjectiveID)
            {
                ActiveMainObjectiveID = NAME_None;
            }
        }
        else if (Info.State == EObjectiveState::Active
            && Info.ObjectiveType == EObjectiveType::Main)
        {
            ActiveMainObjectiveID = Info.ObjectiveID;
        }
    }

    BroadcastMainObjective();
    BroadcastOptionalObjectives();
    BroadcastWaypoints();
}

void UObjectiveSubsystem::SetWaypointVisible(
    FName ObjectiveID, bool bVisible)
{
    if (!AllObjectives.Contains(ObjectiveID)) return;

    AllObjectives[ObjectiveID].bShowWaypoint = bVisible;
    BroadcastWaypoints();
}

// THE FIX: Returns the Struct instead of the Actor Pointer!
FObjectiveInfo UObjectiveSubsystem::GetActiveMainObjective() const
{
    if (ActiveMainObjectiveID != NAME_None && AllObjectives.Contains(ActiveMainObjectiveID))
    {
        return AllObjectives[ActiveMainObjectiveID];
    }
    return FObjectiveInfo();
}

TArray<FObjectiveInfo> UObjectiveSubsystem::GetActiveOptionalObjectives() const
{
    TArray<FObjectiveInfo> Result;
    for (auto& Pair : AllObjectives)
    {
        if (Pair.Value.ObjectiveType == EObjectiveType::Optional
            && Pair.Value.State == EObjectiveState::Active)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

TArray<FObjectiveInfo> UObjectiveSubsystem::GetAllObjectives() const
{
    TArray<FObjectiveInfo> Result;
    AllObjectives.GenerateValueArray(Result);
    return Result;
}

TArray<FObjectiveInfo> UObjectiveSubsystem::GetActiveWaypoints() const
{
    TArray<FObjectiveInfo> Result;
    for (auto& Pair : AllObjectives)
    {
        if (Pair.Value.State == EObjectiveState::Active
            && Pair.Value.bShowWaypoint)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

void UObjectiveSubsystem::BroadcastMainObjective()
{
    OnActiveObjectiveChanged.Broadcast(GetActiveMainObjective());
}

void UObjectiveSubsystem::BroadcastOptionalObjectives()
{
    OnOptionalObjectivesChanged.Broadcast(
        GetActiveOptionalObjectives());
}

void UObjectiveSubsystem::BroadcastWaypoints()
{
    OnWaypointsUpdated.Broadcast(GetActiveWaypoints());
}

void UObjectiveSubsystem::ReportObjectiveEvent(FName EventTag)
{
    OnObjectiveEventReported.Broadcast(EventTag);
}