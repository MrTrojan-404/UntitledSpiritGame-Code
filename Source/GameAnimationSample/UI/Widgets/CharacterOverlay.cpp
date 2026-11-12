// Fill out your copyright notice in the Description page of Project Settings.


#include "CharacterOverlay.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "COOP/CompanionSlotWidget.h"
#include "COOP/DirectionalIndicatorWidget.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/Dialogue/DialogueSubsystem.h"

void UCharacterOverlay::ShowHitMarker(bool bHeadshot)
{
	if (!HitMarkerImage) return;

	// Tint red for headshots, white for normal hits
	HitMarkerImage->SetColorAndOpacity(
		bHeadshot
		? FLinearColor::Red
		: FLinearColor::White
	);

	HitMarkerImage->SetVisibility(ESlateVisibility::Visible);

	GetWorld()->GetTimerManager().ClearTimer(HitMarkerTimer);

	GetWorld()->GetTimerManager().SetTimer(
		HitMarkerTimer,
		[this]()
		{
			if (HitMarkerImage)
			{
				HitMarkerImage->SetVisibility(ESlateVisibility::Hidden);
			}
		},
		0.15f,
		false
	);
}

void UCharacterOverlay::SetAmmoUIVisibility(bool bVisible)
{
	ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;

	if (WeaponAmmoAmount) WeaponAmmoAmount->SetVisibility(NewVisibility);
	if (CarriedAmmoAmount) CarriedAmmoAmount->SetVisibility(NewVisibility);
}

void UCharacterOverlay::NativeConstruct()
{
	Super::NativeConstruct();

	if (HitMarkerImage)
	{
		HitMarkerImage->SetVisibility(ESlateVisibility::Hidden);
	}

	// 1. Hide subtitles initially
	if (SpeakerText) SpeakerText->SetVisibility(ESlateVisibility::Hidden);
	if (SubtitleText) SubtitleText->SetVisibility(ESlateVisibility::Hidden);

	// 2. Bind to the Dialogue Subsystem!
	if (GetGameInstance())
	{
		if (UDialogueSubsystem* DialogueSubsystem = GetGameInstance()->GetSubsystem<UDialogueSubsystem>())
		{
			DialogueSubsystem->OnDialogueUpdated.AddDynamic(this, &UCharacterOverlay::UpdateSubtitle);
			DialogueSubsystem->OnDialogueCleared.AddDynamic(this, &UCharacterOverlay::ClearSubtitle);
		}
	}
	// Hide the cooldown container immediately on startup
	if (MendCooldownContainer)
	{
		MendCooldownContainer->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UCharacterOverlay::StartMendCooldown(float Duration)
{
	MendCooldownRemaining = Duration;
	
	if (MendCooldownContainer)
	{
		// Show the container (and the image/text inside it)
		MendCooldownContainer->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UCharacterOverlay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Process the Mend Cooldown if it is active
	if (MendCooldownRemaining > 0.f)
	{
		MendCooldownRemaining -= InDeltaTime;
		
		if (MendCooldownRemaining <= 0.f)
		{
			// Cooldown finished! Hide everything.
			MendCooldownRemaining = 0.f;
			if (MendCooldownContainer)
			{
				MendCooldownContainer->SetVisibility(ESlateVisibility::Hidden);
			}
		}
		else
		{
			// Update the text to show remaining seconds (e.g., "4.2")
			if (MendCooldownText)
			{
				MendCooldownText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), MendCooldownRemaining)));
			}
		}
	}
}

