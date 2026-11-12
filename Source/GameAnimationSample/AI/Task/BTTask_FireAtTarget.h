#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FireAtTarget.generated.h"

class UCombatComponent;

UCLASS()
class GAMEANIMATIONSAMPLE_API UBTTask_FireAtTarget 
	: public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FireAtTarget();

	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	virtual void TickTask(
	   UBehaviorTreeComponent& OwnerComp, 
	   uint8* NodeMemory, 
	   float DeltaSeconds) override;
	
	virtual FString GetStaticDescription() const override;

	
protected:
	virtual EBTNodeResult::Type AbortTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
	
private:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	// The default aim time for normal grunts (Snipers will override this via the Blackboard)
	UPROPERTY(EditAnywhere, Category = "Fire")
	float DefaultAimDelay = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Fire")
	float FireDuration = 1.5f;

	// Allows us to turn off the hardcoded bursts for Flamethrowers!
	UPROPERTY(EditAnywhere, Category = "Fire")
	bool bUseDynamicBurst = true;
	
	FTimerHandle AimTimer;
	FTimerHandle FireStopTimer;

	UPROPERTY()
	UCombatComponent* CachedCombat = nullptr;

	UPROPERTY()
	class AGASPBaseCharacter* CachedAICharacter = nullptr;
    
	UBehaviorTreeComponent* CachedOwnerComp = nullptr;

	void BeginFire();
	void StopFiring();

	FVector CurrentBurstError = FVector::ZeroVector;
};