#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BulletTrailActor.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

UCLASS(NotBlueprintable, NotPlaceable)
class GAMEANIMATIONSAMPLE_API ABulletTrailActor : public AActor
{
	GENERATED_BODY()

public:
	ABulletTrailActor();

	// Call immediately after spawning
	void InitTrail(UNiagaraSystem* InSystem, const FVector& Start, const FVector& End, float InSpeed = 15000.f, bool bStartFrozen = false);
	void StartTravel();
	
protected:
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY()
	UNiagaraComponent* TrailComp = nullptr;

	FVector TargetEnd   = FVector::ZeroVector;
	float   Speed       = 15000.f;
	bool    bActive     = false;

	bool bWaitingToTravel = false;
};