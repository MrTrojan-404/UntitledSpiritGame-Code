// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerOverheadNameWidget.h"

#include "Components/TextBlock.h"

void UPlayerOverheadNameWidget::SetDisplayText(FString TextToDisplay)
{
	PlayerName->SetText(FText::FromString(TextToDisplay));
}

void UPlayerOverheadNameWidget::NativeDestruct()
{
	Super::NativeDestruct();
}