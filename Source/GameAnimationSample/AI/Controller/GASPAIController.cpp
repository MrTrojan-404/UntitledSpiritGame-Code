#include "GASPAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameAnimationSample/GameAnimationSample.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "GameAnimationSample/AI/Character/GASPEnemyBase.h"
#include "GameAnimationSample/AI/GASPAITypes.h"
#include "GameAnimationSample/AI/Character/GASPAICharacterBase.h"
#include "GameAnimationSample/AI/Character/GASPMeleeEnemy.h"
#include "GameAnimationSample/AI/Character/GASPSniperEnemy.h"
#include "GameAnimationSample/AI/Character/Companion/GASPCompanionCharacter.h"
#include "GameAnimationSample/AI/Waypoint/GASPWaypointPath.h"
#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "GameAnimationSample/Component/DownedComponent.h"
#include "GameAnimationSample/Component/StealthComponent.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "GameAnimationSample/Subsystem/StealthSubsystem.h"

// Blackboard key definitions
const FName AGASPAIController::BB_TargetActor = TEXT("TargetActor");
const FName AGASPAIController::BB_LastKnownLocation = TEXT("LastKnownLocation");
const FName AGASPAIController::BB_bCanSeeTarget = TEXT("bCanSeeTarget");
const FName AGASPAIController::BB_bHeardSound = TEXT("bHeardSound");
const FName AGASPAIController::BB_SoundLocation = TEXT("SoundLocation");
const FName AGASPAIController::BB_WaypointIndex = TEXT("WaypointIndex");
const FName AGASPAIController::BB_bPingPongForward = TEXT("bPingPongForward");
const FName AGASPAIController::BB_AIState = TEXT("AIState");
const FName AGASPAIController::BB_bIsInGracePeriod = TEXT("bIsInGracePeriod");
const FName AGASPAIController::BB_EnemyType = TEXT("EnemyType");
const FName AGASPAIController::BB_PreferredRange = TEXT("PreferredRange");
const FName AGASPAIController::BB_CombatStyle = TEXT("CombatStyle");
const FName AGASPAIController::BB_WaypointWaitTime = TEXT("WaypointWaitTime");
const FName AGASPAIController::BB_bIsCaged = TEXT("bIsCaged");
const FName AGASPAIController::BB_bIsFrozen = TEXT("bIsFrozen");
const FName AGASPAIController::BB_bIsInStasis = TEXT("bIsInStasis");
const FName AGASPAIController::BB_bIsInRagdoll = TEXT("bIsInRagdoll");
const FName AGASPAIController::BB_bIsSuspended = TEXT("bIsSuspended");
const FName AGASPAIController::BB_bIsElectrocuted = TEXT("bIsElectrocuted");
const FName AGASPAIController::BB_DistanceToTarget = TEXT("DistanceToTarget");

// --- COMPANION KEYS ---
const FName AGASPAIController::BB_CompanionOrder = TEXT("CompanionOrder");
const FName AGASPAIController::BB_OrderLocation = TEXT("OrderLocation");
const FName AGASPAIController::BB_LeaderActor = TEXT("LeaderActor");
const FName AGASPAIController::BB_bWeaponsFree = TEXT("bWeaponsFree");
const FName AGASPAIController::BB_AllyToRevive = TEXT("AllyToRevive");
const FName AGASPAIController::BB_MarkedAttackTarget = TEXT("MarkedAttackTarget"); 

AGASPAIController::AGASPAIController()
{
	// Create perception component normally
	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(
		TEXT("AIPerception"));

	// Create sense configs using NewObject instead of 
	// CreateDefaultSubobject
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(
		TEXT("SightConfig"));
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(
		TEXT("HearingConfig"));
	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(
		TEXT("DamageConfig"));

	// Configure defaults here so they show in details panel
	SightConfig->SightRadius = 4000.f;
	SightConfig->LoseSightRadius = SightConfig->SightRadius + 500.f; 
	SightConfig->PeripheralVisionAngleDegrees = 60.f;
	SightConfig->SetMaxAge(0);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->NearClippingRadius = 50.f;

	// Disable the AI's "Sixth Sense" close-range cheating
	SightConfig->AutoSuccessRangeFromLastSeenLocation = -1.f;

	HearingConfig->HearingRange = 5500.f; // 50m
	HearingConfig->SetMaxAge(3.f);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	
	DamageConfig->SetMaxAge(5.f);

	// Add senses to perception component in constructor
	AIPerception->ConfigureSense(*SightConfig);
	AIPerception->ConfigureSense(*HearingConfig);
	AIPerception->ConfigureSense(*DamageConfig);
	AIPerception->SetDominantSense(
		SightConfig->GetSenseImplementation());

	if (GetWorld() && UAIPerceptionSystem::GetCurrent(GetWorld()))
	{
		UAIPerceptionSystem::GetCurrent(GetWorld())->UpdateListener(*AIPerception);
	}
	AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AGASPAIController::OnTargetPerceptionUpdated);
}

void AGASPAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateDetectionMeter(DeltaTime);
}

void AGASPAIController::BeginPlay()
{
	Super::BeginPlay();

	if (UStealthSubsystem* Sub = GetWorld()->GetSubsystem<UStealthSubsystem>())
	{
		Sub->OnGlobalAlert.AddDynamic(this, &AGASPAIController::OnGlobalAlert);
		Sub->OnGlobalAlertCleared.AddDynamic(this, &AGASPAIController::OnGlobalAlertCleared); // ADD
	}
}

void AGASPAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	// 0. Cache the body for the UI!
	CachedUIBody = InPawn;

	// 1. Broad cast to Base Character
	OwnedBaseChar = Cast<AGASPBaseCharacter>(InPawn);
	if (!OwnedBaseChar) return;

	InitializePerception(OwnedBaseChar);

	// 2. Intelligent Brain Selection (Player bodies use the Companion tree!)
	if (Cast<AGASPCompanionCharacter>(InPawn) || Cast<AGASPPlayerCharacter>(InPawn))
	{
		if (OwnedBaseChar->DefaultBehaviorTree) RunBehaviorTree(OwnedBaseChar->DefaultBehaviorTree);
		else if (CompanionBehaviorTree) RunBehaviorTree(CompanionBehaviorTree);
	}
	else
	{
		if (OwnedBaseChar->DefaultBehaviorTree) RunBehaviorTree(OwnedBaseChar->DefaultBehaviorTree);
		else if (EnemyBehaviorTree) RunBehaviorTree(EnemyBehaviorTree);
	}
	UE_LOG(LogAI, Warning, TEXT("[OnPossess] BlackboardComponent after run: %p"), 
	GetBlackboardComponent());

	// Push the freeze time to the Blackboard
	if (UBlackboardComponent* BBComp = GetBlackboardComponent())
	{
		BBComp->SetValueAsFloat(FName("SurpriseDuration"), SurpriseFreezeTime);
	}

	SetupBlackboard(OwnedBaseChar);

	GetWorldTimerManager().SetTimer(TargetSweepTimer, this, &AGASPAIController::PerformTargetSweep, 0.2f, true);
}

