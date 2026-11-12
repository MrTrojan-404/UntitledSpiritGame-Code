#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ObjectiveListWidget.generated.h"

class UVerticalBox;
class AObjectiveActor;

UCLASS()
class GAMEANIMATIONSAMPLE_API UObjectiveListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	// The invisible box that holds all the cards
	UPROPERTY(meta = (BindWidget))
	UVerticalBox* ObjectiveContainer;

	// We select the Blueprint design of our Card in the Editor
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<class UObjectiveTrackerWidget> EntryWidgetClass;

protected:
	UFUNCTION()
	void HandleStateChanged(AObjectiveActor* Objective);
};