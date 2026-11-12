#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ObjectiveTriggerComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class GAMEANIMATIONSAMPLE_API UObjectiveTriggerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UObjectiveTriggerComponent();

	// The tag this component shouts when triggered (e.g., "EnemyKilled")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FName EventTag = NAME_None;

	// Call this from Blueprint when the AI dies or switch is pressed
	UFUNCTION(BlueprintCallable, Category = "Objective")
	void TriggerEvent();
};