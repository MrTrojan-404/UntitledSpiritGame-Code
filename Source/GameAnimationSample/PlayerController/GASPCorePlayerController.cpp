// Fill out your copyright notice in the Description page of Project Settings.


#include "GASPCorePlayerController.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "EnhancedInputComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameAnimationSample/AI/Character/Companion/GASPCompanionCharacter.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"
#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "GameAnimationSample/Component/DashComponent.h"
#include "GameAnimationSample/Component/DownedComponent.h"
#include "GameAnimationSample/Component/InteractionComponent.h"
#include "GameAnimationSample/Component/SquadCommandComponent.h"
#include "GameAnimationSample/GameMode/GASPCoopGameMode.h"
#include "GameAnimationSample/Objective/ObjectiveSubsystem.h"
#include "GameAnimationSample/Objective/Checkpoint/CheckpointActor.h"
#include "GameAnimationSample/SpellAbility/SpellComponent.h"
#include "GameAnimationSample/Subsystem/StealthSubsystem.h"
#include "GameAnimationSample/UI/HUD/GASPHUD.h"
#include "GameAnimationSample/UI/Widgets/CharacterOverlay.h"
#include "GameAnimationSample/UI/Widgets/ReturnToMainMenu.h"
#include "GameAnimationSample/UI/Widgets/COOP/WeaponRadialMenu/WeaponRadialMenu.h"
#include "Kismet/GameplayStatics.h"

AGASPCorePlayerController::AGASPCorePlayerController()
{
	// Give the Controller the tactical radio!
	SquadCommandComponent = CreateDefaultSubobject<USquadCommandComponent>(TEXT("SquadCommandComponent"));
}

void AGASPCorePlayerController::BeginPlay()
{
	Super::BeginPlay();

	GASPHUD = Cast<AGASPHUD>(GetHUD());
}

void AGASPCorePlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    
	CheckTimeSync(DeltaTime);
	PollInit();

	// ── Stealth detection HUD — server polls subsystem, pushes to client ──
	if (HasAuthority())
	{
		StealthHUDUpdateTimer += DeltaTime;
		if (StealthHUDUpdateTimer >= StealthHUDUpdateInterval)
		{
			StealthHUDUpdateTimer = 0.f;

			if (UStealthSubsystem* Sub = GetWorld()->GetSubsystem<UStealthSubsystem>())
			{
				for (const auto& Pair : Sub->GetActiveDetections())
				{
					if (AGASPAIController* AICon = Cast<AGASPAIController>(Pair.Key))
					{
						// Use our permanent cached body, because GetPawn() is NULL after death!
						if (APawn* EnemyPawn = AICon->CachedUIBody)
						{
							// Only filter by SuspectedActor if the meter is ACTIVE (> 0).
							// If the meter is 0, we WANT it to bypass this check so the clear signal broadcasts to everyone!
							if (Pair.Value.DetectionLevel > 0.f)
							{
								// If the enemy is looking at a Companion or another Player, ignore it!
								if (AICon->GetSuspectedActor() != GetPawn())
								{
									continue; 
								}
							}

							// Successfully send the update (or the 0.0f clear signal) to the Client
							Client_UpdateDetectionHUD(EnemyPawn, Pair.Value.DetectionLevel, Pair.Value.AlertPhase);
						}
					}
				}
	
				// Clean up the server memory NOW that the clients have received their updates
				Sub->RemoveZeroDetections();
			}
		}
	}
	
	// Update Local Downed Screen for ANY possessed body
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (UDownedComponent* Downed = BaseChar->FindComponentByClass<UDownedComponent>())
		{
			if (GASPHUD && GASPHUD->CharacterOverlay)
			{
				// Calculate the time left
				float TimeLeft = Downed->bIsBeingRevived 
					? FMath::Max(0.f, Downed->CalculateReviveTime() - Downed->ReviveProgress) 
					: Downed->GetBleedOutTimeRemaining();

				// Push it to the UI (this will auto-hide it if IsDowned() is false!)
				GASPHUD->CharacterOverlay->UpdateDownedStatus(
					Downed->IsDowned(), 
					Downed->bIsBeingRevived, 
					TimeLeft
				);
			}
		}
	}
}

void AGASPCorePlayerController::PollInit()
{
	// 1. Safety fetch the HUD just in case BeginPlay was too fast
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;

	// 2. Try to spawn the HUD (Arena will ignore this, Coop will spawn it!)
	if (GASPHUD && GASPHUD->CharacterOverlay == nullptr)
	{
		SetupInitialHUD();
	}

	// 3. Initialize the cached variables once the overlay finally exists
	if (CharacterOverlay == nullptr)
	{                
		if (GASPHUD && GASPHUD->CharacterOverlay)
		{
			CharacterOverlay = GASPHUD->CharacterOverlay;

			if (CharacterOverlay)
			{
				CharacterOverlay->HighPingImage->SetOpacity(0.f);
				CharacterOverlay->StopAnimation(CharacterOverlay->HighPingAnimation);

				if (GetPawn())
				{
					PushHUDFromPawn(GetPawn());
				}
				
				if (bInitializeCharacterOverlay) 
				{
					SetHUDHealth(HUDHealth, HUDMaxHealth);
					bInitializeCharacterOverlay = false;
				}
				if (bInitializeCarriedAmmo) 
				{
					SetHUDCarriedAmmo(HUDCarriedAmmo);
					bInitializeCarriedAmmo = false;
				}
				if (bInitializeWeaponAmmo) 
				{
					SetHUDWeaponAmmo(HUDWeaponAmmo);
					bInitializeWeaponAmmo = false;
				}

				AGASPPlayerCharacter* PlayerCharacter = Cast<AGASPPlayerCharacter>(GetPawn());
				if (PlayerCharacter && PlayerCharacter->GetCombatComponent())
				{
					SetHUDGrenades(PlayerCharacter->GetCombatComponent()->GetGrenades());
				}                
			}
		}
	}
}

