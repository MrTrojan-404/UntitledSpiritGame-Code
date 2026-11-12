#include "CompanionSlotWidget.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "GameAnimationSample/AI/GASPAITypes.h"
#include "GameAnimationSample/AI/Character/Companion/GASPCompanionCharacter.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"
#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "GameAnimationSample/Component/DownedComponent.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "GameFramework/PlayerState.h"

void UCompanionSlotWidget::SetupSlot(AGASPBaseCharacter* Character)
{
	if (!Character) return;
	TrackedCharacter = Character;

	DownedComponent = TrackedCharacter->FindComponentByClass<UDownedComponent>();
    
	// 1. Grab the cached name directly from the Base Character!
	FString DisplayName = TrackedCharacter->GetDisplayName();

	// 2. If it hasn't initialized yet (still default "Unknown"), try again in 0.5s
	if (DisplayName == TEXT("Unknown") || DisplayName.IsEmpty())
	{
		DisplayName = TEXT("Joining...");
		FTimerHandle RetryHandle;
		GetWorld()->GetTimerManager().SetTimer(RetryHandle, this, &UCompanionSlotWidget::RefreshName, 0.5f, false);
	}

	// 3. Clean up ugly Blueprint names
	if (DisplayName.Contains(TEXT("BP_GASP"))) 
	{
		DisplayName = TEXT("OPERATIVE");
	}

	// 4. Set the text
	if (CompanionNameText) 
	{
		CompanionNameText->SetText(FText::FromString(DisplayName));
	}

	// 5. Prevent double-binding and subscribe to health updates
	Character->OnBaseHealthChanged.RemoveAll(this);
	Character->OnBaseHealthChanged.AddDynamic(this, &UCompanionSlotWidget::UpdateHealth);
    
	UpdateHealth();
}

void UCompanionSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (DownedComponent && HealthBar && TrackedCharacter)
    {
       // 0. ULTIMATE OVERRIDE: ARE THEY COMPLETELY DEAD?
       bool bIsDead = false;
       
       if (AGASPAICharacterBase* AIChar = Cast<AGASPAICharacterBase>(TrackedCharacter))
       {
           bIsDead = !AIChar->IsAlive();
       }
       else if (AGASPPlayerCharacter* PlayerChar = Cast<AGASPPlayerCharacter>(TrackedCharacter))
       {
           bIsDead = PlayerChar->IsElimmed();
       }

       if (bIsDead)
       {
          HealthBar->SetPercent(0.f);
          HealthBar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.2f, 0.2f, 1.f)); // Dark Gray
          
          if (StatusText)
          {
             StatusText->SetText(FText::FromString(TEXT("K.I.A.")));
             StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f, 1.f))); // Grey Text
          }
          return; // EXIT EARLY! They are dead, nothing else matters.
       }

    	// 1. Check Downed State
    	bool bIsCurrentlyDowned = DownedComponent->IsDowned();

    	if (bIsCurrentlyDowned)
    	{
    		bWasDowned = true;
            
    		if (DownedComponent->bIsBeingRevived)
    		{
    			// THEY ARE BEING REVIVED! (Show Green Bar + Revive Time)
    			float ReviveTime = DownedComponent->CalculateReviveTime();
    			float Progress = DownedComponent->ReviveProgress;
              
    			HealthBar->SetPercent(Progress / ReviveTime);
    			HealthBar->SetFillColorAndOpacity(FLinearColor::Green); 
              
    			if (StatusText)
    			{
    				float TimeLeft = FMath::Max(0.f, ReviveTime - Progress);
    				StatusText->SetText(FText::FromString(FString::Printf(TEXT("Reviving: %.1fs"), TimeLeft)));
                
    				// Force the text to be Green
    				StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
    			}
    		}
    		else
    		{
    			// THEY ARE BLEEDING OUT! (Show Orange Bar + Bleed Time)
    			float MaxBleedTime = DownedComponent->CalculateBleedOutTime();
    			float CurrentBleedTime = DownedComponent->GetBleedOutTimeRemaining();

    			if (MaxBleedTime > 0.f)
    			{
    				HealthBar->SetPercent(CurrentBleedTime / MaxBleedTime);
    				HealthBar->SetFillColorAndOpacity(DownedHealthColor);
    			}
              
    			if (StatusText)
    			{
    				StatusText->SetText(FText::FromString(FString::Printf(TEXT("Bleeding: %ds"), FMath::CeilToInt(CurrentBleedTime))));
                
    				// Force the text to be Red
    				StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
    			}
    		}
    	}
       else
       {
          if (bWasDowned)
          {
             bWasDowned = false;
             UpdateHealth();
          }

          // 2. DISPLAY AI STATE WHEN ALIVE
          // 2. DISPLAY AI STATE WHEN ALIVE
          if (StatusText)
          {
             FString CurrentStatus = TEXT("Idle");
             FLinearColor StatusColor = FLinearColor(0.8f, 0.8f, 0.8f, 1.f); // Light Gray Default

             AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(GetOwningPlayer());
             
             // === NEW: TAG TEAM OVERRIDE ===
             if (PC && PC->bTagTeamMode)
             {
                 if (PC->bTagTeamOnCooldown)
                 {
                     float CD = GetWorld()->GetTimerManager().GetTimerRemaining(PC->TagTeamCooldownTimer);
                     CurrentStatus = FString::Printf(TEXT("Cooldown: %.1fs"), FMath::Max(0.f, CD));
                     StatusColor = FLinearColor(0.6f, 0.6f, 0.6f, 1.f); // Grey
                 }
                 else
                 {
                     CurrentStatus = TEXT("Ready");
                     StatusColor = FLinearColor(0.1f, 0.8f, 0.2f, 1.f); // Vibrant Green
                 }
             }
             // === NORMAL SQUAD LOGIC ===
             else
             {
                 // NETWORK SAFE HUMAN CHECK: Does this body have a replicated Player State attached?
                 if (TrackedCharacter->GetPlayerState() != nullptr)
                 {
                    CurrentStatus = TEXT("P"); // Shortened text as requested!
                    
                    if (TrackedCharacter->IsLocallyControlled()) StatusColor = FLinearColor(0.1f, 0.8f, 0.2f, 1.f); // Vibrant Green
                    else StatusColor = FLinearColor(0.2f, 0.6f, 1.0f, 1.f); // Co-op Blue
                 }
                 // Or is it a Companion driven by AI?
                 else if (AGASPCompanionCharacter* Companion = Cast<AGASPCompanionCharacter>(TrackedCharacter))
                 {
                     ECompanionOrder Order = Companion->UIState.CurrentOrder;
                     EAIState AIState = Companion->UIState.CurrentAIState;
                     bool bWeaponsFree = Companion->UIState.bWeaponsFree;

                     if (AIState == EAIState::Engage)
                     {
                        if (bWeaponsFree)
                        {
                           CurrentStatus = TEXT("Engaging");
                           StatusColor = FLinearColor::Red;
                        }
                        else
                        {
                           CurrentStatus = TEXT("Tracking Target");
                           StatusColor = FLinearColor(0.9f, 0.5f, 0.0f, 1.f);
                        }
                     }
                     else
                     {
                        switch (Order)
                        {
                        case ECompanionOrder::Follow:
                           if (!Companion->UIState.LeaderName.IsEmpty()) CurrentStatus = FString::Printf(TEXT("Following %s"), *Companion->UIState.LeaderName);
                           else CurrentStatus = TEXT("Following");
                           break;
                        case ECompanionOrder::StandBy:
                           CurrentStatus = TEXT("Holding Position");
                           StatusColor = FLinearColor(0.9f, 0.7f, 0.1f, 1.f);
                           break;
                        case ECompanionOrder::MoveToLocation:
                           CurrentStatus = TEXT("Moving to Target");
                           StatusColor = FLinearColor(0.2f, 0.7f, 0.9f, 1.f);
                           break;
                        case ECompanionOrder::ReviveAlly:
                           CurrentStatus = TEXT("Moving to Revive");
                           StatusColor = FLinearColor::Green;
                           break;
                        }
                     }
                 }
             }

             // Apply the text and colors
             StatusText->SetText(FText::FromString(CurrentStatus));
             StatusText->SetColorAndOpacity(FSlateColor(StatusColor));
          }
       }
    }
}

void UCompanionSlotWidget::UpdateHealth()
{
	// If they are downed, ignore normal health updates. Tick will handle the bar
	if (DownedComponent && DownedComponent->IsDowned()) return;

	if (TrackedCharacter && HealthBar)
	{
		float HealthPercent = TrackedCharacter->GetHealth() / TrackedCharacter->GetMaxHealth();
		HealthBar->SetPercent(HealthPercent);
       
		// Ensure the bar is the normal color when alive
		HealthBar->SetFillColorAndOpacity(NormalHealthColor);
	}
}

void UCompanionSlotWidget::SetSelected(bool bIsSelected)
{
	if (SelectionArrow)
	{
		SelectionArrow->SetVisibility(bIsSelected ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
}

void UCompanionSlotWidget::RefreshName()
{
	SetupSlot(TrackedCharacter);
}