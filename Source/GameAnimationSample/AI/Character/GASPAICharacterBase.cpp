#include "GASPAICharacterBase.h"
#include "MotionWarpingComponent.h"
#include "BrainComponent.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"
#include "Perception/AISense_Damage.h"
#include "MoverComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/AudioComponent.h"
#include "DefaultMovementSet/LayeredMoves/AnimRootMotionLayeredMove.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "GameAnimationSample/SpellAbility/SpellComponent.h"
#include "GameAnimationSample/UI/HUD/GASPHUD.h"
#include "GameAnimationSample/UI/Widgets/CharacterOverlay.h"

AGASPAICharacterBase::AGASPAICharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AGASPAICharacterBase::BeginPlay()
{
	Super::BeginPlay();
}

void AGASPAICharacterBase::OnDamageReceived(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser)
{
	if (InstigatorController && InstigatorController->GetPawn())
	{
		UAISense_Damage::ReportDamageEvent(
			GetWorld(),
			this,                              // Who got hurt (The AI)
			InstigatorController->GetPawn(),   // Who shot us (The Player)
			Damage,                            // How much damage
			GetActorLocation(),            
			DamageCauser->GetActorLocation()   // Where the bullet came from
		);
	}

	if (bIsDead) return;

	if (Health <= 0.f)
	{
		HandleDeath(InstigatorController, DamageCauser);
	}
}

bool AGASPAICharacterBase::CanPerformStealthTakedown_Implementation(AActor* Interactor) const
{
	if (!Interactor) return false;

	// If the AI is already dead, or currently locked in an animation, hide the prompt!
	if (bIsDead || bDisableGameplay) 
	{
		return false;
	}

	// If the Player is already attacking/locked, hide the prompt!
	AGASPBaseCharacter* Attacker = Cast<AGASPBaseCharacter>(Interactor);
	if (Attacker && Attacker->bDisableGameplay) 
	{
		return false;
	}
	
	// --- FRIENDLY GUARD ---
	// Instantly reject the takedown if this character is flagged as an ally!
	if (IsPlayerAlly()) 
	{
		return false;
	}
	
	// 2. POSITION CHECK: Is the player behind the enemy?
	FVector DirectionToPlayer = Interactor->GetActorLocation() - GetActorLocation();
	DirectionToPlayer.Z = 0.f; // Flatten to 2D plane so height differences don't break the math
	DirectionToPlayer.Normalize();

	FVector EnemyForward = GetActorForwardVector();
	EnemyForward.Z = 0.f;
	EnemyForward.Normalize();

	// The Dot Product returns 1 if facing the same way, -1 if facing opposite.
	// If the vector pointing from the enemy to the player is opposite to the enemy's forward vector, the player is behind them.
	float Dot = FVector::DotProduct(EnemyForward, DirectionToPlayer);
	
	// -0.5f means the player must be within a ~120 degree cone behind the enemy's back
	return Dot < -0.5f;
}

void AGASPAICharacterBase::ExecuteStealthTakedown_Implementation(AActor* Interactor)
{
	if (HasAuthority())
	{
		MulticastExecuteStealthTakedown(Interactor);
	}
}

