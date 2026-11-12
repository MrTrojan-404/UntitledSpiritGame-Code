#include "GASPAirlockEntrance.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Components/BoxComponent.h"
#include "GASPArenaManager.h"
#include "EngineUtils.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameAnimationSample/AI/Character/Companion/GASPCompanionCharacter.h"
#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"

AGASPAirlockEntrance::AGASPAirlockEntrance()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// 1. Setup the Airlock Room Box
	AirlockVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("AirlockVolume"));
	RootComponent = AirlockVolume;
	AirlockVolume->SetCollisionProfileName(TEXT("Trigger"));

	// 2. Setup the Arena Entry Box (Push this further inside the boss room)
	ArenaTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ArenaTrigger"));
	ArenaTrigger->SetupAttachment(RootComponent);
	ArenaTrigger->SetCollisionProfileName(TEXT("Trigger"));

	TeleportDropPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("TeleportDropPoint"));
	TeleportDropPoint->SetupAttachment(RootComponent);
}

void AGASPAirlockEntrance::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		AirlockVolume->OnComponentBeginOverlap.AddDynamic(this, &AGASPAirlockEntrance::OnAirlockOverlap);
		ArenaTrigger->OnComponentBeginOverlap.AddDynamic(this, &AGASPAirlockEntrance::OnArenaOverlap);
	}
}

// Dynamically counts players. Works perfectly for 1 Player Tag-Team, or 2 Player Co-op!
bool AGASPAirlockEntrance::AreAllPlayersInVolume(UBoxComponent* Volume)
{
	int32 TotalPlayers = 0;
	int32 PlayersInVolume = 0;

	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Pawn = *It;
		if (Pawn && Pawn->IsPlayerControlled())
		{
			TotalPlayers++;
			if (Volume->IsOverlappingActor(Pawn))
			{
				PlayersInVolume++;
			}
		}
	}

	return (TotalPlayers > 0 && TotalPlayers == PlayersInVolume);
}

void AGASPAirlockEntrance::OnAirlockOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bAirlockLocked) return;

	if (AreAllPlayersInVolume(AirlockVolume))
	{
		bAirlockLocked = true;
		Multicast_Stage1_LockAirlock(); 

		// === INITIALIZE TAG TEAM (Solo Player Only) ===
		if (HasAuthority())
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				if (AGASPCorePlayerController* GASPPC = Cast<AGASPCorePlayerController>(PC))
				{
					// Enable the Tag Team flag
					GASPPC->bTagTeamMode = true;

					// Pass the level-specific banish coordinates to the controller!
					if (BanishPointActor)
					{
						GASPPC->BanishLocation = BanishPointActor->GetActorLocation();
					}

					if (AGASPPlayerCharacter* PlayerChar = Cast<AGASPPlayerCharacter>(PC->GetPawn()))
					{
						// Instantly banish the active companion so the fight starts 1v1
						if (PlayerChar->ActiveCompanion)
						{
							AGASPBaseCharacter* Comp = Cast<AGASPBaseCharacter>(PlayerChar->ActiveCompanion);
							
							Comp->SetActorHiddenInGame(true);
							Comp->bDisableGameplay = true;
							
							// Leave collision ON so they don't fall through the floor!
							Comp->SetActorEnableCollision(true);

							// Send them to the Target Point!
							FVector TargetDrop = BanishPointActor ? BanishPointActor->GetActorLocation() : FVector(0.f, 0.f, -10000.f);
							Comp->SetActorLocationAndRotation(TargetDrop, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);

							if (AAIController* AICon = Cast<AAIController>(Comp->GetController()))
							{
								if (AICon->GetBrainComponent())
								{
									AICon->GetBrainComponent()->StopLogic("Banish Tag Team Initial");
								}
							}
						}
					}
				}
			}
		}
	}
}

void AGASPAirlockEntrance::OnArenaOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bAirlockLocked || bFightStarted) return;
	if (!HasAuthority()) return; // Server only

	APawn* OverlappedPawn = Cast<APawn>(OtherActor);
	if (OverlappedPawn && OverlappedPawn->IsPlayerControlled())
	{
		bFightStarted = true;
		Multicast_Stage2_StartFight(); // Close Door B on all screens

		// --- CO-OP ONLY TELEPORT ---
		TArray<APawn*> PawnsToTeleport;
		if (AGameStateBase* GameState = GetWorld()->GetGameState())
		{
			for (APlayerState* PS : GameState->PlayerArray)
			{
				if (PS && !PS->IsABot() && PS->GetPawn()) 
				{
					APawn* ClientPawn = PS->GetPawn();
					if (ClientPawn != OverlappedPawn && !ArenaTrigger->IsOverlappingActor(ClientPawn))
					{
						PawnsToTeleport.Add(ClientPawn);
					}
				}
			}
		}

		// Wait until the physics step is completely finished, THEN teleport everyone safely!
		if (PawnsToTeleport.Num() > 0)
		{
			FVector DropLoc = TeleportDropPoint->GetComponentLocation() + FVector(0.f, 0.f, 110.f);
			
			FTimerDelegate TeleportDel;
			TeleportDel.BindLambda([PawnsToTeleport, DropLoc]()
			{
				for (APawn* P : PawnsToTeleport)
				{
					if (IsValid(P)) P->SetActorLocation(DropLoc, false, nullptr, ETeleportType::TeleportPhysics);
				}
			});
			GetWorld()->GetTimerManager().SetTimerForNextTick(TeleportDel);
		}
		
		// Wake up the Boss!
		if (ArenaManager)
		{
			ArenaManager->StartBossFight(OverlappedPawn);
		}
	}
}

// --- MULTICAST IMPLEMENTATIONS ---

void AGASPAirlockEntrance::Multicast_Stage1_LockAirlock_Implementation()
{
	BlueprintCloseDoorA();
	BlueprintOpenDoorB();
}

void AGASPAirlockEntrance::Multicast_Stage2_StartFight_Implementation()
{
	BlueprintCloseDoorB();
}