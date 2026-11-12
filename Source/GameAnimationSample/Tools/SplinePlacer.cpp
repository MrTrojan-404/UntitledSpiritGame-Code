#include "SplinePlacer.h"
#include "Components/SplineComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"

ASplinePlacer::ASplinePlacer()
{
    PrimaryActorTick.bCanEverTick = false;

    SplineComp = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComp"));
    RootComponent = SplineComp;

    InstancedMeshComp = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InstancedMeshComp"));
    InstancedMeshComp->SetupAttachment(RootComponent);
}

void ASplinePlacer::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // Safety checks
    if (!InstancedMeshComp->GetStaticMesh()) return;
    
    InstancedMeshComp->ClearInstances();

    float StepDistance = MeshLength + SpacingGap;
    if (StepDistance <= 0.0f) return;

    float TotalLength = SplineComp->GetSplineLength();
    int32 InstanceCount = FMath::TruncToInt(TotalLength / StepDistance);

    for (int32 i = 0; i <= InstanceCount; ++i)
    {
        float Distance = i * StepDistance;
        
        FTransform SplineTransform = SplineComp->GetTransformAtDistanceAlongSpline(
            Distance, 
            ESplineCoordinateSpace::World
        );

        // Terrain Snapping & Alignment Logic
        if (bSnapToTerrain && GetWorld())
        {
            FVector StartLoc = SplineTransform.GetLocation();
            FVector EndLoc = StartLoc - FVector(0.0f, 0.0f, TraceDistance);
            
            FHitResult HitResult;
            FCollisionQueryParams TraceParams;
            TraceParams.AddIgnoredActor(this);

            if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLoc, EndLoc, ECC_Visibility, TraceParams))
            {
                // 1. Snap Z Location
                FVector SnappedLocation = SplineTransform.GetLocation();
                SnappedLocation.Z = HitResult.Location.Z;
                SplineTransform.SetLocation(SnappedLocation);

                // 2. Align Rotation to Slope
                if (bAlignToTerrainSlope)
                {
                    FVector SplineForward = SplineTransform.GetRotation().GetForwardVector();
                    
                    // Creates a new rotation using the ground's normal as UP, and the spline's direction as FORWARD
                    FRotator SlopeRotation = FRotationMatrix::MakeFromZX(HitResult.ImpactNormal, SplineForward).Rotator();
                    SplineTransform.SetRotation(SlopeRotation.Quaternion());
                }
            }
        }

        // Apply manual offsets
        FTransform OffsetTransform(RotationOffset, PositionOffset, FVector::OneVector);
        FTransform FinalTransform = OffsetTransform * SplineTransform;
        
        InstancedMeshComp->AddInstance(FinalTransform, true);
    }
}