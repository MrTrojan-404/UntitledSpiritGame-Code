#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"
#include "GASPBossController.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPBossController : public AGASPAIController
{
	GENERATED_BODY()

public:
	// Called by the Arena Manager to force the Boss to attack a specific player
	UFUNCTION(BlueprintCallable, Category = "Boss AI")
	void LockOntoTarget(AActor* NewTarget);
};