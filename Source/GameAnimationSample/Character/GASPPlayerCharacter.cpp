// Fill out your copyright notice in the Description page of Project Settings.


#include "GASPPlayerCharacter.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameAnimationSample/Actor/Item.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "GameAnimationSample/Component/InteractionComponent.h"
#include "GameAnimationSample/GameState/GASPGameState.h"
#include "GameAnimationSample/GameVariables/Team.h"
#include "GameAnimationSample/PlayerStart/TeamPlayerStart.h"
#include "GameAnimationSample/UI/Widgets/PlayerOverheadHealthWidget.h"
#include "GameAnimationSample/UI/Widgets/PlayerOverheadNameWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameAnimationSample/GameAnimationSample.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "GameAnimationSample/AI/Character/Companion/GASPCompanionCharacter.h"
#include "GameAnimationSample/Component/DownedComponent.h"
#include "GameAnimationSample/Component/StealthComponent.h"
#include "GameAnimationSample/GameMode/GASPCoopGameMode.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "GameAnimationSample/PlayerState/GASPArenaPlayerState.h"
#include "GameAnimationSample/PlayerState/GASPCorePlayerState.h"
#include "GameAnimationSample/UI/Widgets/CharacterOverlay.h"
#include "GameAnimationSample/UI/Widgets/COOP/WeaponRadialMenu/WeaponRadialMenu.h"
#include "Perception/AISense_Damage.h"

AGASPPlayerCharacter::AGASPPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	
	NoiseEmitterComponent = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("NoiseEmitterComponent"));

	DownedComponent = CreateDefaultSubobject<UDownedComponent>(TEXT("DownedComponent"));
	StealthComponent = CreateDefaultSubobject<UStealthComponent>(TEXT("StealthComponent"));

}

void AGASPPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (!OverheadNameWidget)
	{
		OverheadNameWidget = FindComponentByTag<UWidgetComponent>(FName("NameWidget"));
		if (OverheadNameWidget)
		{
			UE_LOG(LogTemp, Log, TEXT("AGASPPlayerCharacter: Found name widget with tag 'NameWidget'"));
		}
	}

	if (!OverheadHealthWidget)
	{
		OverheadHealthWidget = FindComponentByTag<UWidgetComponent>(FName("HealthWidget"));
		if (OverheadHealthWidget)
		{
			UE_LOG(LogTemp, Log, TEXT("AGASPPlayerCharacter: Found health widget with tag 'HealthWidget'"));
		}
	}

	
	// Configure widgets if they exist
	if (OverheadNameWidget)
	{
		OverheadNameWidget->SetVisibility(false);
	}

	if (OverheadHealthWidget)
	{
		OverheadHealthWidget->SetVisibility(false);
	}
	if (HasAuthority())
	{
		if (AGASPCoopGameMode* CoopGM = Cast<AGASPCoopGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			CoopGM->RegisterSquadMember(this);
		}

		Health = MaxHealth;
	}
	// If we are a client, wait a tiny bit for other players to replicate, then refresh the HUD
	if (!HasAuthority())
	{
		FTimerHandle RefreshTimer;
		GetWorldTimerManager().SetTimer(RefreshTimer, this, &AGASPPlayerCharacter::RefreshHUDWithFullSquad, 1.5f, false);
	}
}

void AGASPPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	PollInit();

	// UPDATE LOCAL DOWNED SCREEN
	if (IsLocallyControlled() && DownedComponent)
	{
		if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(Controller))
		{
			if (AGASPHUD* HUD = Cast<AGASPHUD>(PC->GetHUD()))
			{
				if (HUD->CharacterOverlay)
				{
					// Figure out which timer we are currently tracking
					float TimeLeft = DownedComponent->bIsBeingRevived 
						? FMath::Max(0.f, DownedComponent->CalculateReviveTime() - DownedComponent->ReviveProgress) 
						: DownedComponent->GetBleedOutTimeRemaining();

					HUD->CharacterOverlay->UpdateDownedStatus(
						DownedComponent->IsDowned(), 
						DownedComponent->bIsBeingRevived, 
						TimeLeft
					);
				}
			}
		}
	}
}