void AGASPAIController::OnUnPossess()
{
	// We MUST push a 0 instead of instantly deleting, so the Client UI gets the message to remove the arrow.
	// Call this BEFORE Super::OnUnPossess() so GetPawn() is still valid for the Stealth Subsystem!
	ClearStealthUI();

	Super::OnUnPossess();

	GetWorldTimerManager().ClearTimer(GracePeriodTimer);
	GetWorldTimerManager().ClearTimer(TargetSweepTimer);
    
	// Delete the radio timer if the brain is detached
	GetWorldTimerManager().ClearTimer(GlobalAlertDelayTimer);

	OwnedBaseChar = nullptr;
}

void AGASPAIController::PerformTargetSweep()
{
	// If the AI is dead, stop thinking
	if (GetAIState() == EAIState::Dead || !AIPerception || !OwnedBaseChar) return;

	if (!GetBlackboardComponent() || !GetPerceptionComponent() || !GetPawn())
	{
		return; // Silently skip this tick if the AI is dead or loading
	}
	
	// --- Stop scanning for new targets while in stasis ---
	if (IsInStasis()) return;
	
	// AUTO-CLEAR "MOVE TO" ORDERS WHEN WE ARRIVE
	if (Blackboard && Blackboard->GetValueAsInt(BB_CompanionOrder) == static_cast<int32>(ECompanionOrder::MoveToLocation))
	{
		FVector OrderDest = Blackboard->GetValueAsVector(BB_OrderLocation);
		if (FVector::Dist(OwnedBaseChar->GetActorLocation(), OrderDest) < 150.f)
		{
			IssueOrder(ECompanionOrder::StandBy, FVector::ZeroVector, nullptr);
		}
	}

	TArray<AActor*> PerceivedActors;
	AIPerception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);

	AActor* BestTarget = nullptr;
	float MinDistSq = MAX_flt;

	bool bAmICompanion = Cast<AGASPCompanionCharacter>(OwnedBaseChar) != nullptr || Cast<AGASPPlayerCharacter>(OwnedBaseChar) != nullptr;

	float MaxSightDistance = SightConfig ? SightConfig->SightRadius : 3000.f;
	float MaxSightSq = MaxSightDistance * MaxSightDistance;

	for (AActor* Actor : PerceivedActors)
	{
		if (!Actor || Actor->IsActorBeingDestroyed()) continue;

		if (UDownedComponent* DownedComp = Actor->FindComponentByClass<UDownedComponent>())
		{
			if (DownedComp->IsDowned()) continue;
		}

		// Faction Check
		bool bIsEnemy = Cast<AGASPEnemyBase>(Actor) != nullptr;
		bool bIsCompanion = Cast<AGASPCompanionCharacter>(Actor) != nullptr;
		bool bIsPlayer = Cast<AGASPPlayerCharacter>(Actor) != nullptr;

		// ==========================================
		// NEW: VISUAL STEALTH DISTURBANCES
		// If I am a Guard, and I see a fellow Guard in trouble...
		// ==========================================
		if (!bAmICompanion && bIsEnemy)
		{
			if (AGASPAICharacterBase* SeenAlly = Cast<AGASPAICharacterBase>(Actor))
			{
				bool bIsDead = !SeenAlly->IsAlive();
				bool bIsCCd = SeenAlly->bDisableGameplay; // True during Electrocution and Stealth Takedowns
				bool bInTrap = false;

				if (AGASPAIController* AllyAI = Cast<AGASPAIController>(SeenAlly->GetController()))
				{
					bInTrap = AllyAI->IsInStasis(); // Covers Frost Lock, Hydro Cage, and Stasis Bubble
					if (UBlackboardComponent* BB = AllyAI->GetBlackboardComponent())
					{
						// Covers Heavies in Suspension Matrix and Lights in Ragdoll (Gravity Grenades)
						bInTrap = bInTrap || BB->GetValueAsBool(AGASPAIController::BB_bIsSuspended) 
										  || BB->GetValueAsBool(AGASPAIController::BB_bIsInRagdoll);
					}
				}

				// If I see a dead, trapped, or electrocuted buddy!
				if (bIsDead || bIsCCd || bInTrap)
				{
					// If I'm not already in combat, freak out and investigate!
					if (AlertPhase < EAlertPhase::Caution && GetAIState() != EAIState::Engage)
					{
						SetAlertPhase(EAlertPhase::Caution);
						DetectionMeter = FMath::Max(DetectionMeter, CautionThreshold);
						SetLastKnownLocation(SeenAlly->GetActorLocation());
						SetAIState(EAIState::Alert);
						StartAlertTimer();

						if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
						{
							BTComp->RestartTree();
						}
					}
				}
			}
			continue; // Guards don't target other Guards to shoot, so skip to the next actor!
		}

		// ==========================================
		// NORMAL TARGETING LOGIC
		// ==========================================
		// Now we can safely skip dead bodies for combat targeting so we don't shoot corpses
		if (AGASPAICharacterBase* BaseAI = Cast<AGASPAICharacterBase>(Actor))
		{
			if (!BaseAI->IsAlive()) continue;
		}

		if (bAmICompanion && !bIsEnemy) continue;
		if (!bAmICompanion && (!bIsPlayer && !bIsCompanion)) continue;

		float DistSq = FVector::DistSquared(OwnedBaseChar->GetActorLocation(), Actor->GetActorLocation());

		if (DistSq > MaxSightSq) continue;

		if (DistSq < MinDistSq)
		{
			MinDistSq = DistSq;
			BestTarget = Actor;
		}
	}

	AActor* CurrentTarget = Cast<AActor>(Blackboard->GetValueAsObject(BB_TargetActor));

	// CHECK IF OUR TARGET WAS DEFEATED SO WE DON'T RUSH THE BODY
	if (CurrentTarget)
	{
		bool bTargetDefeated = false;

		if (UDownedComponent* Downed = CurrentTarget->FindComponentByClass<UDownedComponent>())
		{
			if (Downed->IsDowned()) bTargetDefeated = true;
		}

		if (AGASPAICharacterBase* AIChar = Cast<AGASPAICharacterBase>(CurrentTarget))
		{
			if (!AIChar->IsAlive()) bTargetDefeated = true;
		}
		else if (AGASPPlayerCharacter* PlayerChar = Cast<AGASPPlayerCharacter>(CurrentTarget))
		{
			if (PlayerChar->IsElimmed()) bTargetDefeated = true;
		}

		if (bTargetDefeated)
		{
			ClearTarget();
			CurrentTarget = nullptr;
			SetAIState(EAIState::Alert);
			StartAlertTimer();
		}
	}

	if (BestTarget)
	{
		if (bAmICompanion)
		{
			if (CurrentTarget != BestTarget)
			{
				SetTargetActor(BestTarget);
				SetAIState(EAIState::Engage);
			}
		}
		else
		{
			if (AlertPhase != EAlertPhase::Alert)
			{
				SuspectedActor = BestTarget;
			}
			else if (CurrentTarget != BestTarget)
			{
				SetTargetActor(BestTarget);
				SetAIState(EAIState::Engage);

				if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
				{
					BTComp->RestartTree();
				}
			}
		}
	}

	// --- PUSH STATE TO REPLICATED UI STRUCT ---
	if (HasAuthority())
	{
		if (AGASPCompanionCharacter* CompanionPawn = Cast<AGASPCompanionCharacter>(OwnedBaseChar))
		{
			CompanionPawn->UIState.CurrentAIState = GetAIState();
			CompanionPawn->UIState.CurrentOrder = static_cast<ECompanionOrder>(Blackboard->GetValueAsInt(BB_CompanionOrder));
			CompanionPawn->UIState.bWeaponsFree = Blackboard->GetValueAsBool(BB_bWeaponsFree);

			AActor* Leader = Cast<AActor>(Blackboard->GetValueAsObject(BB_LeaderActor));
			if (AGASPBaseCharacter* LeaderChar = Cast<AGASPBaseCharacter>(Leader))
			{
				FString Name = LeaderChar->GetDisplayName();
				if (Name.Contains(TEXT("BP_GASP"))) Name = TEXT("Player");
				CompanionPawn->UIState.LeaderName = Name;
			}
			else
			{
				CompanionPawn->UIState.LeaderName = TEXT("");
			}
		}
	}
}

