#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ObjectiveTypes.h"
#include "ObjectiveSubsystem.generated.h"

class AObjectiveActor;

// Broadcasts when active main objective changes
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActiveObjectiveChanged,
    const FObjectiveInfo&, ObjectiveInfo);

// Broadcasts when active optional objectives change
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOptionalObjectivesChanged,
    const TArray<FObjectiveInfo>&, OptionalObjectives);

// Broadcasts when objective completes
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveCompleted,
    const FObjectiveInfo&, CompletedObjective);

// Broadcasts when objective fails
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveFailed,
    const FObjectiveInfo&, FailedObjective);

// Broadcasts when waypoint data changes
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaypointsUpdated,
    const TArray<FObjectiveInfo>&, ActiveWaypoints);

// Broadcasts a generic event string to all listening objectives
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveEventReported, FName, EventTag);

// Broadcasts when an objective starts, completes, or fails (Global UI Delegate)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGlobalObjectiveStateChanged, AObjectiveActor*, Objective);

// Broadcasts when a counter updates (Global UI Delegate)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGlobalObjectiveProgressChanged, AObjectiveActor*, Objective);

UCLASS()
class GAMEANIMATIONSAMPLE_API UObjectiveSubsystem 
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(
        FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Called by AObjectiveActor on state change
    UFUNCTION(BlueprintCallable, Category = "Objectives")
    void UpdateObjective(const FObjectiveInfo& Info);

    // Called by objective menu to toggle waypoint
    UFUNCTION(BlueprintCallable, Category = "Objectives")
    void SetWaypointVisible(FName ObjectiveID, bool bVisible);

    // THE FIX: Returns the Struct!
    UFUNCTION(BlueprintCallable, Category = "Objectives")
    FObjectiveInfo GetActiveMainObjective() const;

    // THE FIX: Added the cached Actor pointer for the UI to read!
    UPROPERTY(BlueprintReadOnly, Category = "Objectives")
    AObjectiveActor* CurrentMainObjectiveActor = nullptr;

    // Returns all active optional objectives
    UFUNCTION(BlueprintCallable, Category = "Objectives")
    TArray<FObjectiveInfo> GetActiveOptionalObjectives() const;

    // Returns all objectives for log menu
    UFUNCTION(BlueprintCallable, Category = "Objectives")
    TArray<FObjectiveInfo> GetAllObjectives() const;

    // Returns all objectives with waypoints enabled
    UFUNCTION(BlueprintCallable, Category = "Objectives")
    TArray<FObjectiveInfo> GetActiveWaypoints() const;

    UFUNCTION(BlueprintCallable, Category = "Objectives")
    void ReportObjectiveEvent(FName EventTag);

    // HUD binds to these
    UPROPERTY(BlueprintAssignable, Category = "Objectives")
    FOnActiveObjectiveChanged OnActiveObjectiveChanged;

    UPROPERTY(BlueprintAssignable, Category = "Objectives")
    FOnOptionalObjectivesChanged OnOptionalObjectivesChanged;

    UPROPERTY(BlueprintAssignable, Category = "Objectives")
    FOnObjectiveCompleted OnObjectiveCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Objectives")
    FOnObjectiveFailed OnObjectiveFailed;

    UPROPERTY(BlueprintAssignable, Category = "Objectives")
    FOnWaypointsUpdated OnWaypointsUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Objectives")
    FOnObjectiveEventReported OnObjectiveEventReported;

    UPROPERTY(BlueprintAssignable, Category = "Objectives|Events")
    FOnGlobalObjectiveStateChanged OnGlobalObjectiveStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Objectives|Events")
    FOnGlobalObjectiveProgressChanged OnGlobalObjectiveProgressChanged;
    
private:
    // All known objectives — keyed by ObjectiveID
    TMap<FName, FObjectiveInfo> AllObjectives;

    // Cached active main objective ID
    FName ActiveMainObjectiveID = NAME_None;

    void BroadcastMainObjective();
    void BroadcastOptionalObjectives();
    void BroadcastWaypoints();
};