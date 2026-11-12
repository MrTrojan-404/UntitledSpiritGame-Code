#pragma once

#include "CoreMinimal.h"
#include "ObjectiveTypes.generated.h"

UENUM(BlueprintType)
enum class EObjectiveState : uint8
{
    Inactive        UMETA(DisplayName = "Inactive"),
    Active          UMETA(DisplayName = "Active"),
    Completed       UMETA(DisplayName = "Completed"),
    Failed          UMETA(DisplayName = "Failed")
};

UENUM(BlueprintType)
enum class EObjectiveType : uint8
{
    Main            UMETA(DisplayName = "Main"),
    Optional        UMETA(DisplayName = "Optional")
};

UENUM(BlueprintType)
enum class EObjectiveCondition : uint8
{
    // Player enters trigger volume
    Overlap         UMETA(DisplayName = "Overlap"),

    // Reach a count threshold
    // e.g kill 5 enemies, collect 3 items
    Counter         UMETA(DisplayName = "Counter"),

    // Another actor explicitly calls Complete()
    // e.g scripted event, cutscene, door opened
    External        UMETA(DisplayName = "External"),

    // Countdown reaches zero = Complete
    // e.g survive for X seconds
    Timed_Survive   UMETA(DisplayName = "Timed - Survive"),

    // Countdown reaches zero = Fail
    // e.g defuse bomb in X seconds
    Timed_Action    UMETA(DisplayName = "Timed - Action"),

    // All sub conditions must be met
    Composite       UMETA(DisplayName = "Composite")
};

USTRUCT(BlueprintType)
struct FObjectiveConditionData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EObjectiveCondition ConditionType = EObjectiveCondition::External;

    // Counter condition
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "ConditionType == EObjectiveCondition::Counter", EditConditionHides))
    int32 RequiredCount = 1;

    // Timed conditions
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "ConditionType == EObjectiveCondition::Timed_Survive || ConditionType == EObjectiveCondition::Timed_Action", EditConditionHides))
    float TimeLimit = 30.f;

    // Runtime only — not edited
    // replicated so all clients show correct count/time
    UPROPERTY(BlueprintReadOnly)
    int32 CurrentCount = 0;

    UPROPERTY(BlueprintReadOnly)
    float RemainingTime = 0.f;

    // The tag this objective listens for (e.g., "EnemyKilled")
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "ConditionType == EObjectiveCondition::Counter || ConditionType == EObjectiveCondition::External", EditConditionHides))
    FName EventTag = NAME_None;
};

USTRUCT(BlueprintType)
struct FObjectiveInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ObjectiveID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText DisplayText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EObjectiveType ObjectiveType = EObjectiveType::Main;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EObjectiveState State = EObjectiveState::Inactive;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector WorldLocation = FVector::ZeroVector;

    // Condition data for HUD display
    // e.g remaining time or current count
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FObjectiveConditionData ConditionData;

    // Local client only — changed via objective menu
    bool bShowWaypoint = true;

    bool operator==(const FObjectiveInfo& Other) const
    {
        return ObjectiveID == Other.ObjectiveID;
    }
};