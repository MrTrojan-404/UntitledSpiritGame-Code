#pragma once

#include "CoreMinimal.h"
#include "GASPEnemyBase.h"
#include "GASPMeleeEnemy.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPMeleeEnemy : public AGASPEnemyBase
{
	GENERATED_BODY()

public:
	AGASPMeleeEnemy();

protected:
	virtual void BeginPlay() override;
	virtual int32 GetEnemyType() const override { return 0; }
};