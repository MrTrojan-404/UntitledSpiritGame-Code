#include "GASPCompanionCharacter.h"
#include "GameAnimationSample/GameAnimationSample.h"
#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "GameAnimationSample/Component/DownedComponent.h"
#include "GameAnimationSample/Component/InteractionComponent.h"
#include "GameAnimationSample/GameMode/GASPCoopGameMode.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "GameAnimationSample/UI/HUD/GASPHUD.h"
#include "GameAnimationSample/UI/Widgets/CharacterOverlay.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AGASPCompanionCharacter::AGASPCompanionCharacter()
{
	DownedComponent = CreateDefaultSubobject<UDownedComponent>(TEXT("DownedComponent"));
}

void AGASPCompanionCharacter::BeginPlay()
{
	Super::BeginPlay();
	SpawnDefaultWeapon(true, false);

	CharacterName = CompanionName;
	
	// Listen for revives so we can update the UI back to 100%
	if (DownedComponent)
	{
		DownedComponent->OnPlayerRevived.AddDynamic(this, &AGASPCompanionCharacter::OnRevived);
	}
	
	// 1. Server registers Companion to the GameMode (for mission fail logic)
	if (HasAuthority()) 
	{
		if (AGASPCoopGameMode* GM = Cast<AGASPCoopGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			GM->RegisterCompanion(this);
		}
	}

	// 2. Auto-Link to the Player
	if (AGASPPlayerCharacter* Player = Cast<AGASPPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->ActiveCompanion = this;
	}
}

void AGASPCompanionCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGASPCompanionCharacter, UIState);
}

void AGASPCompanionCharacter::OnDamageReceived(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser)
{
	Super::OnDamageReceived(DamagedActor, Damage, DamageType, InstigatorController, DamageCauser);
    
	// 1. Update the Squad List UI (For everyone)
	OnHealthChanged.Broadcast();

	// 2. If a Human is driving me right now, update their main screen HUD!
	if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(GetController()))
	{
		PC->SetHUDHealth(Health, MaxHealth);
	}
}
void AGASPCompanionCharacter::OnRep_Health(float LastHealth)
{
	Super::OnRep_Health(LastHealth);
    
	// 1. Update the Squad List UI (For everyone)
	OnHealthChanged.Broadcast();

	// 2.If a Human is driving me right now, update their main screen HUD!
	if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(GetController()))
	{
		PC->SetHUDHealth(Health, MaxHealth);
	}
}

void AGASPCompanionCharacter::OnRevived(AGASPBaseCharacter* RevivedCharacter)
{
	OnHealthChanged.Broadcast();
}

void AGASPCompanionCharacter::UpdateInteractionFocus()
{
	if (!IsLocallyControlled()) return;

	if (DownedComponent && DownedComponent->IsDowned())
	{
		ClearFocusedItem();
		return;
	}
	
	AController* SlashController = GetController();
	if (!SlashController) return;

	FVector Start;
	FRotator Rot;
	SlashController->GetPlayerViewPoint(Start, Rot);
	FVector End = Start + Rot.Vector() * INTERACTION_DISTANCE;

	FHitResult Hit;
	FCollisionQueryParams Params;
	
	// 1. Ignore the Player
	Params.AddIgnoredActor(this);

	// 2. NEW: Ignore all equipped weapons/items attached to the player!
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	Params.AddIgnoredActors(AttachedActors);

	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;

	if (HitActor && HitActor->GetClass()->ImplementsInterface(UInteractionInterface::StaticClass()))
	{
		// Check if it's a weapon in someone's hand (your previous fix)
		AWeapon* Weapon = Cast<AWeapon>(HitActor);
		if (Weapon && Weapon->GetWeaponState() != EWeaponState::EWS_Dropped && Weapon->GetWeaponState() != EWeaponState::EWS_Initial)
		{
			ClearFocusedItem();
			return; 
		}

		SetFocusedItem(HitActor);

		// heck for Stealth Takedown FIRST
		if (IInteractionInterface::Execute_CanPerformStealthTakedown(HitActor, this))
		{
			if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(GetController()))
			{
				if (AGASPHUD* HUD = Cast<AGASPHUD>(PC->GetHUD()))
				{
					if (HUD->CharacterOverlay)
					{
						// Assuming 'F' is your takedown/melee key, adjust if needed
						HUD->CharacterOverlay->UpdateInteractionPrompt(true, TEXT("F - Stealth Takedown"));
					}
				}
			}
			return; // Exit early so we don't display normal interaction text
		}

		// --- Default Interaction (Weapons, Revives, Items) ---
		FText PromptText = IInteractionInterface::Execute_GetInteractionText(HitActor);
		if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(GetController()))
		{
			if (AGASPHUD* HUD = Cast<AGASPHUD>(PC->GetHUD()))
			{
				if (HUD->CharacterOverlay)
				{
					if (!PromptText.IsEmpty())
					{
						FString FinalText = FString::Printf(TEXT("E - %s"), *PromptText.ToString());
						HUD->CharacterOverlay->UpdateInteractionPrompt(true, FinalText);
					}
					else
					{
						HUD->CharacterOverlay->UpdateInteractionPrompt(false, TEXT(""));
					}
				}
			}
		}
	}
	else
	{
		ClearFocusedItem();
	}
}


void AGASPCompanionCharacter::ClearFocusedItem()
{
	// 1. Hide the HUD Prompt REGARDLESS of whether we have a FocusedItem pointer or not!
	if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(GetController()))
	{
		if (AGASPHUD* HUD = Cast<AGASPHUD>(PC->GetHUD()))
		{
			if (HUD->CharacterOverlay)
			{
				HUD->CharacterOverlay->UpdateInteractionPrompt(false, TEXT(""));
			}
		}
	}

	// 2. Only run the revive-cancel logic if we actually had a valid item focused
	if (InteractionComponent->FocusedItem)
	{
		if (HasAuthority())
		{
			UDownedComponent* Downed = InteractionComponent->FocusedItem->FindComponentByClass<UDownedComponent>();
			if (Downed && Downed->IsDowned() && Downed->GetCurrentReviver() == this)
			{
				Downed->CancelRevive();
			}
		}
		else
		{
			ServerCancelRevive(InteractionComponent->FocusedItem);
		}
	}

	InteractionComponent->FocusedItem = nullptr;
}

void AGASPCompanionCharacter::SetFocusedItem(AActor* NewItem)
{
	if (InteractionComponent->FocusedItem == NewItem) return;

	ClearFocusedItem();
	InteractionComponent->FocusedItem = NewItem;
}

void AGASPCompanionCharacter::HandleDeath(AController* InstigatorController, AActor* DamageCauser)
{
	if (DownedComponent)
	{
		if (DownedComponent->IsDowned())
		{
			return; 
		}

		if (InstigatorController == nullptr && DamageCauser == nullptr)
		{
			Super::HandleDeath(InstigatorController, DamageCauser);
			return;
		}

		DownedComponent->GoDown();
	}
	else
	{
		Super::HandleDeath(InstigatorController, DamageCauser);
	}
}
void AGASPCompanionCharacter::Interact_Implementation(AActor* Interactor)
{
	if (DownedComponent && DownedComponent->IsDowned())
	{
		DownedComponent->Interact_Implementation(Interactor);
	}
}

FText AGASPCompanionCharacter::GetInteractionText_Implementation() const
{
	if (DownedComponent && DownedComponent->IsDowned())
	{
		return DownedComponent->GetInteractionText_Implementation();
	}
	return FText::GetEmpty();
}
