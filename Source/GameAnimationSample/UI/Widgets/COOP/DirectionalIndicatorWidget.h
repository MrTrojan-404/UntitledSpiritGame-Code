#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DirectionalIndicatorWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS()
class GAMEANIMATIONSAMPLE_API UDirectionalIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UProgressBar* DetectionBar;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* PhaseIndicator;

	// We move your old color/text logic into this individual widget!
	void UpdateVisuals(float DetectionLevel, uint8 AlertPhaseRaw);
};