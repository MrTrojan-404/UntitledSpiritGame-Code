// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractWidget.generated.h"

class UTextBlock;
/**
 * 
 */
UCLASS()
class GAMEANIMATIONSAMPLE_API UInteractWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* DisplayText;

	void SetDisplayText(FString TextToDisplay);

	void SetTextColor(FLinearColor Color);
	
protected:	
	virtual void NativeDestruct() override;
	
};
