#include "GASPWaypointPath.h"
#include "Components/SplineComponent.h"

AGASPWaypointPath::AGASPWaypointPath()
{
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(
        TEXT("Root"));

    Spline = CreateDefaultSubobject<USplineComponent>(
        TEXT("Spline"));
    Spline->SetupAttachment(RootComponent);

    // Default two points so spline is visible on placement
    Spline->SetSplinePoints(
        TArray<FVector>{ FVector::ZeroVector, FVector(300.f, 0.f, 0.f) },
        ESplineCoordinateSpace::Local,
        false
    );
}

void AGASPWaypointPath::BeginPlay()
{
    Super::BeginPlay();
    
    const int32 SplinePoints = Spline->GetNumberOfSplinePoints();
    while (PointData.Num() < SplinePoints)
    {
        PointData.Add(FWaypointSplinePoint());
    }
}

FVector AGASPWaypointPath::GetLocationAtIndex(int32 Index) const
{
    if (!Spline) return FVector::ZeroVector;
    if (Index < 0 || Index >= Spline->GetNumberOfSplinePoints())
        return FVector::ZeroVector;

    return Spline->GetLocationAtSplinePoint(
        Index, ESplineCoordinateSpace::World);
}

FWaypointSplinePoint AGASPWaypointPath::GetPointData(
    int32 Index) const
{
    if (!PointData.IsValidIndex(Index))
        return FWaypointSplinePoint();

    return PointData[Index];
}

int32 AGASPWaypointPath::GetNextIndex(
    int32 CurrentIndex, bool& bForward) const
{
    const int32 Count = Num();
    if (Count <= 1) return 0;

    if (bPingPong)
    {
        if (bForward)
        {
            int32 Next = CurrentIndex + 1;
            if (Next >= Count)
            {
                bForward = false;
                return CurrentIndex - 1;
            }
            return Next;
        }
        else
        {
            int32 Next = CurrentIndex - 1;
            if (Next < 0)
            {
                bForward = true;
                return 1;
            }
            return Next;
        }
    }
    else
    {
        return (CurrentIndex + 1) % Count;
    }
}

int32 AGASPWaypointPath::Num() const
{
    return Spline ? Spline->GetNumberOfSplinePoints() : 0;
}

bool AGASPWaypointPath::IsValid() const
{
    return Spline && Spline->GetNumberOfSplinePoints() > 0;
}

#if WITH_EDITOR

void AGASPWaypointPath::PostEditChangeProperty(
    FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    SyncPointDataToSpline();
}

void AGASPWaypointPath::PostEditMove(bool bFinished)
{
    Super::PostEditMove(bFinished);
    SyncPointDataToSpline();
}

void AGASPWaypointPath::SyncPointDataToSpline()
{
    if (!Spline) return;

    const int32 SplinePoints = Spline->GetNumberOfSplinePoints();

    // Add missing entries
    while (PointData.Num() < SplinePoints)
    {
        PointData.Add(FWaypointSplinePoint());
    }

    // Remove excess entries if spline points were deleted
    while (PointData.Num() > SplinePoints)
    {
        PointData.RemoveAt(PointData.Num() - 1);
    }
}

#endif