#include "GASPArenaManager.h"
#include "GASPBossBase.h"
#include "GASPBossController.h"
#include "Boss1/GASPStageGenerator.h"
#include "Boss1/GASPStageShield.h"
#include "GameAnimationSample/Actor/Turret/LaserTurret.h"
#include "Kismet/GameplayStatics.h"

AGASPArenaManager::AGASPArenaManager()
{
	PrimaryActorTick.bCanEverTick = false;
	CurrentPhase = 0;
}

void AGASPArenaManager::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		DisabledGenerators = 0;

		// Bind the Manager to listen to every Generator in the array
		for (AGASPStageGenerator* Generator : StageGenerators)
		{
			if (Generator)
			{
				Generator->OnGeneratorDisabled.AddDynamic(this, &AGASPArenaManager::HandleGeneratorDisabled);
			}
		}
	}
}

void AGASPArenaManager::HandleGeneratorDisabled(AGASPStageGenerator* DisabledGenerator)
{
	if (!HasAuthority()) return;

	DisabledGenerators++;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, FString::Printf(TEXT("Generators Disabled: %d / %d"), DisabledGenerators, StageGenerators.Num()));
	}

	// If all generators are disabled, drop the shield and advance the phase!
	if (DisabledGenerators >= StageGenerators.Num())
	{
		if (MainStageShield)
		{
			MainStageShield->SetShieldActive(false);
		}
		AdvancePhase();
	}
}

void AGASPArenaManager::ResetAllGenerators()
{
	if (!HasAuthority()) return;

	DisabledGenerators = 0;

	for (AGASPStageGenerator* Generator : StageGenerators)
	{
		if (Generator)
		{
			Generator->ResetGenerator();
		}
	}

	// RAISE THE SHIELD
	if (MainStageShield)
	{
		MainStageShield->SetShieldActive(true);
	}
}

void AGASPArenaManager::StartBossFight(AActor* InstigatorPlayer)
{
	if (!InstigatorPlayer) return;

	// Lock Bosses onto the player
	for (AGASPBossBase* Boss : ArenaBosses)
	{
		if (Boss)
		{
			if (AGASPBossController* BossController = Cast<AGASPBossController>(Boss->GetController()))
			{
				BossController->LockOntoTarget(InstigatorPlayer);
			}
		}
	}

	// Grab EVERY character in the world (Players AND AI Companions)
	TArray<AActor*> FoundCharacters;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPBaseCharacter::StaticClass(), FoundCharacters);
	
	TArray<AGASPBaseCharacter*> ValidArenaTargets;
	for (AActor* Actor : FoundCharacters)
	{
		// Add them to the target list as long as they aren't the boss!
		if (!Actor->IsA(AGASPBossBase::StaticClass()))
		{
			if (AGASPBaseCharacter* Char = Cast<AGASPBaseCharacter>(Actor))
			{
				ValidArenaTargets.Add(Char);
			}
		}
	}

	// Hand the complete list of targets to every Turret!
	if (ValidArenaTargets.Num() > 0)
	{
		for (ALaserTurret* Turret : InnerTurrets)
		{
			if (Turret)
			{
				Turret->ActivateTurret(ValidArenaTargets);
			}
		}
	}

	BlueprintStartFight();
}

void AGASPArenaManager::AdvancePhase()
{
	CurrentPhase++;
	
	// Broadcast to anyone listening (e.g., UI Healthbars)
	OnPhaseChanged.Broadcast();

	// Trigger the Blueprint event for specific phase logic (e.g., dropping Commander 2)
	BlueprintTriggerPhase(CurrentPhase);
}