#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/AI/Character/GASPEnemyBase.h"
#include "GASPHeavyEnemy.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPHeavyEnemy : public AGASPEnemyBase
{
	GENERATED_BODY()

public:
	AGASPHeavyEnemy();

protected:
	virtual void BeginPlay() override;

	// Let's assign EnemyType 3 to the Heavy
	virtual int32 GetEnemyType() const override { return 3; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
	float HeavyEngagementRange = 1000.f;
};