void AGASPCorePlayerController::SetupInitialHUD()
{
	// By default, Core assumes we want the HUD immediately (For Coop/Campaign modes)
	if (GASPHUD && GASPHUD->CharacterOverlay == nullptr)
	{
		GASPHUD->AddCharacterOverlay();
	}
}

void AGASPCorePlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	if (IsLocalController())
	{
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
	}
}

void AGASPCorePlayerController::ClientNotifyMissionState_Implementation(FName State)
{
	// Trigger your victory/defeat UI here!
}

void AGASPCorePlayerController::ShowReturnToMainMenu()
{
	if (ReturnToMainMenuWidget == nullptr) return;
    
	if (ReturnToMainMenu == nullptr)
	{
		ReturnToMainMenu = CreateWidget<UReturnToMainMenu>(this, ReturnToMainMenuWidget);
	}
    
	if (ReturnToMainMenu)
	{
		if (!ReturnToMainMenu->IsInViewport())
		{
			ReturnToMainMenu->MenuSetup();
		}
		else
		{
			ReturnToMainMenu->MenuTearDown();
		}
	}
}

void AGASPCorePlayerController::Client_ShowHitMarker_Implementation(bool bHeadshot)
{
	if (GASPHUD && GASPHUD->CharacterOverlay)
	{
		GASPHUD->CharacterOverlay->ShowHitMarker(bHeadshot);
	}

	if (HitMarkerSound)
	{
		UGameplayStatics::PlaySound2D(this, HitMarkerSound);
	}
}

void AGASPCorePlayerController::SetHUDHealth(float Health, float MaxHealth)
{
	// ALWAYS update the cache so PollInit has fresh data if it fires
	HUDHealth = Health;
	HUDMaxHealth = MaxHealth;

	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	bool bHUDValid = GASPHUD && GASPHUD->CharacterOverlay && GASPHUD->CharacterOverlay->HealthBar && GASPHUD->CharacterOverlay->HealthText;
    
	if (bHUDValid)
	{
		const float HealthPercent = Health / MaxHealth;
		GASPHUD->CharacterOverlay->HealthBar->SetPercent(HealthPercent);
		FString HealthText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Health), FMath::CeilToInt(MaxHealth));
		GASPHUD->CharacterOverlay->HealthText->SetText(FText::FromString(HealthText));
	}
	else
	{
		bInitializeCharacterOverlay = true;
	}
}

void AGASPCorePlayerController::SetHUDWeaponAmmo(int32 Ammo)
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	bool bHUDValid = GASPHUD && GASPHUD->CharacterOverlay && GASPHUD->CharacterOverlay->WeaponAmmoAmount;
	if (bHUDValid)
	{
		FString AmmoText = FString::Printf(TEXT("%d / "), Ammo);
		GASPHUD->CharacterOverlay->WeaponAmmoAmount->SetText(FText::FromString(AmmoText));
	}
	else
	{
		bInitializeWeaponAmmo = true;
		HUDWeaponAmmo = Ammo;
	}

	// --- NEW: Keep the 3D Hologram in sync! ---
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			if (AWeapon* Primary = Combat->GetPrimaryWeapon())
			{
				int32 Carried = Combat->CarriedAmmoMap.Contains(Primary->GetWeaponType()) ? Combat->CarriedAmmoMap[Primary->GetWeaponType()] : 0;
				Primary->Update3DAmmoUI(Carried);
			}
		}
	}
}

void AGASPCorePlayerController::SetHUDCarriedAmmo(int32 Ammo)
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	bool bHUDValid = GASPHUD && GASPHUD->CharacterOverlay && GASPHUD->CharacterOverlay->CarriedAmmoAmount;
	if (bHUDValid)
	{
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		GASPHUD->CharacterOverlay->CarriedAmmoAmount->SetText(FText::FromString(AmmoText));
	}
	else
	{
		bInitializeCarriedAmmo = true;
		HUDCarriedAmmo = Ammo;
	}

	// --- NEW: Keep the 3D Hologram in sync! ---
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			if (AWeapon* Primary = Combat->GetPrimaryWeapon())
			{
				Primary->Update3DAmmoUI(Ammo);
			}
		}
	}
}

void AGASPCorePlayerController::SetHUDGrenades(int32 Grenades)
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	bool bHUDValid = GASPHUD &&
		GASPHUD->CharacterOverlay &&
		GASPHUD->CharacterOverlay->GrenadesText;
	if (bHUDValid)
	{
		FString GrenadesText = FString::Printf(TEXT("%d"), Grenades);
		GASPHUD->CharacterOverlay->GrenadesText->SetText(FText::FromString(GrenadesText));
	}
	else
	{
		HUDGrenades = Grenades;
	}
}

float AGASPCorePlayerController::GetServerTime()
{
	if (HasAuthority()) return GetWorld()->GetTimeSeconds();
	else return GetWorld()->GetTimeSeconds() + ClientServerDelta;
}

void AGASPCorePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(InPawn);
	if (BaseChar)
	{
		if (BaseChar->GetCombatComponent())
		{
			SetHUDGrenades(BaseChar->GetCombatComponent()->GetGrenades());
		}
		SetHUDHealth(BaseChar->GetHealth(), BaseChar->GetMaxHealth());

		// Refresh the Squad List
		if (AGASPPlayerCharacter* PlayerChar = Cast<AGASPPlayerCharacter>(BaseChar))
		{
			PlayerChar->RefreshHUDWithFullSquad();
		}
	}
}
void AGASPCorePlayerController::CheckTimeSync(float DeltaTime)
{
	TimeSyncRunningTime += DeltaTime;
	if (IsLocalController() && TimeSyncRunningTime > TimeSyncFrequency)
	{
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
		TimeSyncRunningTime = 0.f;
	}
}

