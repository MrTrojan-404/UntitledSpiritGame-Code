#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GASPArenaManager.generated.h"

class AGASPBossBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossPhaseChanged);

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPArenaManager : public AActor
{
	GENERATED_BODY()
	
public:	
	AGASPArenaManager();
	
	// Called by the Boss Character when its health hits 0
	UFUNCTION(BlueprintCallable, Category = "Boss Arena")
	void AdvancePhase();

	UPROPERTY(BlueprintAssignable, Category = "Boss Arena")
	FOnBossPhaseChanged OnPhaseChanged;

	// Pass the player in so the Manager can tell the bosses who to kill
	UFUNCTION(BlueprintCallable, Category = "Boss Arena")
	void StartBossFight(AActor* InstigatorPlayer);
	
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditInstanceOnly, Category = "Boss Arena")
	int32 CurrentPhase = 0;
	
	// Blueprint Events so you can visually trigger Shields, Generators, and Cinematics!
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss Arena")
	void BlueprintStartFight();

	UFUNCTION(BlueprintImplementableEvent, Category = "Boss Arena")
	void BlueprintTriggerPhase(int32 PhaseIndex);

	// Array of all bosses in this specific arena (e.g., Commander 1 and Commander 2)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Boss Arena")
	TArray<AGASPBossBase*> ArenaBosses;
	
	// Drag and drop your Stage Generators into this array in the Editor
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Boss Arena")
	TArray<class AGASPStageGenerator*> StageGenerators;

	// The listener function
	UFUNCTION()
	void HandleGeneratorDisabled(AGASPStageGenerator* DisabledGenerator);

	// Easily restart all 4 generators from Blueprint during the Phase 2 Transition
	UFUNCTION(BlueprintCallable, Category = "Boss Arena")
	void ResetAllGenerators();

	int32 DisabledGenerators = 0;

	// Slot your giant stage shield here!
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Boss Arena")
	class AGASPStageShield* MainStageShield;

	// Drag and drop your Laser Turrets into this array in the Editor
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Boss Arena")
	TArray<class ALaserTurret*> InnerTurrets;
	
};