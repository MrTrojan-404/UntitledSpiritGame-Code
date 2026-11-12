#include "DirectionalIndicatorWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UDirectionalIndicatorWidget::UpdateVisuals(float DetectionLevel, uint8 AlertPhaseRaw)
{
	// If the enemy dies or loses aggro, literally delete this UI from the screen.
	if (DetectionLevel <= 0.01f && AlertPhaseRaw == 0)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::Visible);
	
	if (DetectionBar)
	{
		DetectionBar->SetPercent(DetectionLevel);

		FLinearColor BarColor;
		switch (AlertPhaseRaw)
		{
		case 1:  BarColor = FLinearColor(1.f, 0.85f, 0.f, 1.f);  break; // Suspicious (Yellow)
		case 2:  BarColor = FLinearColor(1.f, 0.45f, 0.f, 1.f);  break; // Caution (Orange)
		case 3:  BarColor = FLinearColor(1.f, 0.05f, 0.05f, 1.f); break; // Alert (Red)
		default: BarColor = FLinearColor(0.7f, 0.7f, 0.7f, 1.f);  break; // Unaware (Grey)
		}
		DetectionBar->SetFillColorAndOpacity(BarColor);
	}

	if (PhaseIndicator)
	{
		FString PhaseText;
		switch (AlertPhaseRaw)
		{
		case 1:  PhaseText = TEXT("?");   break; 
		case 2:  PhaseText = TEXT("!");   break; 
		case 3:  PhaseText = TEXT("!");   break; 
		default: PhaseText = TEXT("");    break; 
		}

		PhaseIndicator->SetText(FText::FromString(PhaseText));
		PhaseIndicator->SetVisibility(PhaseText.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::Visible);

		FSlateColor TextColor;
		switch (AlertPhaseRaw)
		{
		case 1:  TextColor = FSlateColor(FLinearColor(1.f, 0.85f, 0.f)); break;
		case 2:  TextColor = FSlateColor(FLinearColor(1.f, 0.45f, 0.f)); break;
		case 3:  TextColor = FSlateColor(FLinearColor::Red);             break;
		default: TextColor = FSlateColor(FLinearColor::White);           break;
		}
		PhaseIndicator->SetColorAndOpacity(TextColor);
	}
}