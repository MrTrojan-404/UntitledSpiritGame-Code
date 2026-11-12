#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameAnimationSample/Dialogue/DialogueSequenceData.h"
#include "GASPWaypointPath.generated.h"

class USplineComponent;

USTRUCT(BlueprintType)
struct FWaypointSplinePoint
{
    GENERATED_BODY()

    // How long AI waits at this point
    // 0 means no wait
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float WaitTime = 0.f;

    // Optional dialogue when AI reaches this point
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    UDialogueSequenceData* ArrivalDialogue = nullptr;
};

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPWaypointPath : public AActor
{
    GENERATED_BODY()

public:
    AGASPWaypointPath();

    // True  — ping pong A→B→C→B→A
    // False — loop A→B→C→A
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waypoint")
    bool bPingPong = false;

    // Per point data — must match spline point count
    // Index N corresponds to spline point N
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waypoint")
    TArray<FWaypointSplinePoint> PointData;

    // Get world location of spline point at index
    FVector GetLocationAtIndex(int32 Index) const;

    // Get point data at index — returns default if out of range
    FWaypointSplinePoint GetPointData(int32 Index) const;

    // Get next index given current and direction
    // Modifies bForward for ping pong
    int32 GetNextIndex(int32 CurrentIndex, bool& bForward) const;

    int32 Num() const;
    bool IsValid() const;
    FORCEINLINE USplineComponent* GetSpline() const 
    { 
        return Spline; 
    }

protected:
    virtual void BeginPlay() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(
        FPropertyChangedEvent& PropertyChangedEvent) override;

    // Called when spline is modified in viewport
    virtual void PostEditMove(bool bFinished) override;
#endif

private:
    UPROPERTY(VisibleAnywhere, Category = "Waypoint", meta = (AllowPrivateAccess = "true"))
    USplineComponent* Spline = nullptr;

#if WITH_EDITOR
    void SyncPointDataToSpline();
#endif
};