void AGASPCorePlayerController::ServerRequestServerTime_Implementation(float TimeOfClientRequest)
{
	float ServerTimeOfReceipt = GetWorld()->GetTimeSeconds();
	ClientReportServerTime(TimeOfClientRequest, ServerTimeOfReceipt);
}

void AGASPCorePlayerController::ClientReportServerTime_Implementation(float TimeOfClientRequest, float TimeServerReceivedClientRequest)
{
	float RoundTripTime = GetWorld()->GetTimeSeconds() - TimeOfClientRequest;
	SingleTripTime = 0.5f * RoundTripTime;
	float CurrentServerTime = TimeServerReceivedClientRequest + SingleTripTime;
	ClientServerDelta = CurrentServerTime - GetWorld()->GetTimeSeconds();
}

void AGASPCorePlayerController::ServerReportPingStatus_Implementation(bool bHighPing)
{
	HighPingDelegate.Broadcast(bHighPing);
}

void AGASPCorePlayerController::ServerRestartLevel_Implementation()
{
	// Only the server can execute ServerTravel
	if (HasAuthority())
	{
		GetWorld()->ServerTravel("?Restart", false);
	}
}

void AGASPCorePlayerController::ServerRestartCheckpoint_Implementation()
{
	if (AGASPCoopGameMode* CoopGM = Cast<AGASPCoopGameMode>(GetWorld()->GetAuthGameMode()))
	{
		// 1. If we are playing Borderlands-style, let the player individually respawn themselves via the menu!
		if (CoopGM->RespawnRule == ERespawnRule::IndividualRespawn)
		{
			if (CoopGM->LatestCheckpoint)
			{
				CoopGM->LatestCheckpoint->RestoreIndividual(this);
			}
		}
		// 2. If we are playing Left4Dead-style, ONLY the Host is allowed to manually rewind time for the whole squad!
		else if (HasAuthority()) 
		{
			CoopGM->ForceRestartCheckpoint();
		}
	}
}
void AGASPCorePlayerController::ServerTagTeamSwitch_Implementation()
{
	// 1. CHECK COOLDOWN
	if (bTagTeamOnCooldown) return;

	AGASPBaseCharacter* OldBody = Cast<AGASPBaseCharacter>(GetPawn());
	if (!OldBody) return;

	// 2. CHECK DASH 
	if (UDashComponent* DashComp = OldBody->FindComponentByClass<UDashComponent>())
	{
		if (DashComp->IsDashing()) return;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPBaseCharacter::StaticClass(), FoundActors);
	TArray<AGASPBaseCharacter*> ValidSquadMembers;

	for (AActor* Actor : FoundActors)
	{
		if (AGASPBaseCharacter* Char = Cast<AGASPBaseCharacter>(Actor))
		{
			if (Char->IsPlayerAlly())
			{
				UDownedComponent* Downed = Char->FindComponentByClass<UDownedComponent>();
				if (!Downed || !Downed->IsDowned())
				{
					AController* CharCon = Char->GetController();
					if (CharCon == this || Cast<AAIController>(CharCon))
					{
						ValidSquadMembers.Add(Char);
					}
				}
			}
		}
	}

	if (ValidSquadMembers.IsEmpty() || ValidSquadMembers.Num() == 1) return;

	int32 CurrentIndex = ValidSquadMembers.IndexOfByKey(OldBody);
	int32 NextIndex = (CurrentIndex != INDEX_NONE) ? (CurrentIndex + 1) % ValidSquadMembers.Num() : 0;
	
	AGASPBaseCharacter* TargetBody = ValidSquadMembers[NextIndex];
	if (!TargetBody || TargetBody == OldBody) return;

	AAIController* TargetAI = Cast<AAIController>(TargetBody->GetController());

	if (TargetAI)
	{
		// START COOLDOWN
		bTagTeamOnCooldown = true;
		GetWorldTimerManager().SetTimer(TagTeamCooldownTimer, this, &AGASPCorePlayerController::ClearTagTeamCooldown, TagTeamCooldown, false);

		// CACHE EXACT PHYSICAL STATE (Removed the 150 Z-Offset!)
		FVector SwapLocation = OldBody->GetActorLocation(); 
		FRotator SwapRotation = OldBody->GetActorRotation();
		FRotator CurrentControlRot = GetControlRotation();

		// INSTANTLY BANISH THE OLD BODY
		if (UCombatComponent* OldCombat = OldBody->GetCombatComponent())
		{
			OldCombat->HandleFire(false);
			OldCombat->SetAiming(false);
		}

		// Clear momentum 
		OldBody->CachedMovementInputVector = FVector::ZeroVector;
		if (USceneComponent* Root = OldBody->GetRootComponent())
		{
			Root->ComponentVelocity = FVector::ZeroVector;
		}
		
		OldBody->SetActorHiddenInGame(true);
		OldBody->bDisableGameplay = true;
		
		// Turn OFF collision before moving to ensure they don't act as a phantom wall in the arena
		OldBody->SetActorEnableCollision(false); 

		// FORCE the teleport, bypassing collision safety checks
		OldBody->SetActorLocationAndRotation(BanishLocation, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
		
		// Re-enable collision NOW that they are safely in the banish room
		OldBody->SetActorEnableCollision(true); 

		// WAKE THE NEW BODY
		TargetBody->SetActorHiddenInGame(false);
		TargetBody->SetActorEnableCollision(true);
		TargetBody->bDisableGameplay = false;

		// FORCE TELEPORT AND DISSOLVE IN
		TargetBody->SetActorLocationAndRotation(SwapLocation, SwapRotation, false, nullptr, ETeleportType::TeleportPhysics);
		TargetBody->Multicast_PlayTagTeamDissolve(false); // Dissolves from 0.55 down to -0.55

		// SWAP THE BRAINS
		TargetAI->UnPossess();
		UnPossess();

		Possess(TargetBody);
		SetControlRotation(CurrentControlRot);

		// GIVE THE AI TO THE BANISHED BODY AND PUT IT TO SLEEP
		TargetAI->Possess(OldBody);
		if (AGASPAIController* OldAICon = Cast<AGASPAIController>(TargetAI))
		{
			OldAICon->ClearFocus(EAIFocusPriority::Gameplay);
			OldAICon->ClearTarget();
			if (OldAICon->GetBrainComponent()) 
			{
				OldAICon->GetBrainComponent()->StopLogic("Banished Tag Team");
			}
		}

		// === PLAY VFX & SFX ===
		if (TagTeamSwitchVFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), TagTeamSwitchVFX, SwapLocation, SwapRotation);
		}
		if (TagTeamSwitchSFX)
		{
			UGameplayStatics::PlaySoundAtLocation(this, TagTeamSwitchSFX, SwapLocation);
		}

		// REFRESH UI
		if (GASPHUD)
		{
			if (GASPHUD->CharacterOverlay)
			{
				GASPHUD->CharacterOverlay->RemoveFromParent();
				GASPHUD->CharacterOverlay = nullptr;
			}
			CharacterOverlay = nullptr; 
			SetupInitialHUD();
		}
		PushHUDFromPawn(TargetBody);
		TargetBody->ActiveMouseSensitivity = TargetBody->BaseMouseSensitivity;

		Client_RefreshSquadUI(); 

		OnClientCharacterSwapped(TargetBody);
	}
}

