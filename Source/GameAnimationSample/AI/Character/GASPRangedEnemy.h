#pragma once

#include "CoreMinimal.h"
#include "GASPEnemyBase.h"
#include "GASPRangedEnemy.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPRangedEnemy : public AGASPEnemyBase
{
	GENERATED_BODY()

public:
	AGASPRangedEnemy();

	// Preferred distance to maintain from target
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "AI|Combat")
	float PreferredCombatDistance = 600.f;

	// Min distance before backing up
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "AI|Combat")
	float MinCombatDistance = 300.f;

protected:
	virtual void BeginPlay() override;

	virtual int32 GetEnemyType() const override { return 1; }

};