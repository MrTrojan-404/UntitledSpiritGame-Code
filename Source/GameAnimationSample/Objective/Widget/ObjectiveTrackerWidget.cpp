#include "ObjectiveTrackerWidget.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"
#include "GameAnimationSample/Objective/ObjectiveSubsystem.h"
#include "GameAnimationSample/Objective/ObjectiveActor.h"

void UObjectiveTrackerWidget::SetupTracking(AObjectiveActor* InObjective)
{
    TrackedObjective = InObjective;

    if (UObjectiveSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UObjectiveSubsystem>())
    {
        Subsystem->OnGlobalObjectiveStateChanged.AddDynamic(this, &UObjectiveTrackerWidget::HandleStateChanged);
        Subsystem->OnGlobalObjectiveProgressChanged.AddDynamic(this, &UObjectiveTrackerWidget::HandleProgressChanged);
    }
    RefreshUI();
}

void UObjectiveTrackerWidget::HandleStateChanged(AObjectiveActor* Objective)
{
    if (!Objective || !TrackedObjective) return;
    
    // Only refresh if OUR objective changed, or if a sub-task completed!
    if (Objective == TrackedObjective || Objective->bIsSubObjective)
    {
        RefreshUI();
    }
}

void UObjectiveTrackerWidget::HandleProgressChanged(AObjectiveActor* Objective)
{
    if (!Objective || !TrackedObjective) return;
    if (Objective == TrackedObjective || Objective->bIsSubObjective)
    {
        RefreshUI();
    }
}

void UObjectiveTrackerWidget::RefreshUI()
{
    if (!TrackedObjective) return;

    GetWorld()->GetTimerManager().ClearTimer(HideTimerHandle);
    GetWorld()->GetTimerManager().ClearTimer(ActiveCountdownTimerHandle);

    SetVisibility(ESlateVisibility::HitTestInvisible);

    if (MissionTitleText)
        MissionTitleText->SetText(TrackedObjective->MissionName); 

    if (TaskDescriptionText)
    {
        TaskDescriptionText->SetText(FText::FromString(TrackedObjective->GetFormattedTasks()));

        if (TrackedObjective->GetObjectiveState() == EObjectiveState::Completed)
            TaskDescriptionText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
        else if (TrackedObjective->GetObjectiveState() == EObjectiveState::Failed)
            TaskDescriptionText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
        else
            TaskDescriptionText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    }

    if (DynamicDataText)
    {
        if (TrackedObjective->ConditionData.ConditionType == EObjectiveCondition::Counter)
        {
            DynamicDataText->SetVisibility(ESlateVisibility::Visible);
            DynamicDataText->SetText(FText::FromString(TrackedObjective->GetProgressText()));
        }
        else if (TrackedObjective->ConditionData.ConditionType == EObjectiveCondition::Timed_Survive || 
                 TrackedObjective->ConditionData.ConditionType == EObjectiveCondition::Timed_Action)
        {
            DynamicDataText->SetVisibility(ESlateVisibility::Visible);
            DynamicDataText->SetText(FText::FromString(TrackedObjective->GetTimerText()));
            
            if (TrackedObjective->ConditionData.RemainingTime <= 10.0f)
                DynamicDataText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
            else
                DynamicDataText->SetColorAndOpacity(FSlateColor(FLinearColor::Yellow));

            if (TrackedObjective->GetObjectiveState() == EObjectiveState::Active)
            {
                GetWorld()->GetTimerManager().SetTimer(
                    ActiveCountdownTimerHandle, this, &UObjectiveTrackerWidget::UpdateCountdownText, 0.1f, true);
            }
        }
        else
        {
            DynamicDataText->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    // Only delete the widget if explicitly Completed or Failed!
    // This stops Checkpoint Restores from accidentally wiping the UI.
    EObjectiveState CurrentState = TrackedObjective->GetObjectiveState();
    
    // INSTANTLY DESTROY IF REWOUND TO INACTIVE
    if (CurrentState == EObjectiveState::Inactive)
    {
        RemoveFromParent();
        return; 
    }

    if (CurrentState == EObjectiveState::Completed || CurrentState == EObjectiveState::Failed)
    {
        GetWorld()->GetTimerManager().SetTimer(
            HideTimerHandle,
            this,
            &UObjectiveTrackerWidget::RemoveFromParent, 
            3.0f, false
        );
    }
}

void UObjectiveTrackerWidget::UpdateCountdownText()
{
    if (TrackedObjective)
    {
        DynamicDataText->SetText(FText::FromString(TrackedObjective->GetTimerText()));
        if (TrackedObjective->ConditionData.RemainingTime <= 10.0f)
            DynamicDataText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
        else
            DynamicDataText->SetColorAndOpacity(FSlateColor(FLinearColor::Yellow));
    }
}