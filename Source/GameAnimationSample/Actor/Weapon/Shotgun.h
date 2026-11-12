#pragma once

#include "CoreMinimal.h"
#include "HitScanWeapon.h"
#include "Shotgun.generated.h"

USTRUCT(BlueprintType)
struct FBufferedShotgunShot
{
	GENERATED_BODY()

	UPROPERTY() TArray<FHitResult> HitResults;
	UPROPERTY() TArray<FVector_NetQuantize100> HitTargets;
	UPROPERTY() FVector TraceStart;
	UPROPERTY() AController* InstigatorController;
	UPROPERTY() TArray<TWeakObjectPtr<ABulletTrailActor>> TrailActors;

	// Reprojection data — index-parallel to HitResults, one entry per pellet
	UPROPERTY() TArray<TWeakObjectPtr<USkeletalMeshComponent>> HitMeshComps;
	UPROPERTY() TArray<FName> HitBoneNames;
	UPROPERTY() TArray<FVector> LocalHitOffsets;
};
UCLASS()
class GAMEANIMATIONSAMPLE_API AShotgun : public AHitScanWeapon
{
	GENERATED_BODY()

public:
	virtual void FireShotgun(const TArray<FVector_NetQuantize100>& HitTargets);
	void ShotgunTraceEndWithScatter(const FVector& HitTarget, TArray<FVector_NetQuantize100>& HitTargets, float SpreadMultiplier = 1.0f);

	virtual void ReleaseBufferedShots() override;

protected:
	UPROPERTY()
	TArray<FBufferedShotgunShot> BufferedShotgunShots;

	void ProcessShotgunImpacts(const TArray<FHitResult>& HitResults, const TArray<FVector_NetQuantize100>& HitTargets, const FVector& Start, AController* InstigatorController);

private:
	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	uint32 NumberOfPellets = 10;
};