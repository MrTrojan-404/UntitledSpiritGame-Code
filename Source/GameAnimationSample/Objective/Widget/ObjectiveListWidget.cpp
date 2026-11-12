// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectiveListWidget.h"
#include "ObjectiveListWidget.h"
#include "Components/VerticalBox.h"
#include "GameAnimationSample/Objective/ObjectiveSubsystem.h"
#include "GameAnimationSample/Objective/ObjectiveActor.h"
#include "ObjectiveTrackerWidget.h"

void UObjectiveListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UObjectiveSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UObjectiveSubsystem>())
	{
		// Listen for new missions starting
		Subsystem->OnGlobalObjectiveStateChanged.AddDynamic(this, &UObjectiveListWidget::HandleStateChanged);

		// Catch up late joiners
		if (Subsystem->CurrentMainObjectiveActor)
		{
			HandleStateChanged(Subsystem->CurrentMainObjectiveActor);
		}
	}
}

void UObjectiveListWidget::HandleStateChanged(AObjectiveActor* Objective)
{
	// Ignore children; only spawn cards for the Main Boss
	if (!Objective || Objective->bIsSubObjective) return;

	if (Objective->GetObjectiveState() == EObjectiveState::Active)
	{
		if (ObjectiveContainer)
		{
			// THE FIX: Check if we are already tracking this exact objective!
			for (UWidget* ChildWidget : ObjectiveContainer->GetAllChildren())
			{
				UObjectiveTrackerWidget* ExistingCard = Cast<UObjectiveTrackerWidget>(ChildWidget);
               
				// If a card for this objective is already in the list, abort!
				if (ExistingCard && ExistingCard->TrackedObjective == Objective)
				{
					return; 
				}
			}
		}

		if (EntryWidgetClass && ObjectiveContainer)
		{
			// Spawn a brand new UI Card ONLY if we didn't find an existing one!
			UObjectiveTrackerWidget* NewEntry = CreateWidget<UObjectiveTrackerWidget>(this, EntryWidgetClass);
			if (NewEntry)
			{
				NewEntry->SetupTracking(Objective);
				ObjectiveContainer->AddChildToVerticalBox(NewEntry);
			}
		}
	}
}