void AGASPAICharacterBase::MulticastExecuteStealthTakedown_Implementation(AActor* Interactor)
{
	AGASPBaseCharacter* Attacker = Cast<AGASPBaseCharacter>(Interactor);
	if (!Attacker || !Attacker->MotionWarpingComponent) return;

	// 1 & 2. Memorize
	this->TakedownInstigator = Attacker;
	Attacker->CurrentTakedownTarget = this;

	// 3. WARP: Use the Socket for Position
	FVector WarpLocation = CharacterMesh->GetSocketLocation(FName("Takedown_Socket"));
	FRotator WarpRotation =  CharacterMesh->GetSocketRotation(FName("Takedown_Socket"));
	WarpRotation.Pitch = 0.0f;
	WarpRotation.Roll = 0.0f;

	Attacker->MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(FName("TakedownTarget"), WarpLocation, WarpRotation);

	// 4. Lock States
	bDisableGameplay = true; 
	Attacker->bDisableGameplay = true;

	// 5. DISABLE COLLISIONS (BOTH ACTORS)
	if (CapsuleComponent)
	{
		CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		CapsuleComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		CapsuleComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		CapsuleComponent->ComponentVelocity = FVector::ZeroVector;
		CapsuleComponent->SetEnableGravity(false); 
	}
	if (Attacker->CapsuleComponent)
	{
		Attacker->CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		Attacker->CapsuleComponent->ComponentVelocity = FVector::ZeroVector;
		Attacker->CapsuleComponent->SetEnableGravity(false); 
	}

	if (this->MoverComponent)
	{
		this->MoverComponent->SetComponentTickEnabled(false); 
		// SAFEGUARD: Only the Server is allowed to actually destroy replicated components
		if (HasAuthority()) 
		{
			this->MoverComponent->DestroyComponent();
		}
	}
    
	Attacker->MoveIgnoreActorAdd(this);
	this->MoveIgnoreActorAdd(Attacker);

	// 6. Shut down AI thinking
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
	   if (AIC->GetBrainComponent()) AIC->GetBrainComponent()->StopLogic("Assassinated");
	}

	// 7. Determine which Attacker Montage to use
	UAnimMontage* MontageToPlay = Attacker->AttackerTakedownMontage ? Attacker->AttackerTakedownMontage : Attacker->MeleeMontage;
	float AttackerAnimLength = 1.0f;

	// 8. Play Attacker Montage
	if (MontageToPlay && Attacker->CharacterMesh)
	{
	   if (UAnimInstance* AttackerAnim = Attacker->CharacterMesh->GetAnimInstance())
	   {
		AttackerAnimLength = AttackerAnim->Montage_Play(MontageToPlay);
	   }
	}

	// 9. THE MOVER ROOT MOTION EXTRACTION
	if (UMoverComponent* AttackerMoverComp = Attacker->MoverComponent)
	{
		TSharedPtr<FLayeredMove_AnimRootMotion> RootMotionMove = MakeShared<FLayeredMove_AnimRootMotion>();
		RootMotionMove->MontageState.Montage = MontageToPlay;
		RootMotionMove->MontageState.PlayRate = 1.0f;
		RootMotionMove->DurationMs = AttackerAnimLength * 1000.0f;
		AttackerMoverComp->QueueLayeredMove(RootMotionMove);
	}
	
	// 10. Play Victim Montage
	if (VictimTakedownMontage && CharacterMesh)
	{
	   if (UAnimInstance* VictimAnim = CharacterMesh->GetAnimInstance())
	   {
		   VictimAnim->Montage_Play(VictimTakedownMontage);
	   }
	}
}

void AGASPAICharacterBase::HandleDeath(AController* InstigatorController, AActor* DamageCauser)
{
	if (bIsDead) return;

	// credit whoever landed the kill, distinguishing stealth vs combat via
	// TakedownInstigator, which the stealth takedown path already sets before this fires.
	AGASPBaseCharacter* Killer = nullptr;
	bool bWasStealthKill = false;

	if (TakedownInstigator.IsValid())
	{
		Killer = TakedownInstigator.Get();
		bWasStealthKill = true;
	}
	else if (InstigatorController)
	{
		Killer = Cast<AGASPBaseCharacter>(InstigatorController->GetPawn());
	}

	if (Killer)
	{
		if (USpellComponent* SpellComp = Killer->FindComponentByClass<USpellComponent>())
		{
			SpellComp->OnKillScored(bWasStealthKill);
		}
	}
	
	// Call this BEFORE bIsDead is true so the Stealth Subsystem still sees the AI as valid/alive when sending the RPC!
	NotifyControllerOfDeath();

	bIsDead = true;

	if (CombatComponent)
	{
		CombatComponent->DropEquippedWeapon();
	}

	// This already goes to everyone, so we'll put the visual triggers here!
	MulticastHandleDeath(LastHitDirection, LastHitBone);
	
	// Set lifespan so the actor is actually deleted from the server
	SetLifeSpan(DissolveDelay + 5.f);
}

void AGASPAICharacterBase::NotifyControllerOfDeath()
{
	// Send the death signal to the brain so it stops processing stealth math!
	if (AGASPAIController* AICon = Cast<AGASPAIController>(GetController()))
	{
		AICon->OnEnemyDied();
	}
}

