#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ObjectiveTrackerWidget.generated.h"

class AObjectiveActor;

UCLASS()
class GAMEANIMATIONSAMPLE_API UObjectiveTrackerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Replaces NativeConstruct. The Container will call this when spawning the card!
	void SetupTracking(AObjectiveActor* InObjective);

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* MissionTitleText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TaskDescriptionText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* DynamicDataText;

	// The specific mission this UI Card is responsible for
	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	AObjectiveActor* TrackedObjective = nullptr;
	
protected:
	void RefreshUI();

	UFUNCTION()
	void HandleStateChanged(AObjectiveActor* Objective);

	UFUNCTION()
	void HandleProgressChanged(AObjectiveActor* Objective);

	UFUNCTION()
	void UpdateCountdownText();

	FTimerHandle HideTimerHandle;
	FTimerHandle ActiveCountdownTimerHandle;
	
};