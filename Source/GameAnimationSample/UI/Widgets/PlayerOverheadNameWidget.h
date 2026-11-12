// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerOverheadNameWidget.generated.h"

/**
 * 
 */
UCLASS()
class GAMEANIMATIONSAMPLE_API UPlayerOverheadNameWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* PlayerName;

	void SetDisplayText(FString TextToDisplay);

protected:	
	virtual void NativeDestruct() override;
	
};