void AGASPCorePlayerController::ClearTagTeamCooldown()
{
	bTagTeamOnCooldown = false;
}

void AGASPCorePlayerController::ServerSwitchCharacter_Implementation()
{
	AGASPCoopGameMode* CoopMode = Cast<AGASPCoopGameMode>(GetWorld()->GetAuthGameMode());
	if (!CoopMode) return;

	// Intercept the normal switch if Tag Team is active
	if (bTagTeamMode)
	{
		ServerTagTeamSwitch();
		return;
	}
	
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPBaseCharacter::StaticClass(), FoundActors);

	TArray<AGASPBaseCharacter*> ValidSquadMembers;

	for (AActor* Actor : FoundActors)
	{
		if (AGASPBaseCharacter* Char = Cast<AGASPBaseCharacter>(Actor))
		{
			if (Char->IsPlayerAlly())
			{
				UDownedComponent* Downed = Char->FindComponentByClass<UDownedComponent>();
				if (!Downed || !Downed->IsDowned())
				{
					// Only allow our current body, or bodies driven by AI!
					// This strictly prevents players from attempting to hijack each other.
					AController* CharCon = Char->GetController();
					if (CharCon == this || Cast<AAIController>(CharCon))
					{
						ValidSquadMembers.Add(Char);
					}
				}
			}
		}
	}

	// THE BAILOUT MATH
	if (ValidSquadMembers.IsEmpty()) return; // Nobody is alive!
    
    // If there is only 1 alive person, and it's US, we can't swap.
    if (ValidSquadMembers.Num() == 1 && ValidSquadMembers[0] == GetPawn()) return; 

    // Look for our specific body pointer (Cast ensures it matches the array type)
    int32 CurrentIndex = ValidSquadMembers.IndexOfByKey(Cast<AGASPBaseCharacter>(GetPawn()));
    int32 NextIndex = 0; 

    // If we are downed, CurrentIndex will be INDEX_NONE (-1). 
    // It will safely default to 0, swapping us into the first alive body!
    if (CurrentIndex != INDEX_NONE)
    {
        NextIndex = (CurrentIndex + 1) % ValidSquadMembers.Num(); 
    }

    AGASPBaseCharacter* TargetBody = ValidSquadMembers[NextIndex];
    if (!TargetBody || TargetBody == GetPawn()) return;

    APawn* OldBody = GetPawn();
    AAIController* AICon = Cast<AAIController>(TargetBody->GetController());

    if (OldBody && AICon)
    {
        // CHECK IF WE WERE MID-REVIVE 
        AGASPBaseCharacter* AllyBeingRevived = nullptr;
        for (AActor* Actor : FoundActors)
        {
            if (AGASPBaseCharacter* Char = Cast<AGASPBaseCharacter>(Actor))
            {
                if (UDownedComponent* Downed = Char->FindComponentByClass<UDownedComponent>())
                {
                    if (Downed->IsDowned() && Downed->GetCurrentReviver() == OldBody)
                    {
                        AllyBeingRevived = Char;
                        break;
                    }
                }
            }
        }

        // Force the server to clear the inputs on the old body
        if (AGASPBaseCharacter* OldBaseChar = Cast<AGASPBaseCharacter>(OldBody))
        {
            if (UCombatComponent* OldCombat = OldBaseChar->GetCombatComponent())
            {
                OldCombat->HandleFire(false);
                OldCombat->SetAiming(false);
            }
        }

        // Rip the brains out of both bodies
        AICon->UnPossess();
        UnPossess();

        // Put the Player Brain into the New Body
        Possess(TargetBody);
        Client_FinishCharacterSwap(TargetBody);
        
    	// Put the AI Brain into the Old Body
    	AICon->Possess(OldBody);

    	if (AGASPAIController* GASPAI = Cast<AGASPAIController>(AICon))
    	{
    		// INHERIT THE REVIVE ORDER
    		if (AllyBeingRevived)
    		{
    			// Force the AI to ignore combat and finish the revive we started!
    			GASPAI->IssueOrder(ECompanionOrder::ReviveAlly, FVector::ZeroVector, AllyBeingRevived);
    		}
    		else
    		{
    			// Normal Idle State
    			GASPAI->ClearTarget();
    			GASPAI->SetAIState(EAIState::Patrol);
    			GASPAI->IssueOrder(ECompanionOrder::StandBy, FVector::ZeroVector, nullptr);

    			//Force the abandoned body to hold its fire so it doesn't aggro the room!
    			GASPAI->IssueOrder(ECompanionOrder::HoldFire, FVector::ZeroVector, nullptr);

    			if (GASPAI->GetBlackboardComponent())
    			{
    				GASPAI->GetBlackboardComponent()->ClearValue(AGASPAIController::BB_LeaderActor);
    			}
    		}
    	}

        // 3. Transfer the tether for the REST of the squad
        TArray<AActor*> AllCompanions;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPBaseCharacter::StaticClass(), AllCompanions);
        
        for (AActor* Actor : AllCompanions)
        {
            if (Actor != OldBody && Actor != TargetBody) 
            {
                if (AGASPBaseCharacter* Companion = Cast<AGASPBaseCharacter>(Actor))
                {
                    if (Companion->IsPlayerAlly())
                    {
                        if (AGASPAIController* OtherAI = Cast<AGASPAIController>(Companion->GetController()))
                        {
                            if (OtherAI->GetBlackboardComponent())
                            {
                                AActor* CurrentLeader = Cast<AActor>(OtherAI->GetBlackboardComponent()->GetValueAsObject(AGASPAIController::BB_LeaderActor));
                                
                                if (CurrentLeader == OldBody)
                                {
                                    OtherAI->GetBlackboardComponent()->SetValueAsObject(AGASPAIController::BB_LeaderActor, TargetBody);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void AGASPCorePlayerController::Client_RefreshSquadUI_Implementation()
{
	if (GASPHUD && GASPHUD->CharacterOverlay)
	{
		TArray<AGASPBaseCharacter*> FullSquad;
		TArray<AActor*> FoundActors;
        
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPBaseCharacter::StaticClass(), FoundActors);
        
		for (AActor* Actor : FoundActors)
		{
			// Skip our own body!
			if (Actor != GetPawn()) 
			{
				if (AGASPBaseCharacter* BC = Cast<AGASPBaseCharacter>(Actor))
				{
					// One clean check grabs both Human Teammates and AI Companions
					if (BC->IsPlayerAlly())
					{
						FullSquad.Add(BC);
					}
				}
			}
		}

		GASPHUD->CharacterOverlay->RefreshSquadListUI(FullSquad);
	}
}

void AGASPCorePlayerController::Client_FinishCharacterSwap_Implementation(APawn* NewPawn)
{
    if (!NewPawn) return;

    // 1. Blueprint hook, Use if some logic needs to be called from Blueprint
    OnClientCharacterSwapped(NewPawn);

    // 2. Rebuild the overlay so all widget pointers are fresh
    if (GASPHUD)
    {
        if (GASPHUD->CharacterOverlay)
        {
            GASPHUD->CharacterOverlay->RemoveFromParent();
            GASPHUD->CharacterOverlay = nullptr;
        }
        CharacterOverlay = nullptr; // clear our cached pointer too
        SetupInitialHUD();
    }

    // 3. Push the new body's stats into the fresh overlay
    PushHUDFromPawn(NewPawn);

	// 4. Refresh squad list so the new body isn't listed as a teammate
	if (GASPHUD && GASPHUD->CharacterOverlay)
	{
		TArray<AGASPBaseCharacter*> FullSquad;
		TArray<AActor*> FoundActors;
        
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPBaseCharacter::StaticClass(), FoundActors);
        
		for (AActor* Actor : FoundActors)
		{
			if (Actor != NewPawn)
			{
				if (AGASPBaseCharacter* BC = Cast<AGASPBaseCharacter>(Actor))
				{
					// One clean check grabs both Players and Companions
					if (BC->IsPlayerAlly())
					{
						FullSquad.Add(BC);
					}
				}
			}
		}

		GASPHUD->CharacterOverlay->RefreshSquadListUI(FullSquad);
	}
	
	//Reset the new body's sensitivity
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(NewPawn))
	{
		BaseChar->ActiveMouseSensitivity = BaseChar->BaseMouseSensitivity;
	}
}

void AGASPCorePlayerController::Client_SyncObjective_Implementation(AObjectiveActor* ActiveObjective)
{
	// 1. Give the client's local subsystem the correct objective
	if (UObjectiveSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UObjectiveSubsystem>())
	{
		Subsystem->CurrentMainObjectiveActor = ActiveObjective;
	}

	// 2. Tell the UI to redraw itself with the new data
	if (GASPHUD && GASPHUD->CharacterOverlay)
	{
		// Assuming your Overlay has a function or a way to refresh the objective tracker!
		// For example: GASPHUD->CharacterOverlay->RefreshObjectiveUI();
	}
}

void AGASPCorePlayerController::PushHUDFromPawn(APawn* InPawn)
{
	AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(InPawn);
	if (!BaseChar) return;

	// Health & Medkits
	SetHUDHealth(BaseChar->GetHealth(), BaseChar->GetMaxHealth());
	SetHUDMedkits(BaseChar->GetMedkitCount());

	// Ammo + grenades
	UCombatComponent* Combat = BaseChar->GetCombatComponent();
	if (Combat)
	{
		SetHUDGrenades(Combat->GetGrenades());

		if (Combat->GetPrimaryWeapon())
		{
			SetHUDWeaponAmmo(Combat->GetPrimaryWeapon()->GetAmmo());
			SetHUDCarriedAmmo(Combat->CarriedAmmo);
		}
		else
		{
			SetHUDWeaponAmmo(0);
			SetHUDCarriedAmmo(0);
		}
	}

	// --- SPELLS ---
	if (USpellComponent* SpellComp = BaseChar->FindComponentByClass<USpellComponent>())
	{
		// Use the public variable and public Getter
		SetHUDEnergy(SpellComp->CurrentEnergySegments, SpellComp->GetMaxEnergySegments());
		
		// Use the math trick: Percent is 0.0 to 1.0, so if we pass 1.0 as the Max, the math perfectly matches!
		SetHUDUltimate(SpellComp->GetUltimateBarPercent(), 1.0f);
	}
}


void AGASPCorePlayerController::Client_StartMendCooldown_Implementation(float CooldownDuration)
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	
	if (GASPHUD && GASPHUD->CharacterOverlay)
	{
		GASPHUD->CharacterOverlay->StartMendCooldown(CooldownDuration);
	}
}

void AGASPCorePlayerController::Server_ToggleSuppressor_Implementation()
{
	// Fallback RPC path — ToggleSuppressor() already calls ServerToggleSuppressor
	// internally, so this RPC is reserved for future use (e.g. hotkey from spectator).
	ToggleSuppressorButtonPressed();
}

void AGASPCorePlayerController::Client_UpdateDetectionHUD_Implementation(
	AActor* DetectingEnemy, float DetectionLevel, uint8 AlertPhaseRaw)
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	if (GASPHUD && GASPHUD->CharacterOverlay)
	{
		// Pass the enemy actor into the overlay so it can calculate the angle!
		GASPHUD->CharacterOverlay->UpdateStealthHUD(DetectingEnemy, DetectionLevel, AlertPhaseRaw);
	}
}

void AGASPCorePlayerController::SetHUDUltimate(float CurrentCharge, float MaxCharge)
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	if (GASPHUD && GASPHUD->CharacterOverlay)
	{
		GASPHUD->CharacterOverlay->SetUltimateCharge(CurrentCharge, MaxCharge);
	}
}

void AGASPCorePlayerController::SetHUDEnergy(int32 CurrentSegments, int32 MaxSegments)
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	if (GASPHUD && GASPHUD->CharacterOverlay)
	{
		GASPHUD->CharacterOverlay->SetEnergySegments(CurrentSegments, MaxSegments);
	}
}