void AGASPPlayerCharacter::Destroyed()
{
	Super::Destroyed();
	
	GASPGameMode = GASPGameMode == nullptr ? GetWorld()->GetAuthGameMode<AGASPCoreGameMode>() : GASPGameMode;
	bool bMatchNotInProgress = GASPGameMode && GASPGameMode->GetMatchState() != MatchState::InProgress;
	
	if (CombatComponent && CombatComponent->GetPrimaryWeapon() && bMatchNotInProgress)
	{
		CombatComponent->GetPrimaryWeapon()->Destroy();
	}
}



void AGASPPlayerCharacter::PollInit()
{
	if (GASPPlayerState == nullptr)
	{
		GASPPlayerState = GetPlayerState<AGASPCorePlayerState>();
		if (GASPPlayerState)
		{
			OnPlayerStateInitialized();
          
			AGASPGameState* GASPGameState = Cast<AGASPGameState>(UGameplayStatics::GetGameState(this));

			if (GASPGameState && GASPGameState->TopScoringPlayers.Contains(GASPPlayerState))
			{
				MulticastGainedTheLead();
			}
		}
	}

	if (GASPPlayerController == nullptr)
	{
		GASPPlayerController = Cast<AGASPCorePlayerController>(Controller);
		if (GASPPlayerController)
		{
			SpawnDefaultWeapon(true, bElimmed);
			UpdateHUDAmmo();
			UpdateHUDHealth();

			// We don't need the TArray FoundActors loop here anymore!
			// Just call the refresh helper.
			RefreshHUDWithFullSquad();
		}
	}
}

void AGASPPlayerCharacter::RefreshHUDWithFullSquad_Implementation()
{
	// 1. Get the local controller and HUD (only exists on the owning client/listen-server)
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController()) return;

	AGASPHUD* HUD = Cast<AGASPHUD>(PC->GetHUD());
	if (!HUD || !HUD->CharacterOverlay) return;

	TArray<AGASPBaseCharacter*> FullSquad;

	// 2. Gather Players (Skip Me)
	TArray<AActor*> FoundPlayers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPPlayerCharacter::StaticClass(), FoundPlayers);
	for (AActor* Actor : FoundPlayers)
	{
		if (Actor != this) 
		{
			if (AGASPBaseCharacter* BC = Cast<AGASPBaseCharacter>(Actor)) FullSquad.Add(BC);
		}
	}

	// 3. Gather AI
	TArray<AActor*> FoundCompanions;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPCompanionCharacter::StaticClass(), FoundCompanions);
	for (AActor* Actor : FoundCompanions)
	{
		if (AGASPBaseCharacter* BC = Cast<AGASPBaseCharacter>(Actor)) FullSquad.Add(BC);
	}

	// 4. Update UI
	HUD->CharacterOverlay->RefreshSquadListUI(FullSquad);
}