void UCharacterOverlay::UpdateInteractionPrompt(bool bShow, const FString& PromptText)
{
	if (InteractionText)
	{
		if (bShow)
		{
			InteractionText->SetText(FText::FromString(PromptText));
			
			// HitTestInvisible ensures the text doesn't block mouse clicks
			InteractionText->SetVisibility(ESlateVisibility::HitTestInvisible); 
		}
		else
		{
			// Hide the text when not looking at an interactable object
			InteractionText->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UCharacterOverlay::UpdateSubtitle(const FDialogueEntry& Entry)
{
	if (SpeakerText)
	{
		SpeakerText->SetText(FText::FromString(Entry.SpeakerName));
		SpeakerText->SetVisibility(ESlateVisibility::Visible);
	}
	if (SubtitleText)
	{
		SubtitleText->SetText(Entry.SubtitleText);
		SubtitleText->SetVisibility(ESlateVisibility::Visible);
	}
}

void UCharacterOverlay::ClearSubtitle()
{
	if (SpeakerText) SpeakerText->SetVisibility(ESlateVisibility::Hidden);
	if (SubtitleText) SubtitleText->SetVisibility(ESlateVisibility::Hidden);
}

void UCharacterOverlay::RefreshSquadListUI(const TArray<AGASPBaseCharacter*>& SquadMembers)
{
	if (!SquadListContainer || !CompanionSlotClass) return;

	SquadListContainer->ClearChildren();
	SpawnedCompanionSlots.Empty();

	for (AGASPBaseCharacter* Member : SquadMembers)
	{
		if (!Member) continue;

		UCompanionSlotWidget* NewSlot = CreateWidget<UCompanionSlotWidget>(this, CompanionSlotClass);
		if (NewSlot)
		{
			NewSlot->SetupSlot(Member);
			SquadListContainer->AddChildToVerticalBox(NewSlot);
          
			//You must add them to the map or the arrow can't find them! <---
			SpawnedCompanionSlots.Add(Member, NewSlot); 
		}
	}
}


void UCharacterOverlay::HighlightSelectedCompanion(AGASPBaseCharacter* SelectedCharacter, bool bIsWheelActive)
{
	for (const auto& Pair : SpawnedCompanionSlots)
	{
		if (UCompanionSlotWidget* SlotWidget = Pair.Value)
		{
			// Compare the base character pointers
			bool bShouldBeHighlighted = bIsWheelActive && (Pair.Key == SelectedCharacter);
			SlotWidget->SetSelected(bShouldBeHighlighted);
		}
	}
}

void UCharacterOverlay::UpdateDownedStatus(bool bIsDowned, bool bIsBeingRevived, float TimeLeft)
{
	if (!DownedStatusText) return;

	if (!bIsDowned)
	{
		DownedStatusText->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	DownedStatusText->SetVisibility(ESlateVisibility::Visible);
    
	if (bIsBeingRevived)
	{
		DownedStatusText->SetText(FText::FromString(FString::Printf(TEXT("BEING REVIVED: %.1fs"), TimeLeft)));
		DownedStatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
	}
	else
	{
		DownedStatusText->SetText(FText::FromString(FString::Printf(TEXT("BLEEDING OUT: %ds"), FMath::CeilToInt(TimeLeft))));
		DownedStatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	}
}

float UCharacterOverlay::CalculateDirectionalUIAngle(APlayerController* PC, AActor* EnemyActor)
{
    if (!PC || !PC->GetPawn() || !EnemyActor) return 0.f;

    APawn* PlayerPawn = PC->GetPawn();
    
    FVector PlayerLoc = PlayerPawn->GetActorLocation();
    FVector EnemyLoc = EnemyActor->GetActorLocation();

    FVector DirToEnemy = (EnemyLoc - PlayerLoc).GetSafeNormal();

    FRotator CamRot = PC->PlayerCameraManager->GetCameraRotation();
    FVector CamForward = CamRot.Vector();
    FVector CamRight = FRotationMatrix(CamRot).GetScaledAxis(EAxis::Y);

    FVector2D Forward2D(CamForward.X, CamForward.Y);
    FVector2D Right2D(CamRight.X, CamRight.Y);
    FVector2D Dir2D(DirToEnemy.X, DirToEnemy.Y);

    Forward2D.Normalize();
    Right2D.Normalize();
    Dir2D.Normalize();

    float DotForward = FVector2D::DotProduct(Forward2D, Dir2D);
    float DotRight = FVector2D::DotProduct(Right2D, Dir2D);

    return FMath::RadiansToDegrees(FMath::Atan2(DotRight, DotForward)); 
}

void UCharacterOverlay::UpdateStealthHUD(AActor* DetectingEnemy, float DetectionLevel, uint8 AlertPhaseRaw)
{
    if (!DetectingEnemy || !DetectionCanvas || !DirectionalIndicatorClass) return;

    // 1. If detection is 0, remove the widget and clear it from the map
    if (DetectionLevel <= 0.01f)
    {
        if (UDirectionalIndicatorWidget** FoundWidget = ActiveStealthIndicators.Find(DetectingEnemy))
        {
            if (*FoundWidget)
            {
                (*FoundWidget)->RemoveFromParent();
            }
            ActiveStealthIndicators.Remove(DetectingEnemy);
        }
        return;
    }

    // 2. Get or Create the Indicator Widget
    UDirectionalIndicatorWidget* IndicatorWidget = nullptr;
    if (UDirectionalIndicatorWidget** FoundWidget = ActiveStealthIndicators.Find(DetectingEnemy))
    {
        IndicatorWidget = *FoundWidget;
    }
    else
    {
        // Spawn a new one!
        IndicatorWidget = CreateWidget<UDirectionalIndicatorWidget>(GetOwningPlayer(), DirectionalIndicatorClass);
        if (IndicatorWidget)
        {
            UCanvasPanelSlot* CanvasSlot = DetectionCanvas->AddChildToCanvas(IndicatorWidget);
            if (CanvasSlot)
            {
                // Anchor it to the exact center of the screen
                CanvasSlot->SetAutoSize(true);
                CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
                CanvasSlot->SetAnchors(FAnchors(0.5f)); 
                CanvasSlot->SetPosition(FVector2D::ZeroVector);
            }
            ActiveStealthIndicators.Add(DetectingEnemy, IndicatorWidget);
        }
    }

	// 3. Update Rotation, Visuals, and POSITION
	if (IndicatorWidget)
	{
		// Calculate where the enemy is relative to our camera
		float Angle = CalculateDirectionalUIAngle(GetOwningPlayer(), DetectingEnemy);
        
		// Rotate the widget to point at them
		IndicatorWidget->SetRenderTransformAngle(Angle);

		// === NEW RADIAL POSITIONING MATH ===
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(IndicatorWidget->Slot))
		{
			// Convert Angle back to radians for the math functions
			float RadAngle = FMath::DegreesToRadians(Angle);
            
			// In Unreal UMG, the Y-axis goes DOWN, so we must invert the Cosine!
			// 0 degrees = Up (X:0, Y:-Radius)
			// 90 degrees = Right (X:Radius, Y:0)
			float X = FMath::Sin(RadAngle) * IndicatorRadius;
			float Y = -FMath::Cos(RadAngle) * IndicatorRadius;
            
			CanvasSlot->SetPosition(FVector2D(X, Y));
		}

		// Update the fill bar and colors
		IndicatorWidget->UpdateVisuals(DetectionLevel, AlertPhaseRaw);
	}
}

void UCharacterOverlay::SetUltimateCharge(float CurrentCharge, float MaxCharge)
{
	if (UltimateBar)
	{
		UltimateBar->SetPercent(MaxCharge > 0.f ? (CurrentCharge / MaxCharge) : 0.f);
	}
}

void UCharacterOverlay::SetEnergySegments(int32 CurrentSegments, int32 MaxSegments)
{
	if (!EnergySegmentBox) return;

	// 1. If we increased our Max Energy, spawn new blocks dynamically!
	while (EnergySegmentBox->GetChildrenCount() < MaxSegments)
	{
		UImage* NewSegment = NewObject<UImage>(this);

		// Give the raw image a solid white brush so it actually renders!
		FSlateBrush SolidBrush;
		SolidBrush.DrawAs = ESlateBrushDrawType::Box;
		NewSegment->SetBrush(SolidBrush);

		UHorizontalBoxSlot* BoxSlot = EnergySegmentBox->AddChildToHorizontalBox(NewSegment);
		
		BoxSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 0.f));
		BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// 2. If we somehow lost Max Energy, remove the excess blocks
	while (EnergySegmentBox->GetChildrenCount() > MaxSegments)
	{
		EnergySegmentBox->RemoveChildAt(EnergySegmentBox->GetChildrenCount() - 1);
	}

	// 3. Color them based on how much energy we currently have
	for (int32 i = 0; i < MaxSegments; i++)
	{
		if (UImage* SegmentImage = Cast<UImage>(EnergySegmentBox->GetChildAt(i)))
		{
			SegmentImage->SetBrushTintColor(i < CurrentSegments ? EnergyFullColor : EnergyEmptyColor);
		}
	}
}

void UCharacterOverlay::SetHUDMedkits(int32 MedkitCount)
{
	if (MedkitCountText)
	{
		FString CountText = FString::Printf(TEXT("%d"), MedkitCount);
		MedkitCountText->SetText(FText::FromString(CountText));
	}
}