void AGASPCorePlayerController::SetHUDMedkits(int32 Medkits)
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	if (GASPHUD && GASPHUD->CharacterOverlay)
	{
		GASPHUD->CharacterOverlay->SetHUDMedkits(Medkits);
	}
}

#pragma region Inputs

void AGASPCorePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent)) 
    {
    	EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AGASPCorePlayerController::InteractButtonPressed);
    	EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &AGASPCorePlayerController::InteractButtonReleased);
    	EnhancedInputComponent->BindAction(SwapWeaponAction, ETriggerEvent::Triggered, this, &AGASPCorePlayerController::SwapButtonPressed);
		EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Triggered, this, &AGASPCorePlayerController::ReloadButtonPressed);
		EnhancedInputComponent->BindAction(ThrowGrenadeAction, ETriggerEvent::Triggered, this, &AGASPCorePlayerController::ThrowGrenadeButtonPressed);
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Triggered, this, &AGASPCorePlayerController::DropButtonPressed);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &AGASPCorePlayerController::AimButtonPressed);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &AGASPCorePlayerController::AimButtonReleased);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &AGASPCorePlayerController::FireButtonPressed);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &AGASPCorePlayerController::FireButtonReleased);
		EnhancedInputComponent->BindAction(MeleeAction, ETriggerEvent::Triggered, this, &AGASPCorePlayerController::MeleeButtonPressed);
    	EnhancedInputComponent->BindAction(PauseAction, ETriggerEvent::Triggered, this, &AGASPCorePlayerController::ShowReturnToMainMenu);
    	EnhancedInputComponent->BindAction(SwitchCharacterAction, ETriggerEvent::Triggered, this, &AGASPCorePlayerController::SwitchCharacterButtonPressed);
    	EnhancedInputComponent->BindAction(WeaponWheelAction, ETriggerEvent::Started, this, &AGASPCorePlayerController::WeaponWheelStarted);
    	EnhancedInputComponent->BindAction(WeaponWheelAction, ETriggerEvent::Completed, this, &AGASPCorePlayerController::WeaponWheelReleased);
    	EnhancedInputComponent->BindAction(ToggleSuppressorAction, ETriggerEvent::Triggered,this, &AGASPCorePlayerController::ToggleSuppressorButtonPressed);
    	EnhancedInputComponent->BindAction(ThrowDistractionAction, ETriggerEvent::Triggered,this, &AGASPCorePlayerController::ThrowDistractionButtonPressed);
    	EnhancedInputComponent->BindAction(SwapShoulderAction, ETriggerEvent::Triggered,this, &AGASPCorePlayerController::SwapShoulderButtonPressed);
    	EnhancedInputComponent->BindAction(MarkTargetAction, ETriggerEvent::Started, this, &AGASPCorePlayerController::MarkTargetButtonPressed);
    	EnhancedInputComponent->BindAction(ToggleFollowAction, ETriggerEvent::Started, this, &AGASPCorePlayerController::ToggleFollowButtonPressed);
    	EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Started, this, &AGASPCorePlayerController::DashButtonPressed);
    	EnhancedInputComponent->BindAction(ConsumeMedkitAction, ETriggerEvent::Started, this, &AGASPCorePlayerController::ConsumeMedkitButtonPressed);
    	EnhancedInputComponent->BindAction(Spell1Action, ETriggerEvent::Started, this, &AGASPCorePlayerController::Spell1ButtonPressed);
    	EnhancedInputComponent->BindAction(Spell2Action, ETriggerEvent::Started, this, &AGASPCorePlayerController::Spell2ButtonPressed);
    	EnhancedInputComponent->BindAction(Spell2Action, ETriggerEvent::Completed, this, &AGASPCorePlayerController::Spell2ButtonReleased);
    	EnhancedInputComponent->BindAction(UltimateAction, ETriggerEvent::Started, this, &AGASPCorePlayerController::UltimateButtonPressed);
    	
    }
}