void AGASPAIController::IssueOrder(ECompanionOrder NewOrder, FVector Location, AActor* TargetActor)
{
	if (!Blackboard) return;

	// 1. Properly route the TargetActor so LeaderActor doesn't get hijacked by Medics!
	if (TargetActor)
	{
		if (NewOrder == ECompanionOrder::ReviveAlly)
		{
			// Ignore duplicate orders so we don't restart the revive timer
			AActor* CurrentReviveTarget = Cast<AActor>(Blackboard->GetValueAsObject(BB_AllyToRevive));

			if (CurrentReviveTarget == TargetActor)
			{
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT("AI: I'm already on it!"));
				return; // Stop here so the Behavior Tree doesn't restart!
			}

			Blackboard->SetValueAsObject(BB_AllyToRevive, TargetActor);
		}
		else if (NewOrder == ECompanionOrder::AttackTarget)          // NEW branch
		{
			Blackboard->SetValueAsObject(BB_MarkedAttackTarget, TargetActor);
			Blackboard->SetValueAsBool(BB_bWeaponsFree, true); // marking implies weapons free for this shot
		}
		else
		{
			Blackboard->SetValueAsObject(BB_LeaderActor, TargetActor);
		}
	}
	// 2. Protect the Movement State
	if (NewOrder == ECompanionOrder::Follow ||
		NewOrder == ECompanionOrder::StandBy ||
		NewOrder == ECompanionOrder::MoveToLocation ||
		NewOrder == ECompanionOrder::ReviveAlly)
	{
		Blackboard->SetValueAsInt(BB_CompanionOrder, static_cast<int32>(NewOrder));

		// Instantly kill 'Wait' tasks
		if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
		{
			BTComp->RestartTree();
		}
	}

	// 3. Route the specific debug messages / variables
	switch (NewOrder)
	{
	case ECompanionOrder::Follow:
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("Order: FOLLOW ME"));
		break;

	case ECompanionOrder::StandBy:
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("Order: STAND BY"));
		break;

	case ECompanionOrder::MoveToLocation:
		Blackboard->SetValueAsVector(BB_OrderLocation, Location);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("Order: MOVE TO POSITION"));
		break;

	case ECompanionOrder::WeaponsFree:
		Blackboard->SetValueAsBool(BB_bWeaponsFree, true);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Order: WEAPONS FREE"));
		break;

	case ECompanionOrder::HoldFire:
		Blackboard->SetValueAsBool(BB_bWeaponsFree, false);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Blue, TEXT("Order: HOLD FIRE"));
		break;

	case ECompanionOrder::ReviveAlly:
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Order: MEDIC!!"));
		break;
	case ECompanionOrder::AttackTarget:
		// TargetActor already routed into BB_LeaderActor above via the TargetActor block —
		// but AttackTarget shouldn't hijack BB_LeaderActor (that's for follow-leader).
		// So route it to a dedicated key instead. See note below.
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Magenta, TEXT("Order: ATTACK MARKED TARGET"));
		break;
	}
}

#pragma region Perception

void AGASPAIController::InitializePerception(AGASPBaseCharacter* BaseChar)
{
	if (!BaseChar || !SightConfig || !HearingConfig) return;

	// If it's a dedicated AI, use their tuned stats
	if (AGASPAICharacterBase* AIChar = Cast<AGASPAICharacterBase>(BaseChar))
	{
		SightConfig->SightRadius = AIChar->SightRadius;
		SightConfig->LoseSightRadius = AIChar->SightRadius + 100.f;
		SightConfig->PeripheralVisionAngleDegrees = AIChar->PeripheralVisionAngle;
		HearingConfig->HearingRange = AIChar->HearingRange;
	}
	else // If it's an abandoned Player body, use these solid default companion stats!
	{
		SightConfig->SightRadius = 3000.f;
		SightConfig->LoseSightRadius = 3200.f;
		SightConfig->PeripheralVisionAngleDegrees = 90.f;
		HearingConfig->HearingRange = 1500.f;
	}

	// Companions are smarter and get a short auto-success window.
	// Enemies must have real LOS — no cheating through walls.
	if (Cast<AGASPCompanionCharacter>(BaseChar) || Cast<AGASPPlayerCharacter>(BaseChar))
		SightConfig->AutoSuccessRangeFromLastSeenLocation = 300.f;
	else
		SightConfig->AutoSuccessRangeFromLastSeenLocation = -1.f;

	AIPerception->ConfigureSense(*SightConfig);
	AIPerception->ConfigureSense(*HearingConfig);
	AIPerception->SetDominantSense(SightConfig->GetSenseImplementation());
	SightConfig->SetMaxAge(0.f);

	AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AGASPAIController::OnTargetPerceptionUpdated);
}

void AGASPAIController::SetupBlackboard(AGASPBaseCharacter* BaseChar)
{
	if (!Blackboard) return;

	SetAIState(EAIState::Patrol);
	SetCanSeeTarget(false);
	SetHeardSound(false, FVector::ZeroVector);
	SetWaypointIndex(0);
	SetPingPongForward(true);
	SetIsInGracePeriod(false);

	bool bIsCompanionOrPlayerBody = Cast<AGASPCompanionCharacter>(BaseChar) || Cast<AGASPPlayerCharacter>(BaseChar);
	if (bIsCompanionOrPlayerBody)
	{
		// Companions/player-vacated bodies start silent — the stealth system
		// (global alert) decides when they're allowed to fire.
		UStealthSubsystem* StealthSub = GetWorld()->GetSubsystem<UStealthSubsystem>();
		bool bStartArmed = StealthSub && StealthSub->IsGlobalAlertActive();
		Blackboard->SetValueAsBool(BB_bWeaponsFree, bStartArmed);
	}
	else
	{
		// Hostile enemies are always weapons free.
		Blackboard->SetValueAsBool(BB_bWeaponsFree, true);
	}
	
	// If it's a dedicated AI, pull their specific stats
	if (AGASPAICharacterBase* AIChar = Cast<AGASPAICharacterBase>(BaseChar))
	{
		Blackboard->SetValueAsInt(BB_EnemyType, AIChar->GetEnemyType());
		Blackboard->SetValueAsInt(BB_CombatStyle, AIChar->CombatStyle);

		if (AGASPMeleeEnemy* Melee = Cast<AGASPMeleeEnemy>(AIChar)) SetPreferredRange(Melee->MeleeRange);
		else if (AGASPRangedEnemy* Ranged = Cast<AGASPRangedEnemy>(AIChar)) SetPreferredRange(Ranged->EngagementRange);
		else if (AGASPSniperEnemy* Sniper = Cast<AGASPSniperEnemy>(AIChar))
		{
			SetPreferredRange(Sniper->PreferredCombatDistance);
			Blackboard->SetValueAsFloat(FName("AimDuration"), Sniper->AimDuration);
		}
		else if (AGASPCompanionCharacter* Companion = Cast<AGASPCompanionCharacter>(AIChar))
		{
			SetPreferredRange(Companion->EngagementRange);
		}
	}
	else if (Cast<AGASPPlayerCharacter>(BaseChar))
	{
		// If it's an abandoned Player body, set default companion combat variables!
		SetPreferredRange(1500.f);
	}
}