void AGASPAICharacterBase::MulticastHandleDeath_Implementation(FVector_NetQuantize HitDirection, FName HitBone)
{
	bIsDead = true;
	LastHitDirection = HitDirection;
	LastHitBone = HitBone;

	if (CombatComponent) CombatComponent->HandleFire(false);

	SetReplicateMovement(false);

	if (CapsuleComponent)
	{
		CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		CapsuleComponent->SetEnableGravity(false); 
		CapsuleComponent->ComponentVelocity = FVector::ZeroVector;
	}

	if (CharacterMesh)
	{
		CharacterMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		CharacterMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		CharacterMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	}

	// Only delete the body if it is an Enemy!
	// Allies stay so they can be revived.
	if (IsPlayerAlly())
	{
		SetLifeSpan(0.f);
	}
	else
	{
		GetWorldTimerManager().SetTimer(DissolveTImer, this, &AGASPAICharacterBase::DissolveTimerFinished, DissolveDelay);
	}
	
	ApplyDeathPhysics();

	// --- LOCAL UI CLEANUP FOR CLIENTS ---
	// GetPlayerController with index 0 always gets the LOCAL player on the current machine
	if (APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		// Make sure we are only doing UI stuff for the actual human player looking at the screen
		if (LocalPC->IsLocalController()) 
		{
			if (AGASPCorePlayerController* GASPPC = Cast<AGASPCorePlayerController>(LocalPC))
			{
				if (AGASPHUD* HUD = Cast<AGASPHUD>(GASPPC->GetHUD()))
				{
					if (HUD->CharacterOverlay)
					{
						// Push a 0.f detection to the Overlay locally so it deletes the Ghost Meter!
						HUD->CharacterOverlay->UpdateStealthHUD(this, 0.f, 0);
					}
				}
			}
		}
	}
}

void AGASPAICharacterBase::TriggerElectrocutionDeath(AActor* InstigatorActor, UNiagaraSystem* ZapVFX, USoundBase* ZapSound)
{
	if (!HasAuthority() || bIsDead) return;

	bDisableGameplay = true;
	if (AGASPAIController* AIC = Cast<AGASPAIController>(GetController()))
	{
		if (AIC->GetBrainComponent()) AIC->GetBrainComponent()->StopLogic("Electrocuted");
	}
	if (CombatComponent) CombatComponent->HandleFire(false);

	TakedownInstigator = Cast<AGASPBaseCharacter>(InstigatorActor);

	// Figure out how long the animation takes
	float MontageLength = 2.0f; 
	if (ElectrocuteDeathMontage) MontageLength = ElectrocuteDeathMontage->GetPlayLength();

	// Cut the FX off slightly earlier than the full animation so it syncs with the ragdoll drop!
	float FXDuration = FMath::Max(0.1f, MontageLength - 0.25f); 

	// Play the effects and the animation
	Multicast_PlayElectrocuteFX(ZapVFX, ZapSound, FXDuration, true);
}

void AGASPAICharacterBase::Multicast_PlayElectrocuteFX_Implementation(UNiagaraSystem* ZapVFX, USoundBase* ZapSound, float Duration, bool bPlayDeathAnimation)
{
	if (bIsDead) return;

	// Only freeze their physics and play the death fall if they are actually dying
	if (bPlayDeathAnimation)
	{
		if (CapsuleComponent) CapsuleComponent->ComponentVelocity = FVector::ZeroVector;
		if (MoverComponent) MoverComponent->SetComponentTickEnabled(false); 

		if (ElectrocuteDeathMontage && CharacterMesh && CharacterMesh->GetAnimInstance())
		{
			CharacterMesh->GetAnimInstance()->Montage_Play(ElectrocuteDeathMontage);
		}
	}

	// 1. Spawn and save a reference to the VFX
	if (ZapVFX && CharacterMesh)
	{
		ActiveElectrocuteVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(ZapVFX, CharacterMesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
	}
	
	// 2. Spawn and save a reference to the Audio
	if (ZapSound && CharacterMesh)
	{
		ActiveElectrocuteSound = UGameplayStatics::SpawnSoundAttached(ZapSound, CharacterMesh);
	}

	// 3. Set a strict timer to shut them both down early!
	GetWorldTimerManager().SetTimer(ElectrocuteFXTimer, this, &AGASPAICharacterBase::StopElectrocuteFX, Duration, false);
}

void AGASPAICharacterBase::StopElectrocuteFX()
{
	if (ActiveElectrocuteVFX) 
	{
		ActiveElectrocuteVFX->Deactivate();
	}
	if (ActiveElectrocuteSound) 
	{
		ActiveElectrocuteSound->Stop();
	}

	// Wake the Heavy back up when the sparks stop!
	if (HasAuthority())
	{
		if (AGASPAIController* AIC = Cast<AGASPAIController>(GetController()))
		{
			if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
			{
				BB->SetValueAsBool(AGASPAIController::BB_bIsElectrocuted, false);
			}
		}
	}
}