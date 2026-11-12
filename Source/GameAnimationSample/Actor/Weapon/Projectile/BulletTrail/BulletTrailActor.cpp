#include "BulletTrailActor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

ABulletTrailActor::ABulletTrailActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false; // pure cosmetic, no network overhead

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ABulletTrailActor::InitTrail(UNiagaraSystem* InSystem, const FVector& Start, const FVector& End, float InSpeed, bool bStartFrozen)
{
	SetActorLocation(Start);
	TargetEnd = End;
	Speed     = InSpeed;
	bActive   = !bStartFrozen;
	bWaitingToTravel = bStartFrozen;

	if (InSystem)
	{
		TrailComp = UNiagaraFunctionLibrary::SpawnSystemAttached(InSystem, GetRootComponent(), NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, false);
	}

	if (!bStartFrozen)
	{
		SetLifeSpan(3.f);
	}
}

void ABulletTrailActor::StartTravel()
{
	if (!bWaitingToTravel) return;
	bWaitingToTravel = false;
	bActive = true;
	SetLifeSpan(3.f);
}
void ABulletTrailActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bActive) return;

	const FVector Current  = GetActorLocation();
	const FVector ToEnd    = TargetEnd - Current;
	const float   DistLeft = ToEnd.Size();
	const float   Step     = Speed * DeltaTime;

	if (Step >= DistLeft)
	{
		// Reached the impact point — stop emitting, let existing particles fade
		SetActorLocation(TargetEnd);
		bActive = false;

		if (TrailComp)
		{
			TrailComp->Deactivate();
		}
		return;
	}

	SetActorLocation(Current + ToEnd.GetSafeNormal() * Step);
}