void AGASPAIController::OnTargetPerceptionUpdated(
	AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor) return;

	if (!GetBlackboardComponent() || !GetPawn())
	{
		return;
	}
	
	FString SenseType = TEXT("Unknown");
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
		SenseType = TEXT("Sight");
	else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
		SenseType = TEXT("Hearing");
	else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Damage>())
		SenseType = TEXT("Damage");

	UE_LOG(LogAI, Warning,
	       TEXT("[Perception] Sense: %s | Actor: %s | Sensed: %s"),
	       *SenseType,
	       *Actor->GetName(),
	       Stimulus.WasSuccessfullySensed() ? TEXT("YES") : TEXT("NO"));

	if (Stimulus.Type ==
		UAISense::GetSenseID<UAISense_Sight>())
		HandleSightStimulus(Actor, Stimulus);
	else if (Stimulus.Type ==
		UAISense::GetSenseID<UAISense_Hearing>())
		HandleHearingStimulus(Actor, Stimulus);
	else if (Stimulus.Type ==
		UAISense::GetSenseID<UAISense_Damage>())
		HandleDamageStimulus(Actor, Stimulus);
}

void AGASPAIController::HandleSightStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
	if (!OwnedBaseChar || !Actor) return;

	if (!GetBlackboardComponent() || !GetPawn())
	{
		return; // Ignore sight if the brain isn't fully loaded!
	}
	// -
	
	// --- Ignore visual changes while in stasis ---
	if (IsInStasis()) return;
	
	// ONLY enemies reset. Companions don't care.
	if (AGASPEnemyBase* Enemy = Cast<AGASPEnemyBase>(OwnedBaseChar))
	{
		if (Enemy->bIsResetting) return;
	}

	// Faction Targeting Rules
	bool bAmICompanion = Cast<AGASPCompanionCharacter>(OwnedBaseChar) != nullptr || Cast<
		AGASPPlayerCharacter>(OwnedBaseChar) != nullptr;

	bool bSawEnemy = Cast<AGASPEnemyBase>(Actor) != nullptr;
	bool bSawPlayer = Cast<AGASPPlayerCharacter>(Actor) != nullptr;
	bool bSawCompanion = Cast<AGASPCompanionCharacter>(Actor) != nullptr;

	if (bAmICompanion)
	{
		if (!bSawEnemy) return; // Companions ignore the player and only target enemies
	}
	else
	{
		if (!bSawPlayer && !bSawCompanion) return;
		// Enemies target both players and companions
	}

	// If the person we just saw is downed, completely ignore them.
	if (UDownedComponent* DownedComp = Actor->FindComponentByClass<UDownedComponent>())
	{
		if (DownedComp->IsDowned()) return;
	}

	if (Stimulus.WasSuccessfullySensed())
    {
		bIsVisuallySensingTarget = true;
        SetLastKnownLocation(Actor->GetActorLocation());
        SetCanSeeTarget(true);
        GetWorldTimerManager().ClearTimer(GracePeriodTimer);
        GetWorldTimerManager().ClearTimer(AlertTimer);
        SetIsInGracePeriod(false);

        AActor* CurrentTarget = Cast<AActor>(Blackboard->GetValueAsObject(BB_TargetActor));

        if (bAmICompanion)
        {
            // Companions engage instantly
            if (!CurrentTarget || CurrentTarget != Actor)
            {
                // Switch targets if the new one is significantly closer
                if (CurrentTarget)
 
               {
                    float DistToCurrent = FVector::Dist(OwnedBaseChar->GetActorLocation(), CurrentTarget->GetActorLocation());
                    float DistToNew = FVector::Dist(OwnedBaseChar->GetActorLocation(), Actor->GetActorLocation());
                    if (DistToNew < (DistToCurrent - 200.f)) SetTargetActor(Actor);
                }
                else
                {
                    SetTargetActor(Actor);
                }
                
                SetAIState(EAIState::Engage);

                if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
                {
                    BTComp->RestartTree();
                }
            }
        }
        else
        {
            // === ENEMIES ===
            // If we are ALREADY fully alerted (or engaged from taking damage), lock on so we don't stare!
            if (AlertPhase == EAlertPhase::Alert || GetAIState() == EAIState::Engage)
            {
                if (!CurrentTarget || CurrentTarget != Actor)
                {
                    SetTargetActor(Actor);
                    SetAIState(EAIState::Engage);
                    if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
                    {
                        BTComp->RestartTree();
                    }
                }
            }
            else
            {
                // We are NOT fully alerted yet. Feed the stealth detection meter!
                // Do NOT set TargetActor or Engage here. UpdateDetectionMeter() will handle it.
                SuspectedActor = Actor;
                InvestigateLocation = Actor->GetActorLocation();
            }
        }
    }
	else // lost sight
	{
		bIsVisuallySensingTarget = false;
		LogAIStatus(TEXT("SightLost"));
		SetLastKnownLocation(Actor->GetActorLocation());
		SetCanSeeTarget(false);
		bHasLOSToSuspected = false;

		if (GetAIState() == EAIState::Engage)
		{
			bool bTargetDead = false;
			if (AGASPAICharacterBase* SensedAI = Cast<AGASPAICharacterBase>(Actor))
				bTargetDead = !SensedAI->IsAlive();

			if (bAmICompanion && bTargetDead)
			{
				ClearTarget();
				SetAIState(EAIState::Patrol);
			}
			else
			{
				StartGracePeriod();
			}
		}
		// For enemies not yet engaged: meter decays naturally because bHasLOSToSuspected is false.
	}
}