// --- UNIVERSAL COMBAT INPUTS ---

void AGASPCorePlayerController::FireButtonPressed()
{
	//Intercept Left Click for Targeting
	if (SquadCommandComponent && SquadCommandComponent->CurrentTargetingMode != ETargetingMode::ETM_None)
	{
		SquadCommandComponent->ConfirmTargetLocation();
		return;
	}

	// Normal Firing Logic
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return; 
        
		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->HandleFire(true);
		}
	}
}

void AGASPCorePlayerController::FireButtonReleased()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return; // Guardrail!
        
		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->HandleFire(false);
		}
	}
}

void AGASPCorePlayerController::MarkTargetButtonPressed()
{
	USquadCommandComponent* Squad = FindComponentByClass<USquadCommandComponent>();
	if (!Squad) return;

	if (Squad->HasMarkedTarget())
	{
		if (Squad->IsMarkReady())
		{
			Squad->ExecuteMark(); // only fires if the 3s window has actually elapsed
		}
		// Still lining up — press is silently ignored. No exploit path.
		return;
	}

	AGASPBaseCharacter* PlayerChar = Cast<AGASPBaseCharacter>(GetPawn());
	if (!PlayerChar) return;

	FVector ViewLoc;
	FRotator ViewRot;
	GetPlayerViewPoint(ViewLoc, ViewRot);
	FVector TraceEnd = ViewLoc + (ViewRot.Vector() * MarkTraceRange);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(PlayerChar);

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc, TraceEnd, ECC_Visibility, Params))
	{
		if (AGASPBaseCharacter* HitChar = Cast<AGASPBaseCharacter>(Hit.GetActor()))
		{
			if (!HitChar->IsPlayerAlly())
			{
				Squad->TryMarkTarget(HitChar);
			}
		}
	}
}

