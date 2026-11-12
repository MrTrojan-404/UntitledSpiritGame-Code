#include "StasisSpell.h"

#include "AIController.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "MoverComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "Engine/OverlapResult.h"
#include "GameAnimationSample/Actor/EnvironmentInteractable/Explosives/ExplosiveBarrel.h"
#include "GameAnimationSample/Actor/Weapon/Projectile/Projectile.h"
#include "GameAnimationSample/SpellAbility/Projectile/MicroMissileProjectile.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "GameAnimationSample/SpellAbility/SpellComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "GameAnimationSample/Actor/Weapon/HitScanWeapon.h"

void UStasisSpell::Execute_Implementation(AGASPBaseCharacter* Caster)
{
	if (!Caster || !Caster->HasAuthority() || bStasisActive) return;

	if (USpellComponent* SpellComp = Caster->FindComponentByClass<USpellComponent>())
	{
		SpellComp->Multicast_ActivateStasis(StasisDuration, GlobalDilationFactor);
	}
}

void UStasisSpell::TickStasis(float RealDeltaSeconds)
{
	if (!CachedCaster || !bStasisActive) return;

	// 1. Everyone checks for new projectiles flying into the bubble every frame
	CatchNewProjectiles();

	// 2. Only the Server tracks the timer
	if (CachedCaster->HasAuthority())
	{
		ElapsedRealTime += RealDeltaSeconds;
		if (ElapsedRealTime >= StasisDuration)
		{
			if (USpellComponent* SpellComp = CachedCaster->FindComponentByClass<USpellComponent>())
			{
				SpellComp->Multicast_DeactivateStasis();
			}
		}
	}
}

void UStasisSpell::CatchNewProjectiles()
{
	if (!CachedCaster || !CachedCaster->GetWorld()) return;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CachedCaster); // Ignore the caster's body

	TArray<FOverlapResult> ProjOverlaps;
	CachedCaster->GetWorld()->OverlapMultiByObjectType(
		ProjOverlaps,
		StasisOrigin, // Use the stored center of the bubble
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_WorldDynamic),
		FCollisionShape::MakeSphere(StasisRadius),
		Params
	);

	for (const FOverlapResult& ProjOverlap : ProjOverlaps)
	{
		if (AActor* ProjActor = ProjOverlap.GetActor())
		{
			bool bAlreadyCaught = AffectedProjectiles.ContainsByPredicate(
				[ProjActor](const TWeakObjectPtr<AActor>& P) { return P.Get() == ProjActor; });

			bool bIsProjectile = (ProjActor->FindComponentByClass<UProjectileMovementComponent>() != nullptr);
			bool bIsBarrel = ProjActor->IsA(AExplosiveBarrel::StaticClass());

			if (!bAlreadyCaught && (bIsProjectile || bIsBarrel))
			{
				if (AProjectile* Projectile = Cast<AProjectile>(ProjActor))
				{
					Projectile->SetStasis(true, GlobalDilationFactor); 
					Projectile->DestroyTime += StasisDuration;
				}
				else if (AExplosiveBarrel* Barrel = Cast<AExplosiveBarrel>(ProjActor))
				{
					Barrel->SetStasis(true, GlobalDilationFactor);
				}
				else if (AMicroMissileProjectile* Missile = Cast<AMicroMissileProjectile>(ProjActor))
				{
					// Explicitly handle the Micro Missile so it pauses its timers!
					Missile->SetStasis(true, GlobalDilationFactor);
					if (Missile->GetLifeSpan() > 0.f)
					{
						Missile->SetLifeSpan(Missile->GetLifeSpan() + StasisDuration);
					}
				}
				else
				{
					// Fallback for standalone spell projectiles
					ProjActor->CustomTimeDilation = GlobalDilationFactor;
					if (ProjActor->GetLifeSpan() > 0.f)
					{
						ProjActor->SetLifeSpan(ProjActor->GetLifeSpan() + StasisDuration);
					}
				}
				
				AffectedProjectiles.Add(ProjActor);
			}
		}
	}
}