void AGASPAIController::HandleHearingStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
	if (!OwnedBaseChar || !Actor) return;

	// --- Ignore sounds while in stasis ---
	if (IsInStasis()) return;
	
	// KILL THE PERCEPTION ECHO
	// We MUST ignore the "forget" signal, or the AI counts every noise twice!
	if (!Stimulus.WasSuccessfullySensed()) return;

	// === GRENADE PANIC LOGIC ===
	if (Stimulus.Tag == FName("GrenadeBounce"))
	{
		// Check if a wall is already protecting us
		FHitResult Hit;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(GetPawn()); // Ignore our own body
		QueryParams.AddIgnoredActor(Actor); // Ignore the grenade

		// Trace a line from the grenade to the AI's chest
		bool bHitWall = GetWorld()->LineTraceSingleByChannel(
			Hit,
			Stimulus.StimulusLocation,
			GetPawn()->GetActorLocation(),
			ECC_Visibility,
			QueryParams
		);

		// If the line hit a wall, we are completely safe from the blast. Ignore the panic!
		if (bHitWall)
		{
			UE_LOG(LogTemp, Warning, TEXT("AI: Grenade blocked by wall, holding position."));
			return;
		}

		if (Blackboard)
		{
			// 1. LOAD THE PAYLOAD FIRST: Silently save the grenade's location into memory.
			Blackboard->SetValueAsVector(FName("DangerLocation"), Stimulus.StimulusLocation);

			// 2. PULL THE TRIGGER: Now that the data is safe, wake up the Behavior Tree!
			Blackboard->SetValueAsBool(FName("bInDanger"), true);
		}

		// THE INTERRUPT
		// Forcefully restart the tree! This snaps them out of shooting tasks instantly.
		if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
		{
			BTComp->RestartTree();
		}

		if (GetAIState() == EAIState::Patrol)
		{
			SetAIState(EAIState::Alert);
		}

		return;
	}
	// === DISTRACTION LOGIC (Coin Toss / Rock) ===
	if (Stimulus.Tag == FName("Distraction"))
	{
		LogAIStatus(TEXT("Heard Distraction"));

		// Ignore coins if we are already in a gunfight or dead!
		if (GetAIState() == EAIState::Engage || GetAIState() == EAIState::Dead) return;

		// THE SPAM FILTER
		CurrentDistractionCount++;
		if (CurrentDistractionCount > MaxDistractionsAllowed)
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("AI: STOP THROWING THINGS! WE ARE UNDER ATTACK!"));

			// The AI figured it out! Pinpoint the person who threw the coin (Actor).
			SuspectedActor = Actor; 
			InvestigateLocation = Actor->GetActorLocation();

			// Instantly force full detection! The next tick will trigger the gunfight.
			DetectionMeter = 1.f;
			return;
		}

		// 1. Force the meter up so the UI turns yellow (!)
		DetectionMeter = FMath::Max(DetectionMeter, SuspiciousThreshold);
		SetAlertPhase(EAlertPhase::Suspicious);

		// 2. Tell the brain exactly where the NEW coin landed
		SetLastKnownLocation(Stimulus.StimulusLocation);

		// 3. Put them into investigation mode
		SetAIState(EAIState::Alert);

		// 4.THE PIVOT
		// ALWAYS restart the Alert Timer and the Behavior Tree so they instantly 
		// abort whatever they were doing and snap their attention to the new noise!
		StartAlertTimer();
		if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
		{
			BTComp->RestartTree();
		}
		
		return; 
	}
	
	bool bAmICompanion = Cast<AGASPCompanionCharacter>(OwnedBaseChar) != nullptr || Cast<
		AGASPPlayerCharacter>(OwnedBaseChar) != nullptr;

	bool bHeardPlayer = Cast<AGASPPlayerCharacter>(Actor) != nullptr;
	bool bHeardCompanion = Cast<AGASPCompanionCharacter>(Actor) != nullptr;
	bool bHeardEnemy = Cast<AGASPEnemyBase>(Actor) != nullptr;

	if (bAmICompanion && (bHeardPlayer || bHeardCompanion))
	{
		return;
	}

	// Enemies ignore other enemy noises (gunfire, footsteps, etc.)
	if (!bAmICompanion && bHeardEnemy) return;

	// ONLY enemies reset. Companions don't care.
	if (AGASPEnemyBase* Enemy = Cast<AGASPEnemyBase>(OwnedBaseChar))
	{
		if (Enemy->bIsResetting) return;
		float DistFromPatrol = FVector::Dist(Enemy->GetActorLocation(), Enemy->PatrolOrigin);

		if (DistFromPatrol > Enemy->MaxChaseDistance) return;

		float SoundDistFromPatrol = FVector::Dist(Stimulus.StimulusLocation, Enemy->PatrolOrigin);
		if (SoundDistFromPatrol > Enemy->MaxChaseDistance) return;
	}

	LogAIStatus(TEXT("HearingTriggered"));

	if (GetAIState() == EAIState::Engage || GetAIState() == EAIState::Dead) return;

	if (AlertPhase != EAlertPhase::Alert)
	{
		SuspectedActor = Actor;
		InvestigateLocation = Stimulus.StimulusLocation;

		// Gunshots jump to Caution immediately; footsteps just to Suspicious
		float NoiseBoost = (Stimulus.Tag == FName("Gunshot")) ? 0.70f : 0.25f;

		DetectionMeter = FMath::Min(0.95f, DetectionMeter + NoiseBoost);
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		SetHeardSound(true, Stimulus.StimulusLocation);
		SetLastKnownLocation(Stimulus.StimulusLocation);

		// Transition from Patrol to Alert so the Behavior Tree reacts!
		if (GetAIState() != EAIState::Alert)
		{
			SetAIState(EAIState::Alert);
		}
		StartAlertTimer();
	}
}