void AGASPPlayerCharacter::OnDamageReceived(AActor* DamagedActor,
	float Damage, const UDamageType* DamageType,
	AController* InstigatorController, AActor* DamageCauser)
{
	Super::OnDamageReceived(DamagedActor, Damage, DamageType,
	   InstigatorController, DamageCauser);

	// When the player body is AI-controlled (switched away), report damage
	// so the AI controller's HandleDamageStimulus fires the self-defense override.
	if (InstigatorController && InstigatorController->GetPawn() && DamageCauser)
	{
		UAISense_Damage::ReportDamageEvent(
			GetWorld(),
			this,
			InstigatorController->GetPawn(),
			Damage,
			GetActorLocation(),
			DamageCauser->GetActorLocation()
		);
	}
	
	GASPGameMode = GASPGameMode == nullptr 
	   ? GetWorld()->GetAuthGameMode<AGASPCoreGameMode>() 
	   : GASPGameMode;
	if (bElimmed || GASPGameMode == nullptr) return;

	UpdateHUDHealth();
	Multicast_ShowOverheadHealth(Health);

	if (Health <= 0.f)
	{
		// 1. Are we in Co-op Mode?
		bool bIsCoopMode = GASPGameMode->IsA<AGASPCoopGameMode>();

		// 2. Check Co-op condition (using your direct pointer instead of the slow FindComponent!)
		if (bIsCoopMode && DownedComponent && !DownedComponent->IsDowned())
		{
			DownedComponent->GoDown();
			return;
		}

		// 3. Normal elim flow — Deathmatch (or we already bled out in Co-op)
		if (GASPGameMode)
		{
			GASPPlayerController = GASPPlayerController == nullptr 
			   ? Cast<AGASPCorePlayerController>(Controller) 
			   : GASPPlayerController;
			AGASPCorePlayerController* AttackerController = 
			   Cast<AGASPCorePlayerController>(InstigatorController);
             
			GASPGameMode->PlayerEliminated(
			   this, GASPPlayerController, AttackerController);
		}
	}
}

void AGASPPlayerCharacter::Multicast_ResetToAlive_Implementation()
{
	Super::Multicast_ResetToAlive_Implementation();
	
	bElimmed = false;
	GetWorldTimerManager().ClearTimer(ElimTimer);
}

void AGASPPlayerCharacter::Interact_Implementation(AActor* Interactor)
{
	// If we are crawling on the floor, let the Downed Component handle the interaction!
	if (DownedComponent && DownedComponent->IsDowned())
	{
		//Change the Cast from PlayerCharacter to BaseCharacter
		AGASPBaseCharacter* Reviver = Cast<AGASPBaseCharacter>(Interactor);
		if (Reviver)
		{
			DownedComponent->StartRevive(Reviver);
		}
	}
}

FText AGASPPlayerCharacter::GetInteractionText_Implementation() const
{
	// Show the revive timer text on the HUD!
	if (DownedComponent && DownedComponent->IsDowned())
	{
		return DownedComponent->GetInteractionText_Implementation();
	}
	return FText::GetEmpty();
}

void AGASPPlayerCharacter::DropOrDestroyWeapon(AWeapon* Weapon)
{
	
	if (Weapon == nullptr) return;
	if (Weapon->bDestroyWeapon)
	{
		Weapon->Destroy();
	}
	else
	{
		Weapon->Dropped();
	}
}

void AGASPPlayerCharacter::DropOrDestroyWeapons()
{
	if (CombatComponent)
	{
		if (CombatComponent->GetPrimaryWeapon())
		{
			DropOrDestroyWeapon(CombatComponent->GetPrimaryWeapon());
		}
		if (CombatComponent->GetSecondaryWeapon())
		{
			DropOrDestroyWeapon(CombatComponent->GetSecondaryWeapon());
		}
	}
}

#pragma region Elim

void AGASPPlayerCharacter::Elim(bool bPlayerLeftGame)
{
	AGASPCoopGameMode* GM = Cast<AGASPCoopGameMode>(GetWorld()->GetAuthGameMode());
	bool bDropWeapons = true;
	
	//Do NOT drop weapons if we are in Borderlands-style persistent respawn!
	if (GM && GM->CheckpointType == ECheckpointType::Persistent && !bPlayerLeftGame)
	{
		bDropWeapons = false;
	}

	if (bDropWeapons)
	{
		DropOrDestroyWeapons();
	}
	
	MulticastElim(bPlayerLeftGame, LastHitDirection, LastHitBone, bDropWeapons);
}

