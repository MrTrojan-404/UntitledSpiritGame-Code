#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CompanionSlotWidget.generated.h"

class UDownedComponent;
class AGASPCompanionCharacter;
class UTextBlock;
class UProgressBar;
class UImage;

UCLASS()
class GAMEANIMATIONSAMPLE_API UCompanionSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* CompanionNameText;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* HealthBar;

	UPROPERTY(meta = (BindWidget))
	UImage* SelectionArrow;

	UPROPERTY()
	class AGASPBaseCharacter* TrackedCharacter;

	void SetupSlot(AGASPBaseCharacter* Character);

	UFUNCTION()
	void UpdateHealth();
	
	void SetSelected(bool bIsSelected);
	void RefreshName();

	// Track the Downed Component
	UPROPERTY()
	UDownedComponent* DownedComponent;

	// Colors you can tweak in the Blueprint!
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
	FLinearColor NormalHealthColor = FLinearColor(0.8f, 0.1f, 0.1f, 1.f); // Red

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
	FLinearColor DownedHealthColor = FLinearColor(0.9f, 0.5f, 0.05f, 1.f); // Orange

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StatusText;
	
protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Track their state to catch the moment they wake up
	bool bWasDowned = false;
	
};