void AGASPAIController::HandleDamageStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
	if (!OwnedBaseChar || !Actor) return;

	// --- Ignore bullet damage perception while in stasis ---
	if (IsInStasis()) return;
	
	// If we are retreating, ignore EVERYTHING.
	if (AGASPEnemyBase* Enemy = Cast<AGASPEnemyBase>(OwnedBaseChar))
	{
		if (Enemy->bIsResetting) return;
	}

	//Do not process damage if the bullet killed us
	if (AGASPAICharacterBase* AIChar = Cast<AGASPAICharacterBase>(OwnedBaseChar))
	{
		if (!AIChar->IsAlive()) return;
	}
	
	// Prevent Friendly Fire
	bool bAmICompanion = Cast<AGASPCompanionCharacter>(OwnedBaseChar) != nullptr || Cast<
		AGASPPlayerCharacter>(OwnedBaseChar) != nullptr;
	bool bShotByPlayer = Cast<AGASPPlayerCharacter>(Actor) != nullptr;
	bool bShotByCompanion = Cast<AGASPCompanionCharacter>(Actor) != nullptr;

	if (bAmICompanion && (bShotByPlayer || bShotByCompanion)) return; // Companions ignore friendly fire
	if (!bAmICompanion && !bShotByPlayer && !bShotByCompanion) return; // Enemies ignore enemy crossfire

	// Ignore damage sources from downed actors
	if (UDownedComponent* DownedComp = Actor->FindComponentByClass<UDownedComponent>())
	{
		if (DownedComp->IsDowned()) return;
	}

	LogAIStatus(TEXT("DamageReceived"));

	if (GetAIState() == EAIState::Dead) return;

	if (Stimulus.WasSuccessfullySensed() && Actor)
	{
		// Dynamically Boost Sight to see the attacker
		if (UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent())
		{
			FAISenseID SightID = UAISense::GetSenseID<UAISense_Sight>();
			if (UAISenseConfig_Sight* CurrentSightConfig = Cast<UAISenseConfig_Sight>(
				PerceptionComp->GetSenseConfig(SightID)))
			{
				float DistanceToAttacker = FVector::Dist(GetPawn()->GetActorLocation(), Actor->GetActorLocation());
				if (CurrentSightConfig->SightRadius < DistanceToAttacker)
				{
					CurrentSightConfig->SightRadius = DistanceToAttacker + 500.f;
					CurrentSightConfig->LoseSightRadius = CurrentSightConfig->SightRadius + 500.f;
					PerceptionComp->ConfigureSense(*CurrentSightConfig);
					UAIPerceptionSystem::GetCurrent(GetWorld())->UpdateListener(*PerceptionComp);
				}
			}
		}

		// Self-Defense Override for companions
		if (Blackboard && !Blackboard->GetValueAsBool(BB_bWeaponsFree))
		{
			if (GEngine)
				GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange,
				                                 TEXT("AI: Taking fire! Self-defense override! Weapons Free!"));
			Blackboard->SetValueAsBool(BB_bWeaponsFree, true);
		}

		// Feed the detection meter — a large boost but still requires LOS to fully engage.
		// The enemy knows they were shot FROM somewhere; they still need to LOOK at the player.
		SuspectedActor = Actor;
		InvestigateLocation = Actor->GetActorLocation();
		DetectionMeter = FMath::Min(0.95f, DetectionMeter + 0.75f);
		SetLastKnownLocation(Actor->GetActorLocation());
		SetFocus(Actor);

		// Bump the internal phase so the meter threshold transitions are consistent
		if (DetectionMeter >= CautionThreshold && AlertPhase < EAlertPhase::Caution)
			SetAlertPhase(EAlertPhase::Caution);

		// Switch to investigation mode — NOT full Engage yet
		if (GetAIState() != EAIState::Engage)
		{
			SetAIState(EAIState::Alert);
			StartAlertTimer();
			StopMovement(); // brief surprise freeze

			if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
				BTComp->RestartTree();
		}

		// Update the stealth HUD with the new meter value
		if (!bIsStealthUIHidden)
		{
			if (UStealthSubsystem* Sub = GetWorld()->GetSubsystem<UStealthSubsystem>())
				Sub->RegisterDetection(this, DetectionMeter, static_cast<uint8>(AlertPhase));
		}

		// Global alert/backup radio logic (unchanged timing intent)
		if (AlertPhase == EAlertPhase::Alert)
		{
			if (!bHasCalledForBackup && GetWorldTimerManager().IsTimerActive(GlobalAlertDelayTimer))
			{
				GetWorldTimerManager().ClearTimer(GlobalAlertDelayTimer);
				BroadcastGlobalAlert();
			}
		}
		else if (!bHasCalledForBackup && !GetWorldTimerManager().IsTimerActive(GlobalAlertDelayTimer))
		{
			GetWorldTimerManager().SetTimer(GlobalAlertDelayTimer, this,
			                                &AGASPAIController::BroadcastGlobalAlert, TimeToCallForBackup, false);
		}
	}
}

#pragma endregion Perception

#pragma region GracePeriod

void AGASPAIController::StartGracePeriod()
{
	LogAIStatus(TEXT("GraceStart"));

	UE_LOG(LogAI, Warning,
	       TEXT("[Grace] Starting grace period: %.1fs | AI: %s"),
	       GracePeriodDuration,
	       *GetName());

	SetIsInGracePeriod(true);
	GetWorldTimerManager().SetTimer(
		GracePeriodTimer,
		this,
		&AGASPAIController::OnGracePeriodExpired,
		GracePeriodDuration,
		false
	);
}

void AGASPAIController::OnGracePeriodExpired()
{
	LogAIStatus(TEXT("GraceExpired"));

	UE_LOG(LogAI, Warning,
	       TEXT("[Grace] Grace period expired | AI: %s"),
	       *GetName());

	SetIsInGracePeriod(false);
	ClearTarget();

	// Companions go back to the player, Enemies investigate
	if (Cast<AGASPCompanionCharacter>(OwnedBaseChar))
	{
		SetAIState(EAIState::Patrol);
	}
	else
	{
		SetAIState(EAIState::Alert);
		StartAlertTimer();
	}
}

#pragma endregion GracePeriod

#pragma region BlackboardHelpers

void AGASPAIController::SetTargetActor(AActor* Target)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsObject(BB_TargetActor, Target);

	SetFocus(Target);
}

void AGASPAIController::ClearTarget()
{
	if (!Blackboard) return;
	Blackboard->ClearValue(BB_TargetActor);
	SetCanSeeTarget(false);

	ClearFocus(EAIFocusPriority::Gameplay);
}

void AGASPAIController::SetLastKnownLocation(
	const FVector& Location)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsVector(BB_LastKnownLocation, Location);
}

void AGASPAIController::SetAIState(EAIState NewState)
{
	if (!Blackboard) return;
	EAIState OldState = GetAIState();

	if (OldState != NewState)
	{
		if (NewState == EAIState::Patrol)
		{
			GetWorldTimerManager().ClearTimer(GracePeriodTimer);
			GetWorldTimerManager().ClearTimer(AlertTimer);
			SetIsInGracePeriod(false);

			// ONLY enemies have waypoint paths!
			if (AGASPEnemyBase* Enemy = Cast<AGASPEnemyBase>(OwnedBaseChar))
			{
				if (Enemy->WaypointPath && OldState != EAIState::Patrol)
				{
					int32 ClosestIndex = 0;
					float MinDist = MAX_flt;

					for (int32 i = 0; i < Enemy->WaypointPath->Num(); ++i)
					{
						float Dist = FVector::Dist(Enemy->GetActorLocation(),
						                           Enemy->WaypointPath->GetLocationAtIndex(i));

						if (Dist < MinDist)
						{
							MinDist = Dist;
							ClosestIndex = i;
						}
					}
					SetWaypointIndex(ClosestIndex);
				}
			}
		}
		Blackboard->SetValueAsInt(BB_AIState, static_cast<int32>(NewState));
	}
}

void AGASPAIController::SetCanSeeTarget(bool bCanSee)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsBool(BB_bCanSeeTarget, bCanSee);
}

void AGASPAIController::SetHeardSound(
	bool bHeard, const FVector& Location)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsBool(BB_bHeardSound, bHeard);
	if (bHeard)
	{
		Blackboard->SetValueAsVector(BB_SoundLocation, Location);
	}
}

void AGASPAIController::SetWaypointIndex(int32 Index)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsInt(BB_WaypointIndex, Index);
}

void AGASPAIController::SetPingPongForward(bool bForward)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsBool(BB_bPingPongForward, bForward);
}

void AGASPAIController::SetIsInGracePeriod(bool bInGrace)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsBool(BB_bIsInGracePeriod, bInGrace);
}

void AGASPAIController::SetPreferredRange(float Range)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsFloat(BB_PreferredRange, Range);
}

EAIState AGASPAIController::GetAIState() const
{
	if (!Blackboard) return EAIState::Patrol;
	return static_cast<EAIState>(
		Blackboard->GetValueAsInt(BB_AIState));
}

void AGASPAIController::OnEnemyDied()
{
	LogAIStatus(TEXT("EnemyDied"));

	ClearStealthUI(); // Clear the UI BEFORE setting the AI state to Dead

	SetAIState(EAIState::Dead);
	GetWorldTimerManager().ClearTimer(GracePeriodTimer);
	GetWorldTimerManager().ClearTimer(GlobalAlertDelayTimer);

	if (UBehaviorTreeComponent* BTComp = FindComponentByClass<UBehaviorTreeComponent>())
	{
		BTComp->StopTree();
	}
	StopMovement();
}

