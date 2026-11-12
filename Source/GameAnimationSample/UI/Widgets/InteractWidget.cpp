// Fill out your copyright notice in the Description page of Project Settings.


#include "InteractWidget.h"

#include "Components/TextBlock.h"

void UInteractWidget::SetDisplayText(FString TextToDisplay)
{
	if (DisplayText)
	{
		DisplayText->SetText(FText::FromString(TextToDisplay));
	}
}


void UInteractWidget::NativeDestruct()
{
	RemoveFromParent();
	
	Super::NativeDestruct();
	
}


void UInteractWidget::SetTextColor(FLinearColor Color)
{
	// Make sure to use the exact name of your UTextBlock variable here!
	if (DisplayText) 
	{
		DisplayText->SetColorAndOpacity(FSlateColor(Color));
	}
}