void AGASPPlayerCharacter::MulticastElim_Implementation(bool bPlayerLeftGame, FVector_NetQuantizeNormal HitDirection, FName HitBone, bool bDropWeapons)
{
	bLeftGame = bPlayerLeftGame;
	
	//Respect the GameMode rules before dropping the visual mesh!
	if (bDropWeapons && CombatComponent && CombatComponent->GetPrimaryWeapon())
	{
		CombatComponent->GetPrimaryWeapon()->Dropped();
	}

	if (GASPPlayerController)
	{
		GASPPlayerController->SetHUDWeaponAmmo(0);
	}

	if (OverheadHealthWidget) OverheadHealthWidget->SetVisibility(false);
	if (OverheadNameWidget) OverheadNameWidget->SetVisibility(false);
	
	bElimmed = true;
	bDisableGameplay = true;
	
	if (CombatComponent)
	{
		CombatComponent->HandleFire(false);
	}

	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapsuleComponent->SetEnableGravity(false);
	CapsuleComponent->ComponentVelocity = FVector::ZeroVector;

	AttachedGrenade->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetReplicateMovement(false);
	
	LastHitDirection = HitDirection;
	LastHitBone = HitBone;
	ApplyDeathPhysics();
	
	bool bHideSniperScope = IsLocallyControlled() && 
		CombatComponent && 
		CombatComponent->GetIsAiming() && 
		CombatComponent->GetPrimaryWeapon() && 
		CombatComponent->GetPrimaryWeapon()->GetWeaponType() == EWeaponType::EWT_SniperRifle;
	if (bHideSniperScope)
	{
		ShowSniperScopeWidget(false);
	}

	if (CrownComponent) CrownComponent->DestroyComponent();

	GetWorldTimerManager().SetTimer(ElimTimer, this, &AGASPPlayerCharacter::ElimTimerFinished, RespawnDelay);

	DissolveDelay = RespawnDelay - 2.4f;
	GetWorldTimerManager().SetTimer(DissolveTImer, this, &AGASPPlayerCharacter::DissolveTimerFinished, DissolveDelay);
}

void AGASPPlayerCharacter::ElimTimerFinished()
{
	// Stop physics before respawn moves the actor
	if (CharacterMesh)
	{
		CharacterMesh->SetSimulatePhysics(false);
		CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	
	GASPGameMode = GASPGameMode == nullptr ? GetWorld()->GetAuthGameMode<AGASPCoreGameMode>() : GASPGameMode;
	if (GASPGameMode && !bLeftGame)
	{
		GASPGameMode->RequestRespawn(this, Controller);
	}
	if (bLeftGame && IsLocallyControlled())
	{
		OnLeftGame.Broadcast();
	}
}

#pragma endregion Elim

void AGASPPlayerCharacter::ServerLeaveGame_Implementation()
{
	GASPGameMode = GASPGameMode == nullptr ? GetWorld()->GetAuthGameMode<AGASPCoreGameMode>() : GASPGameMode;
	GASPPlayerState = GASPPlayerState == nullptr ? GetPlayerState<AGASPCorePlayerState>() : GASPPlayerState;
	if (GASPGameMode && GASPPlayerState)
	{
		GASPGameMode->PlayerLeftGame(GASPPlayerState);
	}
}

void AGASPPlayerCharacter::OnRep_Health(float LastHealth)
{
	Super::OnRep_Health(LastHealth);
	
	UpdateHUDHealth();
}

void AGASPPlayerCharacter::SetSpawnPoint()
{
	AGASPArenaPlayerState* ArenaPS = Cast<AGASPArenaPlayerState>(GetPlayerState());
	if (HasAuthority() && ArenaPS && ArenaPS->GetTeam() != ETeam::ET_NoTeam)
	{
		TArray<AActor*> PlayerStarts;
		UGameplayStatics::GetAllActorsOfClass(this, ATeamPlayerStart::StaticClass(), PlayerStarts);
		TArray<ATeamPlayerStart*> TeamPlayerStarts;
		for (auto Start : PlayerStarts)
		{
			ATeamPlayerStart* TeamStart = Cast<ATeamPlayerStart>(Start);
			if (TeamStart && TeamStart->Team == ArenaPS->GetTeam())
			{
				TeamPlayerStarts.Add(TeamStart);
			}
		}
		if (TeamPlayerStarts.Num() > 0)
		{
			ATeamPlayerStart* ChosenPlayerStart = TeamPlayerStarts[FMath::RandRange(0, TeamPlayerStarts.Num() - 1)];
			SetActorLocationAndRotation(
				ChosenPlayerStart->GetActorLocation(),
				ChosenPlayerStart->GetActorRotation()
			);
		}
	}
}

void AGASPPlayerCharacter::MulticastLostTheLead_Implementation()
{
	if (CrownComponent)
	{
		CrownComponent->DestroyComponent();
	}
}

void AGASPPlayerCharacter::MulticastGainedTheLead_Implementation()
{
	if (CrownSystem == nullptr) return;
	if (CrownComponent == nullptr)
	{
		CrownComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			CrownSystem,
			CharacterMesh,
			FName(),
			GetActorLocation() + FVector(0.f, 0.f, 110.f),
			GetActorRotation(),
			EAttachLocation::KeepWorldPosition,
			false
		);
	}
	if (CrownComponent)
	{
		CrownComponent->Activate();
	}
}