#pragma endregion BlackboardHelpers

void AGASPAIController::SetEnemyType(int32 Type)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsInt(BB_EnemyType, Type);
}

void AGASPAIController::LogAIStatus(const FString& Context) const
{
	if (!OwnedBaseChar) return;

	// Get player
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* PlayerPawn = PC ?
		PC->GetPawn() : nullptr;

	FVector AILocation = OwnedBaseChar->GetActorLocation();
	FRotator AIRotation = OwnedBaseChar->GetActorRotation();

	FString PlayerInfo = TEXT("No Player");
	float DistanceToPlayer = -1.f;

	if (PlayerPawn)
	{
		FVector PlayerLocation = PlayerPawn->GetActorLocation();
		FRotator PlayerRotation = PlayerPawn->GetActorRotation();
		DistanceToPlayer = FVector::Dist(AILocation, PlayerLocation);

		PlayerInfo = FString::Printf(
			TEXT("Player Loc: X=%.1f Y=%.1f Z=%.1f | "
				"Player Rot: P=%.1f Y=%.1f | "
				"Distance: %.1fcm"),
			PlayerLocation.X, PlayerLocation.Y, PlayerLocation.Z,
			PlayerRotation.Pitch, PlayerRotation.Yaw,
			DistanceToPlayer);
	}

	FString AIInfo = FString::Printf(
		TEXT("AI Loc: X=%.1f Y=%.1f Z=%.1f | "
			"AI Rot: P=%.1f Y=%.1f | "
			"State: %d | "
			"CanSee: %s | "
			"InGrace: %s"),
		AILocation.X, AILocation.Y, AILocation.Z,
		AIRotation.Pitch, AIRotation.Yaw,
		static_cast<int32>(GetAIState()),
		Blackboard && Blackboard->GetValueAsBool(BB_bCanSeeTarget)
			? TEXT("YES")
			: TEXT("NO"),
		Blackboard && Blackboard->GetValueAsBool(BB_bIsInGracePeriod)
			? TEXT("YES")
			: TEXT("NO"));

	UE_LOG(LogAI, Warning,
	       TEXT("[Status | %s]\n  %s\n  %s"),
	       *Context, *AIInfo, *PlayerInfo);
}

void AGASPAIController::StartAlertTimer()
{
	GetWorldTimerManager().ClearTimer(AlertTimer);

	GetWorldTimerManager().SetTimer(
		AlertTimer,
		this,
		&AGASPAIController::OnAlertExpired,
		10.f,
		false
	);
}

void AGASPAIController::OnAlertExpired()
{
	if (GetAIState() == EAIState::Alert)
	{
		SetAIState(EAIState::Patrol);
		AlertPhase     = EAlertPhase::Unaware;
		DetectionMeter = 0.f;
		SuspectedActor = nullptr;
        
		bHasCalledForBackup = false;
       
		// Reset the spam counter because they gave up the search
		CurrentDistractionCount = 0;

		// Force explicit UI clear!
		ClearStealthUI();
		// Allow the UI to be triggered again later
		bIsStealthUIHidden = false; 
	}
}

void AGASPAIController::UpdateDetectionMeter(float DeltaTime)
{
    // Stop dead bodies from running stealth math and keeping the UI alive
    if (GetAIState() == EAIState::Dead) return;

	// Pause meter decay and growth while in stasis
	if (IsInStasis()) return;
	
    // Stop companions from running stealth math
    bool bAmICompanion = Cast<AGASPCompanionCharacter>(OwnedBaseChar) != nullptr || Cast<AGASPPlayerCharacter>(OwnedBaseChar) != nullptr;

    if (bAmICompanion) return;
    
    if (AlertPhase == EAlertPhase::Alert) return;

    // 1. UPDATE THE METER MATH
    if (SuspectedActor && bIsVisuallySensingTarget)
    {
       DetectionMeter = FMath::Min(1.f, DetectionMeter + CalculateDetectionRate(SuspectedActor) * DeltaTime);
    }
    // Only decay the meter if they are NOT actively investigating
    else if (GetAIState() != EAIState::Alert)
    {
       DetectionMeter = FMath::Max(0.f, DetectionMeter - DetectionDecayRate * DeltaTime);

       if (DetectionMeter <= 0.f && AlertPhase != EAlertPhase::Unaware)
       {
          SetAlertPhase(EAlertPhase::Unaware);
          SuspectedActor = nullptr;
          
          // Use our new explicit clear function!
          ClearStealthUI();
          // Allow it to be drawn again next time we see them
          bIsStealthUIHidden = false; 
       }
       // Handle UI turning back to yellow if it drops from orange
       else if (DetectionMeter < CautionThreshold && DetectionMeter >= SuspiciousThreshold && AlertPhase == EAlertPhase::Caution)
       {
          SetAlertPhase(EAlertPhase::Suspicious);
       }
    }

    // 2. CHECK PHASE TRANSITIONS (Going Up)
    if (DetectionMeter >= 1.f)
    {
       SetAlertPhase(EAlertPhase::Alert);
       SetTargetActor(SuspectedActor);
        
       // "Surprise" Freeze 
       StopMovement(); 
       SetAIState(EAIState::Engage);
        
       // Wait 1 second before hiding the fully-red UI
       GetWorldTimerManager().SetTimer(ClearUITimer, this, &AGASPAIController::ClearStealthUI, StealthUIClearTime, false);

       if (!bHasCalledForBackup && !GetWorldTimerManager().IsTimerActive(GlobalAlertDelayTimer))
       {
          GetWorldTimerManager().SetTimer(GlobalAlertDelayTimer, this, &AGASPAIController::BroadcastGlobalAlert, TimeToCallForBackup, false);
       }

       if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
          BTComp->RestartTree();

    }
    else if (DetectionMeter >= CautionThreshold && AlertPhase < EAlertPhase::Caution)
    {
       SetAlertPhase(EAlertPhase::Caution);

    }
    else if (DetectionMeter >= SuspiciousThreshold && AlertPhase < EAlertPhase::Suspicious)
    {
       SetAlertPhase(EAlertPhase::Suspicious);

       // BREAK PATROL AND INVESTIGATE
       if (SuspectedActor && GetAIState() != EAIState::Engage)
       {
          SetLastKnownLocation(SuspectedActor->GetActorLocation());
          SetAIState(EAIState::Alert);
          StartAlertTimer();

          if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
          {
             BTComp->RestartTree();
          }
       }
    }

    // 3. UPDATE THE UI
    if (!bIsStealthUIHidden)
    {
       if (UStealthSubsystem* Sub = GetWorld()->GetSubsystem<UStealthSubsystem>())
       {
           // Only push to the subsystem if the meter is active (> 0). 
           // If it is 0, we already forcefully cleared it above!
           if (DetectionMeter > 0.f)
           {
               Sub->RegisterDetection(this, DetectionMeter, static_cast<uint8>(AlertPhase));
           }
       }
    }
}