void UStasisSpell::LocalActivate(AGASPBaseCharacter* Caster, float Duration, float DilationFactor)
{
	CachedCaster = Caster;
	bStasisActive = true;
	ElapsedRealTime = 0.f;
	StasisDuration = Duration;
	AffectedTargets.Empty();
	AffectedProjectiles.Empty();

	Caster->OnStasisActive(true);

	// Save exactly where the caster was standing when the spell was popped
	StasisOrigin = Caster->GetActorLocation(); 

	// ==========================================
	// AUDIO: Play Cast & Spawn Loop
	// ==========================================
	if (CastSound)
	{
		UGameplayStatics::PlaySoundAtLocation(Caster, CastSound, StasisOrigin);
	}
	
	if (LoopSound)
	{
		ActiveLoopSound = UGameplayStatics::SpawnSoundAtLocation(Caster, LoopSound, StasisOrigin);
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Caster);

	// --- 1. SWEEP FOR ENEMIES ---
	TArray<FOverlapResult> Overlaps;
	Caster->GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		StasisOrigin, 
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(StasisRadius),
		Params
	);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (AGASPBaseCharacter* Target = Cast<AGASPBaseCharacter>(Overlap.GetActor()))
		{
			if (Target->IsPlayerAlly() || !Target->CanBeCrowdControlled()) continue;

			AffectedTargets.Add(Target);

			// A. Slow Animations
			Target->CustomTimeDilation = DilationFactor;

			// Dilate the weapon!
			TArray<AActor*> AttachedActors;
			Target->GetAttachedActors(AttachedActors);
			for (AActor* Attached : AttachedActors)
			{
				Attached->CustomTimeDilation = DilationFactor;
			}
			
			// B. Stop Momentum
			if (UMoverComponent* TargetMover = Target->FindComponentByClass<UMoverComponent>())
			{
				TSharedPtr<FLayeredMove_LinearVelocity> FreezeMove = MakeShared<FLayeredMove_LinearVelocity>();
				FreezeMove->Velocity = FVector::ZeroVector; 
				FreezeMove->DurationMs = Duration * 1000.f;
				FreezeMove->MixMode = EMoveMixMode::OverrideVelocity;
				TargetMover->QueueLayeredMove(FreezeMove);
			}

			// C. Update AI Brain
			if (AGASPAIController* AICon = Cast<AGASPAIController>(Target->GetController()))
			{
				if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
				{
					BB->SetValueAsBool(AGASPAIController::BB_bIsInStasis, true);
				}
			}
		}
	}

	// --- 2. CATCH PROJECTILES ---
	CatchNewProjectiles();

	// --- 3. BUFFER CASTER'S WEAPONS ---
	if (UCombatComponent* Combat = Caster->FindComponentByClass<UCombatComponent>())
	{
		FlaggedWeapons.Empty();
		if (AWeapon* Primary = Combat->GetPrimaryWeapon())
			RegisterWeaponForStasis(Primary);
		if (AWeapon* Secondary = Combat->GetSecondaryWeapon())
			RegisterWeaponForStasis(Secondary);
	}
}

void UStasisSpell::LocalDeactivate()
{
	bStasisActive = false;

	if (CachedCaster)
	{
		CachedCaster->OnStasisActive(false);
	}

	// ==========================================
	// AUDIO: Stop Loop & Play Shatter/End Sound
	// ==========================================
	if (ActiveLoopSound)
	{
		ActiveLoopSound->Stop();
		ActiveLoopSound = nullptr;
	}

	if (EndSound && CachedCaster)
	{
		UGameplayStatics::PlaySoundAtLocation(CachedCaster, EndSound, StasisOrigin);
	}

	// 1. Restore Enemies
	for (TWeakObjectPtr<AGASPBaseCharacter> TargetPtr : AffectedTargets)
	{
		if (AGASPBaseCharacter* Target = TargetPtr.Get())
		{
			Target->CustomTimeDilation = 1.f;

			// Restore the weapon
			TArray<AActor*> AttachedActors;
			Target->GetAttachedActors(AttachedActors);
			for (AActor* Attached : AttachedActors)
			{
				Attached->CustomTimeDilation = 1.f;
			}
			
			// Un-jam the fire timers
			if (UCombatComponent* Combat = Target->FindComponentByClass<UCombatComponent>())
			{
				Combat->ResetFireTimer();
			}
			
			// Unpause AI Perception
			if (AGASPAIController* AICon = Cast<AGASPAIController>(Target->GetController()))
			{
				if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
				{
					BB->SetValueAsBool(AGASPAIController::BB_bIsInStasis, false);
				}

				// Force the AI to forget everyone so it immediately re-evaluates its surroundings
				if (UAIPerceptionComponent* PerceptionComp = AICon->GetPerceptionComponent())
				{
					PerceptionComp->ForgetAll();
				}
			}
		}
	}

	// 2. Restore Projectiles and Barrels
	for (TWeakObjectPtr<AActor> ProjPtr : AffectedProjectiles)
	{
		if (AActor* Proj = ProjPtr.Get())
		{
			if (AProjectile* Projectile = Cast<AProjectile>(Proj))
			{
				Projectile->SetStasis(false);
			}
			else if (AExplosiveBarrel* Barrel = Cast<AExplosiveBarrel>(Proj))
			{
				Barrel->SetStasis(false);
			}
			else if (AMicroMissileProjectile* Missile = Cast<AMicroMissileProjectile>(Proj))
			{
				Missile->SetStasis(false);
			}
			else
			{
				Proj->CustomTimeDilation = 1.f;
			}
		}
	}

	// --- 3. RELEASE BUFFERED SHOTS ---
	for (TWeakObjectPtr<AWeapon> WeaponPtr : FlaggedWeapons)
	{
		if (AWeapon* Weapon = WeaponPtr.Get())
		{
			Weapon->bIsWeaponInStasis = false;
			Weapon->StasisDilation = 1.0f;

			// We still only want to call ReleaseBufferedShots on HitScan weapons
			if (AHitScanWeapon* HitScan = Cast<AHitScanWeapon>(Weapon))
			{
				HitScan->ReleaseBufferedShots();
			}
		}
	}
	FlaggedWeapons.Empty();

	AffectedTargets.Empty();
	AffectedProjectiles.Empty();
	CachedCaster = nullptr;

	
}

void UStasisSpell::RegisterWeaponForStasis(AWeapon* NewWeapon)
{
	if (!bStasisActive || !NewWeapon) return;
	NewWeapon->bIsWeaponInStasis = true;
	NewWeapon->StasisDilation = GlobalDilationFactor; // Save this so the bullet knows how slow to go!
	FlaggedWeapons.AddUnique(NewWeapon);
}