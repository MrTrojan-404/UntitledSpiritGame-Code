// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterOverlay.generated.h"

class AGASPBaseCharacter;
class UVerticalBox;
class UCompanionSlotWidget;
class AGASPCompanionCharacter;
struct FDialogueEntry;

UCLASS()
class GAMEANIMATIONSAMPLE_API UCharacterOverlay : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void UpdateInteractionPrompt(bool bShow, const FString& PromptText);
	
	UPROPERTY(meta = (BindWidget))
	class UProgressBar* HealthBar;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* HealthText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* WeaponAmmoAmount;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* CarriedAmmoAmount;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* GrenadesText;

	UPROPERTY(meta = (BindWidgetOptional))
	class UTextBlock* MedkitCountText;

	void SetHUDMedkits(int32 MedkitCount);
	
	UPROPERTY(meta = (BindWidget))
	class UImage* HitMarkerImage;
	
	UPROPERTY(meta = (BindWidget))
	class UImage* HighPingImage;

	UPROPERTY(meta = (BindWidgetAnim), Transient)
	UWidgetAnimation* HighPingAnimation;

	UFUNCTION(BlueprintCallable)
	void ShowHitMarker(bool bHeadshot);
	FTimerHandle HitMarkerTimer;

	// --- SUBTITLES ---
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* SpeakerText;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* SubtitleText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* InteractionText;
	
	UFUNCTION()
	void UpdateSubtitle(const FDialogueEntry& Entry);

	UFUNCTION()
	void ClearSubtitle();

	//  SQUAD LIST UI FOUNDATION
	UPROPERTY(meta = (BindWidgetOptional))
	UVerticalBox* SquadListContainer;

	// We need to know which Blueprint widget to spawn
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UCompanionSlotWidget> CompanionSlotClass;
	
	// A Map to instantly find a widget based on the companion it tracks
	UPROPERTY()
	TMap<AGASPBaseCharacter*, UCompanionSlotWidget*> SpawnedCompanionSlots;

	void RefreshSquadListUI(const TArray<AGASPBaseCharacter*>& SquadMembers);
	void HighlightSelectedCompanion(AGASPBaseCharacter* SelectedCharacter, bool bIsWheelActive);

	// LOCAL PLAYER DOWNED STATUS
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* DownedStatusText;

	void UpdateDownedStatus(bool bIsDowned, bool bIsBeingRevived, float TimeLeft);

	// ── Stealth Detection HUD ──────────────────────────────────
	
	// A Canvas Panel to hold all the individual enemy indicators
	UPROPERTY(meta = (BindWidgetOptional))
	class UCanvasPanel* DetectionCanvas;

	// The widget class to spawn for each detecting enemy
	UPROPERTY(EditDefaultsOnly, Category = "Stealth")
	TSubclassOf<class UDirectionalIndicatorWidget> DirectionalIndicatorClass;

	// How far from the center of the screen the stealth arrows will float
	UPROPERTY(EditDefaultsOnly, Category = "Stealth")
	float IndicatorRadius = 250.f;
	
	// Keeps track of which widget belongs to which enemy
	UPROPERTY()
	TMap<AActor*, class UDirectionalIndicatorWidget*> ActiveStealthIndicators;

	// Helper to calculate the rotation angle
	float CalculateDirectionalUIAngle(APlayerController* PC, AActor* EnemyActor);

	/**
	* Called by GASPCorePlayerController via Client_UpdateDetectionHUD RPC.
   * @param DetectionLevel  0.0 – 1.0 (highest enemy meter fill)
   * @param AlertPhaseRaw   uint8 cast of EAlertPhase (0=Unaware…3=Alert)
	*/
	void UpdateStealthHUD(AActor* DetectingEnemy, float DetectionLevel, uint8 AlertPhaseRaw);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetAmmoUIVisibility(bool bVisible);

	// --- SPELL & ABILITY UI ---
	UPROPERTY(meta = (BindWidgetOptional))
	class UProgressBar* UltimateBar;

	UPROPERTY(meta = (BindWidgetOptional))
	class UHorizontalBox* EnergySegmentBox;

	// The color of a full segment (Default: Cyan/Blue)
	UPROPERTY(EditDefaultsOnly, Category = "Spells|Energy")
	FSlateColor EnergyFullColor = FSlateColor(FLinearColor(0.0f, 0.8f, 1.0f, 1.0f)); 

	// The color of an empty/spent segment (Default: Translucent Dark Grey)
	UPROPERTY(EditDefaultsOnly, Category = "Spells|Energy")
	FSlateColor EnergyEmptyColor = FSlateColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f)); 

	void SetUltimateCharge(float CurrentCharge, float MaxCharge);
	void SetEnergySegments(int32 CurrentSegments, int32 MaxSegments);

	// --- MEND COOLDOWN ---
	// This container will hold BOTH the Image and the Text Block
	UPROPERTY(meta = (BindWidgetOptional))
	class UWidget* MendCooldownContainer;

	UPROPERTY(meta = (BindWidgetOptional))
	class UTextBlock* MendCooldownText;

	void StartMendCooldown(float Duration);
	
protected:
	// Adding NativeTick so the UI can count down smoothly every frame
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	float MendCooldownRemaining = 0.f;
	
};