void AGASPCorePlayerController::ConsumeMedkitButtonPressed()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		BaseChar->ConsumeMedkit();
	}
}

void AGASPCorePlayerController::Spell1ButtonPressed()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (USpellComponent* Spell = BaseChar->FindComponentByClass<USpellComponent>())
		{
			Spell->TryCastSpell1();
		}
	}
}

void AGASPCorePlayerController::Spell2ButtonPressed()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (USpellComponent* Spell = BaseChar->FindComponentByClass<USpellComponent>())
		{
			Spell->TryCastSpell2();
		}
	}
}

void AGASPCorePlayerController::Spell2ButtonReleased()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (USpellComponent* Spell = BaseChar->FindComponentByClass<USpellComponent>())
		{
			Spell->ReleaseSpell2();
		}
	}
}

void AGASPCorePlayerController::UltimateButtonPressed()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (USpellComponent* Spell = BaseChar->FindComponentByClass<USpellComponent>())
		{
			Spell->TryCastUltimate();
		}
	}
}

void AGASPCorePlayerController::AimButtonPressed()
{
	if (SquadCommandComponent && SquadCommandComponent->CurrentTargetingMode != ETargetingMode::ETM_None)
	{
		SquadCommandComponent->CancelTargetingMode();
		return;
	}

	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return; 
        
		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			if (Combat->bLocallyReloading) return;
			Combat->SetAiming(true);

			// --- HYBRID UI LOGIC ---
			if (Combat->GetPrimaryWeapon())
			{
				// If it's a Sniper, show the 2D Screen Text
				if (Combat->GetPrimaryWeapon()->GetWeaponType() == EWeaponType::EWT_SniperRifle)
				{
					GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
					if (GASPHUD && GASPHUD->CharacterOverlay)
					{
						GASPHUD->CharacterOverlay->SetAmmoUIVisibility(true);
					}
					
					// Camera setup for scope
					BaseChar->ActiveMouseSensitivity = BaseChar->SniperAimSens;
					BaseChar->PreAimShoulder = BaseChar->TargetShoulderSide;
					BaseChar->SetCameraShoulder(0.4f);
				}
				// If it's ANY OTHER WEAPON, show the 3D Hologram
				else
				{
					// Refresh the 3D Ammo value right before making it visible! 
					// This catches cases where the weapon replicated AFTER the ammo did on spawn.
					Combat->GetPrimaryWeapon()->Update3DAmmoUI(Combat->CarriedAmmo);
					
					Combat->GetPrimaryWeapon()->Set3DAmmoVisibility(true);
					BaseChar->ActiveMouseSensitivity = BaseChar->DefaultAimSensitivity;
				}
			}
		}
	}
}

void AGASPCorePlayerController::AimButtonReleased()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return; 
        
		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->SetAiming(false);
			
			// --- HIDE EVERYTHING TO BE SAFE ---
			if (Combat->GetPrimaryWeapon())
			{
				Combat->GetPrimaryWeapon()->Set3DAmmoVisibility(false);
			}
			
			GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
			if (GASPHUD && GASPHUD->CharacterOverlay)
			{
				GASPHUD->CharacterOverlay->SetAmmoUIVisibility(false);
			}
			
			// Reset Camera
			BaseChar->ActiveMouseSensitivity = BaseChar->BaseMouseSensitivity;

			if (Combat->GetPrimaryWeapon() && Combat->GetPrimaryWeapon()->GetWeaponType() == EWeaponType::EWT_SniperRifle)
			{
				BaseChar->SetCameraShoulder(BaseChar->PreAimShoulder);
			}
		}
	}
}