void AGASPPlayerCharacter::SetTeamColor(ETeam Team)
{
	if (CharacterMesh == nullptr || OriginalMaterial == nullptr) return;
	switch (Team)
	{
	case ETeam::ET_NoTeam:
		CharacterMesh->SetMaterial(0, OriginalMaterial);
		DissolveMaterialInstance01 = BlueDissolveMatInst;
		DissolveMaterialInstance02 = BlueDissolveMatInst;
		break;
	case ETeam::ET_BlueTeam:
		CharacterMesh->SetMaterial(0, BlueMaterial);
		DissolveMaterialInstance01 = BlueDissolveMatInst;
		DissolveMaterialInstance02 = BlueDissolveMatInst;
		break;
	case ETeam::ET_RedTeam:
		CharacterMesh->SetMaterial(0, RedMaterial);
		DissolveMaterialInstance01 = RedDissolveMatInst;
		DissolveMaterialInstance02 = RedDissolveMatInst;
		break;
	}
}


#pragma region UI

void AGASPPlayerCharacter::UpdateHUDHealth()
{
	// CRITICAL FIX: Do not use the cached GASPPlayerController. 
	// Check who is CURRENTLY driving this body! If it's an AI, this cast fails and the UI is ignored.
	if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(GetController()))
	{
		PC->SetHUDHealth(Health, MaxHealth);
	}
}

void AGASPPlayerCharacter::Multicast_ShowOverheadHealth_Implementation(float NewHealth)
{
	if (IsLocallyControlled()) return;

	if (!OverheadHealthWidget) return;

	if (UPlayerOverheadHealthWidget* HealthWidget =
		Cast<UPlayerOverheadHealthWidget>(OverheadHealthWidget->GetUserWidgetObject()))
	{
		HealthWidget->UpdateHealthBar(NewHealth, MaxHealth);
	}

	OverheadHealthWidget->SetVisibility(true);

	GetWorldTimerManager().ClearTimer(HideHealthTimer);

	GetWorldTimerManager().SetTimer(
		HideHealthTimer,
		this,
		&AGASPPlayerCharacter::HideOverheadHealth,
		3.f
	);
}

void AGASPPlayerCharacter::HideOverheadName()
{
	if (OverheadNameWidget)
	{
		OverheadNameWidget->SetVisibility(false);
	}
}

void AGASPPlayerCharacter::HideOverheadHealth()
{
	if (OverheadHealthWidget)
	{
		OverheadHealthWidget->SetVisibility(false);
	}
}

void AGASPPlayerCharacter::UpdateHUDAmmo()
{
	// Only update the main screen ammo if a human is driving this body.
	if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(GetController()))
	{
		if (CombatComponent && CombatComponent->GetPrimaryWeapon())
		{
			PC->SetHUDCarriedAmmo(CombatComponent->CarriedAmmo);
			PC->SetHUDWeaponAmmo(CombatComponent->GetPrimaryWeapon()->GetAmmo());
		}
	}
}