float AGASPAIController::CalculateDetectionRate(AActor* TargetActor) const
{
    APawn* OwnerPawn = GetPawn();
    if (!OwnerPawn || !TargetActor) return 0.f;

    FVector ToTarget = (TargetActor->GetActorLocation() - OwnerPawn->GetActorLocation()).GetSafeNormal();
    float Dot = FVector::DotProduct(OwnerPawn->GetActorForwardVector(), ToTarget);

    // --- VISION CONE GATE ---
    // Anything outside the peripheral vision angle contributes zero detection.
    float HalfAngleDeg = SightConfig ? SightConfig->PeripheralVisionAngleDegrees : 60.f;
    float ConeDot = FMath::Cos(FMath::DegreesToRadians(HalfAngleDeg));
    if (Dot < ConeDot) return 0.f;

    // --- LINE OF SIGHT GATE ---
    // If geometry sits between the AI's eyes and the target, no detection.
    FHitResult Hit;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerPawn);
    QueryParams.AddIgnoredActor(TargetActor);
    const FVector EyeOffset(0.f, 0.f, 64.f);

    bool bBlocked = GetWorld()->LineTraceSingleByChannel(
        Hit,
        OwnerPawn->GetActorLocation() + EyeOffset,
        TargetActor->GetActorLocation() + EyeOffset,
        ECC_Visibility,
        QueryParams);

    if (bBlocked) return 0.f;

    float Rate = BaseDetectionRate;

    // Distance falloff — full rate up close, near-zero at the sight radius boundary
    float Dist = FVector::Dist(OwnerPawn->GetActorLocation(), TargetActor->GetActorLocation());
    float SightRadius = SightConfig ? SightConfig->SightRadius : 2000.f;
    Rate *= FMath::Clamp(1.f - (Dist / SightRadius), 0.05f, 1.f);

    // Angle falloff — 30% at the cone edge, 100% dead ahead (no longer bleeds outside cone)
    float AngleFactor = FMath::Clamp((Dot - ConeDot) / (1.f - ConeDot), 0.f, 1.f);
    Rate *= FMath::Lerp(0.3f, 1.f, AngleFactor);

    // Player stealth exposure (crouching, sprinting, etc.)
    if (UStealthComponent* SC = TargetActor->FindComponentByClass<UStealthComponent>())
        Rate *= SC->GetExposureLevel();

    return Rate;
}

void AGASPAIController::SetAlertPhase(EAlertPhase NewPhase)
{
	if (AlertPhase == NewPhase) return;
	AlertPhase = NewPhase;

	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->SetValueAsEnum(FName("AlertPhase"), (uint8)AlertPhase);
		if (SuspectedActor && NewPhase >= EAlertPhase::Suspicious)
			BB->SetValueAsVector(FName("InvestigateLocation"), SuspectedActor->GetActorLocation());
	}
}

void AGASPAIController::OnGlobalAlert(AActor* AlertInstigator, float AlertRadius)
{
	// --- Ignore radio calls while in stasis ---
	if (IsInStasis()) return;
	
	// Stop companions from answering enemy radio calls
	bool bAmICompanion = Cast<AGASPCompanionCharacter>(OwnedBaseChar) != nullptr || Cast<AGASPPlayerCharacter>(OwnedBaseChar) != nullptr;

	if (bAmICompanion)
	{
		// Companions go weapons-free the instant the player is detected —
		// no distance check needed, this is a squad-wide stealth break.
		if (Blackboard) Blackboard->SetValueAsBool(BB_bWeaponsFree, true);
		return;
	}
    
	// If we are ALREADY engaging the player, or we are dead, ignore the radio call
	if (GetAIState() == EAIState::Engage || GetAIState() == EAIState::Dead) return;

	bHasCalledForBackup = true;
    
	APawn* OwnerPawn = GetPawn();
	if (!OwnerPawn || !AlertInstigator) return;

	if (AlertRadius > 0.f)
	{
		float Dist = FVector::Dist(OwnerPawn->GetActorLocation(), AlertInstigator->GetActorLocation());
		if (Dist > AlertRadius) return;
	}
	
	// 1. We know SOMEONE is there, but we DO NOT have visual confirmation yet!
	SuspectedActor = AlertInstigator;
	
	// 2. Bring them to High Suspicion so they are actively looking, but DO NOT push to 1.0!
	// (If it hits 1.0, the Tick function will instantly lock on and Engage)
	DetectionMeter = FMath::Max(DetectionMeter, 0.85f);
	SetAlertPhase(EAlertPhase::Caution);

	// 3. Tell the Behavior Tree where the radio call came from
	SetLastKnownLocation(AlertInstigator->GetActorLocation());

	// 4. Put them in Search/Alert mode, NOT Engage mode
	SetAIState(EAIState::Alert);
	StartAlertTimer();

	// 5. Wake up the Behavior Tree so they drop their patrol and run to investigate
	if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
	{
		BTComp->RestartTree();
	}
}

void AGASPAIController::OnGlobalAlertCleared()
{
	bool bAmICompanion = Cast<AGASPCompanionCharacter>(OwnedBaseChar) != nullptr || Cast<AGASPPlayerCharacter>(OwnedBaseChar) != nullptr;
	if (bAmICompanion && Blackboard)
	{
		Blackboard->SetValueAsBool(BB_bWeaponsFree, false);
	}
}

void AGASPAIController::ClearStealthUI()
{
	bIsStealthUIHidden = true;

	if (UStealthSubsystem* Sub = GetWorld()->GetSubsystem<UStealthSubsystem>())
	{
		// Safely unregister instantly! We no longer push a 0.f here.
		Sub->UnregisterDetection(this); 
	}

	// FORCE the update to all human players explicitly right now.
	// This completely bypasses the race condition where the Server drops the RPC.
	UWorld* World = GetWorld();
	if (World)
	{
		APawn* UIBody = OwnedBaseChar ? OwnedBaseChar : CachedUIBody;
		if (UIBody)
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(It->Get()))
				{
					// Send a hard 0 to everyone's screen to safely clear it!
					PC->Client_UpdateDetectionHUD(UIBody, 0.f, 0);
				}
			}
		}
	}
}
void AGASPAIController::BroadcastGlobalAlert()
{
	if (GetAIState() == EAIState::Dead || !OwnedBaseChar) return;

	// Directly check if the physical body is dead
	if (AGASPAICharacterBase* AIChar = Cast<AGASPAICharacterBase>(OwnedBaseChar))
	{
		if (!AIChar->IsAlive()) return; // Dead men tell no tales!
	}

	bHasCalledForBackup = true;

	if (UStealthSubsystem* Sub = GetWorld()->GetSubsystem<UStealthSubsystem>())
	{
		AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(BB_TargetActor));

		if (!Target) Target = SuspectedActor;

		if (Target)
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("AI: CONTACT! SEND BACKUP!"));
			Sub->TriggerGlobalAlert(Target, AlertPropagationRadius);
		}
	}
}

bool AGASPAIController::IsInStasis() const
{
	if (!Blackboard) return false;
    
	// If they are frozen, caged, in stasis, OR electrocuted, their perception is paused!
	return Blackboard->GetValueAsBool(BB_bIsFrozen) || 
		   Blackboard->GetValueAsBool(BB_bIsCaged) || 
		   Blackboard->GetValueAsBool(BB_bIsInStasis) ||
		   Blackboard->GetValueAsBool(BB_bIsElectrocuted);
}