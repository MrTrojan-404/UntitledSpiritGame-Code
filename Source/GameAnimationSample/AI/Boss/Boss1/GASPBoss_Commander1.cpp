#include "GASPBoss_Commander1.h"

#include "BrainComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"

AGASPBoss_Commander1::AGASPBoss_Commander1()
{
	bIsHeavyEnemy = true; // Use Heavy staggers
	
	// Boss Stats!
	MaxHealth = 3000.f;
	Health = 3000.f;

	SightRadius = 4000.f;
	HearingRange = 5000.f;

	// Give him a unique combat style ID so the master BT knows to route him to the Boss Subtree
	CombatStyle = 4; 
}

void AGASPBoss_Commander1::BeginPlay()
{
	// 1. Save where we placed him in the editor BEFORE we banish him!
	ArenaSpawnTransform = GetActorTransform();

	Super::BeginPlay();

	// If they are not supposed to start awake (Commander 2), put them in stasis!
	if (!bStartsAwake)
	{
		SetBossActiveState(false);
	}
}

void AGASPBoss_Commander1::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Fill the health bar smoothly every frame!
	if (bIsRegenerating && Health < MaxHealth)
	{
		Health += CurrentRegenRate * DeltaTime;
		Health = FMath::Min(Health, MaxHealth);
		
		// If you want the UI to visually update while he heals:
		// OnRep_Health(Health); 
	}
}

void AGASPBoss_Commander1::SetBossActiveState(bool bIsActive)
{
	// 1. HANDLE TELEPORTATION FIRST
	if (bIsActive)
	{
		// Teleport him back into the arena exactly where he belongs
		SetActorTransform(ArenaSpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		// Banish him to the void (far away from the player and the level)
		SetActorLocation(FVector(500000.f, 500000.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	}

	// 2. TOGGLE VISUALS & COLLISION
	SetActorHiddenInGame(!bIsActive);
	SetActorEnableCollision(bIsActive);
	
	// We no longer mess with Mover Tick. Let it tick harmlessly in the void.
	bDisableGameplay = !bIsActive;

	// 3. TOGGLE THE BRAIN
	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		if (bIsActive)
		{
			if (AICon->GetBrainComponent())
			{
				AICon->GetBrainComponent()->RestartLogic();
			}
		}
		else
		{
			if (AICon->GetBrainComponent())
			{
				AICon->GetBrainComponent()->StopLogic("Boss is Dormant");
			}
		}
	}
}

void AGASPBoss_Commander1::StartDownedRegen(bool bIsCoop)
{
	if (Health >= MaxHealth) return; // Don't regen if he's already full!

	bIsRegenerating = true;
	
	// Double the regen speed if playing Co-Op!
	CurrentRegenRate = bIsCoop ? (BaseRegenRate * 2.0f) : BaseRegenRate;

	// Freeze his brain and gun
	bDisableGameplay = true; 
	
	// Force him onto his knees
	if (DownedLoopMontage && CharacterMesh && CharacterMesh->GetAnimInstance())
	{
		CharacterMesh->GetAnimInstance()->Montage_Play(DownedLoopMontage);
	}
}

void AGASPBoss_Commander1::StopDownedRegen()
{
	bIsRegenerating = false;
	
	// Wake him back up!
	bDisableGameplay = false;
	
	// Blend out of the downed animation over 0.25 seconds so he stands up
	if (DownedLoopMontage && CharacterMesh && CharacterMesh->GetAnimInstance())
	{
		CharacterMesh->GetAnimInstance()->Montage_Stop(0.25f, DownedLoopMontage);
	}
}