void AGASPPlayerCharacter::OnPlayerStateInitialized()
{
	// 1. UNIVERSAL LOGIC: Works for Co-op, Arena, and any future modes
	if (AGASPCorePlayerState* CorePS = Cast<AGASPCorePlayerState>(GetPlayerState()))
	{
		// Cache the name permanently into the body!
		//CharacterName = CorePS->GetPlayerName(); 
	}

	// 2. ARENA-SPECIFIC LOGIC: Only fires if we are playing Deathmatch
	if (AGASPArenaPlayerState* ArenaPS = Cast<AGASPArenaPlayerState>(GetPlayerState()))
	{
		ArenaPS->AddToScore(0.f);
		ArenaPS->AddToDefeats(0);
		SetTeamColor(ArenaPS->GetTeam());
	}
	
	SetSpawnPoint();

	// 3. UI UPDATE
	if (OverheadNameWidget)
	{
		if (UPlayerOverheadNameWidget* NameWidget = Cast<UPlayerOverheadNameWidget>(OverheadNameWidget->GetUserWidgetObject()))
		{
			// Now we use our cached variable!
			NameWidget->SetDisplayText(CharacterName);
		}
	}
	
	RefreshHUDWithFullSquad();
}


#pragma endregion UI

#pragma region PickupInterface

void AGASPPlayerCharacter::OnPickupBegin_Implementation(AActor* PickupActor)
{
	// Remove AItem cast — check interface instead
	if (!PickupActor || !CombatComponent) return;

	if (PickupActor->GetClass()->ImplementsInterface(
		UInteractionInterface::StaticClass()))
	{
		InteractionComponent->NearbyItem.AddUnique(PickupActor);
	}
}

void AGASPPlayerCharacter::OnPickupEnd_Implementation(
	AActor* PickupActor)
{
	if (!PickupActor) return;

	InteractionComponent->NearbyItem.Remove(PickupActor);

	if (InteractionComponent->FocusedItem == PickupActor)
	{
		ClearFocusedItem();
	}
}

#pragma endregion PickupInterface

#pragma region CombatInterface

bool AGASPPlayerCharacter::GetIsGameplayDisbled_Implementation()
{
	return bDisableGameplay;
}

#pragma endregion CombatInterface

#pragma region InteractWithCrosshairsInterface

void AGASPPlayerCharacter::OnCrosshairFocused()
{
	if (!OverheadNameWidget) return;
	OverheadNameWidget->SetVisibility(true);
	GetWorldTimerManager().ClearTimer(HideNameTimer);
	GetWorldTimerManager().SetTimer(HideNameTimer, this,
		&AGASPPlayerCharacter::HideOverheadName, 3.f);
}

void AGASPPlayerCharacter::OnCrosshairUnfocused()
{
	HideOverheadName();
}

#pragma endregion InteractWithCrosshairsInterface

#pragma region Interaction

void AGASPPlayerCharacter::UpdateInteractionFocus()
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

void AGASPPlayerCharacter::SetFocusedItem(AActor* NewItem)
{
	if (InteractionComponent->FocusedItem == NewItem) return;

	ClearFocusedItem();
	InteractionComponent->FocusedItem = NewItem;
    
	// We removed the old interface call here because UpdateInteractionFocus 
	// now handles pushing the text to the HUD on the exact frame it focuses!
}

void AGASPPlayerCharacter::ClearFocusedItem()
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

void AGASPPlayerCharacter::OnEquipItem_Implementation(AActor* Item)
{
	if (!CombatComponent || !Item) return;

	AItem* WeaponItem = Cast<AItem>(Item);
	if (WeaponItem)
	{
		CombatComponent->ServerEquip(WeaponItem);
	}
}
#pragma endregion Interaction