void AGASPCorePlayerController::ReloadButtonPressed()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return; // Guardrail!
        
		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->Reload();
		}
	}
}

void AGASPCorePlayerController::SwapButtonPressed()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return; // Guardrail!
        
		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->EquippedSwap();
		}
	}
}

void AGASPCorePlayerController::ThrowGrenadeButtonPressed()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;
        
		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->ThrowGrenade();
		}
	}
}

void AGASPCorePlayerController::ThrowDistractionButtonPressed()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return; 
        
		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->ThrowDistraction();
		}
	}
}

void AGASPCorePlayerController::MeleeButtonPressed()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return; 

		if (BaseChar->InteractionComponent && BaseChar->InteractionComponent->FocusedItem)
		{
			AActor* Target = BaseChar->InteractionComponent->FocusedItem;
			if (IInteractionInterface::Execute_CanPerformStealthTakedown(Target, BaseChar))
			{
				// FIX: Tell OUR Server Character to request the takedown!
				if (AGASPAICharacterBase* Enemy = Cast<AGASPAICharacterBase>(Target))
				{
					BaseChar->Server_RequestTakedown(Enemy);
				}
				return; // Exit early, we did a takedown!
			}
		}
        
		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->Melee();
		}
	}
}

void AGASPCorePlayerController::SwapShoulderButtonPressed()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		BaseChar->SwapCameraShoulder();
	}
}

void AGASPCorePlayerController::DashButtonPressed()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (UDashComponent* Dash = BaseChar->FindComponentByClass<UDashComponent>())
		{
			Dash->TryDash();
		}
	}
}

void AGASPCorePlayerController::ToggleFollowButtonPressed()
{
	USquadCommandComponent* Squad = FindComponentByClass<USquadCommandComponent>();
	if (!Squad) return;

	Squad->ToggleFollowStandBy();
}

void AGASPCorePlayerController::DropButtonPressed()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return; 
        
		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->ServerDropWeapon();
		}
	}
}
void AGASPCorePlayerController::InteractButtonPressed()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return; 

		// Find the interaction component on our current body and trigger it!
		if (UInteractionComponent* InteractComp = BaseChar->FindComponentByClass<UInteractionComponent>())
		{
			InteractComp->TryInteract();
		}
	}
}

void AGASPCorePlayerController::InteractButtonReleased()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return; 

		if (UInteractionComponent* InteractComp = BaseChar->FindComponentByClass<UInteractionComponent>())
		{
			InteractComp->TryInteractReleased(); // Calls the function we added previously!
		}
	}
}

void AGASPCorePlayerController::SwitchCharacterButtonPressed()
{
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		// FIX: Clear the UI prompt before we leave this body!
		BaseChar->ClearFocusedItem(); 

		// 1. Drop the gun and release the triggers
		if (UCombatComponent* CombatComp = BaseChar->GetCombatComponent())
		{
			CombatComp->HandleFire(false);
			CombatComp->SetAiming(false); 
		}
	}

	// 3. Swap the brains
	ServerSwitchCharacter();
}

void AGASPCorePlayerController::WeaponWheelStarted()
{
	AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn());
	if (!BaseChar || BaseChar->bDisableGameplay || !WeaponWheelClass) return;

	UCombatComponent* CombatComp = BaseChar->GetCombatComponent();
	if (CombatComp) CombatComp->bShowCrosshairs = false;

	if (!WeaponWheelInstance)
	{
		WeaponWheelInstance = CreateWidget<UWeaponRadialMenu>(this, WeaponWheelClass);
	}

	if (WeaponWheelInstance && !WeaponWheelInstance->IsInViewport())
	{
		// Give the UI a reference to the current body so it can read the inventory!
		// Note: If you want companions to use the wheel, you may need to update 
		// the Widget to expect a BaseCharacter instead of a PlayerCharacter!
		WeaponWheelInstance->BaseCharacter = BaseChar;

		WeaponWheelInstance->AddToViewport();
        
		SetShowMouseCursor(true);
		SetInputMode(FInputModeGameAndUI());
        
		int32 ViewX, ViewY;
		GetViewportSize(ViewX, ViewY);
		SetMouseLocation(ViewX / 2, ViewY / 2);
	}
}

void AGASPCorePlayerController::WeaponWheelReleased()
{
	UCombatComponent* CombatComp = nullptr;
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		CombatComp = BaseChar->GetCombatComponent();
		if (CombatComp) CombatComp->bShowCrosshairs = true;
	}

	if (WeaponWheelInstance && WeaponWheelInstance->IsInViewport())
	{
		// Tell the Combat Component of the body we are currently driving to equip the weapon!
		if (WeaponWheelInstance->HoveredWeapon && CombatComp && !WeaponWheelInstance->bJustEquippedToSecondary)
		{
			CombatComp->ServerEquipFromMenuToPrimary(WeaponWheelInstance->HoveredWeapon);
		}

		WeaponWheelInstance->RemoveFromParent();
        
		SetShowMouseCursor(false);
		SetInputMode(FInputModeGameOnly());
	}
}

void AGASPCorePlayerController::ToggleSuppressorButtonPressed()
{
	// Works whether the player is controlling a PlayerCharacter or a Companion body,
	// because GetPawn() always returns whatever we currently possess.
	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(GetPawn()))
	{
		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			if (AWeapon* PrimaryWeapon = Combat->GetPrimaryWeapon())
			{
				if (!PrimaryWeapon->HasSuppressor()) return; // weapon type can't suppress
 
				// ToggleSuppressor handles the authority check internally
				PrimaryWeapon->ToggleSuppressor();
			}
		}
	}
}

#pragma endregion Inputs
