#pragma once

#include "CoreMinimal.h"
#include "GASPRangedEnemy.h"
#include "GASPSniperEnemy.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPSniperEnemy 
	: public AGASPRangedEnemy
{
	GENERATED_BODY()

public:
	AGASPSniperEnemy();

	// How long sniper aims before firing
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "AI|Combat")
	float AimDuration = 1.5f;

	// How long sniper waits between shots
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "AI|Combat")
	float ShotCooldown = 3.f;

	// Max range sniper engages at
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "AI|Combat")
	float SniperRange = 3000.f;

protected:
	virtual void BeginPlay() override;

	virtual int32 GetEnemyType() const override { return 2; }

};