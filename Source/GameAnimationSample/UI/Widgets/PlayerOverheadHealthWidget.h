// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerOverheadHealthWidget.generated.h"

class UProgressBar;
/**
 * 
 */
UCLASS()
class GAMEANIMATIONSAMPLE_API UPlayerOverheadHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void UpdateHealthBar(float Health, float MaxHealth);


protected:	
	virtual void NativeDestruct() override;

private:
	UPROPERTY(meta = (BindWidget))
	UProgressBar* HealthBar;
};
