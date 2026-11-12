// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerOverheadHealthWidget.h"

#include "Components/ProgressBar.h"

void UPlayerOverheadHealthWidget::UpdateHealthBar(float Health, float MaxHealth)
{
	HealthBar->SetPercent(Health/MaxHealth);
}

void UPlayerOverheadHealthWidget::NativeDestruct()
{
	Super::NativeDestruct();
}
