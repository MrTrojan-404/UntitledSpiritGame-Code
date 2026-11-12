#include "FrostLockSpell.h"
#include "MoverComponent.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "GameAnimationSample/SpellAbility/SpellComponent.h"
#include "GameAnimationSample/SpellAbility/Projectile/FrostIcicleProjectile.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"

void UFrostLockSpell::Execute_Implementation(AGASPBaseCharacter* Caster)
{
	if (!Caster || !Caster->HasAuthority()) return;

	CachedCaster = Caster;
	LockedTargets.Empty();

	Caster->GetWorldTimerManager().SetTimer(ScanTimer, this, &UFrostLockSpell::ScanForTarget, LockScanInterval, true);
}

void UFrostLockSpell::ScanForTarget()
{
	if (!CachedCaster) return;

	USpellComponent* SpellComp = CachedCaster->FindComponentByClass<USpellComponent>();
	int32 AvailableEnergy = SpellComp ? SpellComp->GetCurrentEnergySegments() : 0;
	
	// The upfront cast cost covers the first lock. 
	// Each additional lock requires 1 more available energy.
	int32 AllowedLocks = FMath::Min(MaxLocks, 1 + AvailableEnergy);

	// Stop scanning if we hit our MaxLocks limit OR if we are out of energy
	if (LockedTargets.Num() >= AllowedLocks) return;

	AController* Controller = CachedCaster->GetController();
	if (!Controller) return;

	FVector ViewLoc;
	FRotator ViewRot;
	Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
	FVector TraceEnd = ViewLoc + (ViewRot.Vector() * LockTraceRange);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CachedCaster);

	FHitResult Hit;
	if (!CachedCaster->GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc, TraceEnd, ECC_Visibility, Params)) return;

	AGASPBaseCharacter* Target = Cast<AGASPBaseCharacter>(Hit.GetActor());
	if (!Target || Target->IsPlayerAlly()) return;
	if (!Target->CanBeCrowdControlled()) return;

	bool bAlreadyLocked = LockedTargets.ContainsByPredicate(
		[Target](const TWeakObjectPtr<AGASPBaseCharacter>& P) { return P.Get() == Target; });
	if (bAlreadyLocked) return;

	// If we already used our "free" lock from the initial cast, deduct 1 energy for this new lock
	if (LockedTargets.Num() >= 1 && SpellComp)
	{
		// Passing a negative number subtracts energy and instantly updates the HUD
		SpellComp->RefundEnergySegments(-1);
	}

	LockedTargets.Add(Target);
	Target->OnFrostLocked(true);
	CachedCaster->Client_SetTargetOutline(Target, true, FrostLockedStencil);
}

void UFrostLockSpell::OnReleased_Implementation(AGASPBaseCharacter* Caster)
{
	if (!Caster || !Caster->HasAuthority()) return;

	//Ensure the spell actually successfully started ===
	if (!CachedCaster) return;

	Caster->GetWorldTimerManager().ClearTimer(ScanTimer);

	// DUMBFIRE MODE 
	if (LockedTargets.Num() == 0)
	{
		// The 1 energy was already deducted in Execute. 
		LaunchIcicle(nullptr, 0);
		CachedCaster = nullptr; // Reset for next cast
		return;
	}

	for (int32 i = 0; i < LockedTargets.Num(); i++)
	{
		FTimerHandle LaunchDelayHandle;
		float Delay = FMath::Max(LaunchStaggerDelay * i, 0.01f);
		AGASPBaseCharacter* Target = LockedTargets[i].Get();
		
		FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &UFrostLockSpell::LaunchIcicle, Target, i);
		if (Caster) Caster->GetWorldTimerManager().SetTimer(LaunchDelayHandle, Del, Delay, false);
	}
	
	LockedTargets.Empty();
	CachedCaster = nullptr;
}

void UFrostLockSpell::LaunchIcicle(AGASPBaseCharacter* Target, int32 Index)
{
	if (!CachedCaster) return;

	FVector SpawnLoc = CachedCaster->GetActorLocation() + FVector(0.f, 0.f, 70.f) - (CachedCaster->GetActorRightVector() * 40.f);
	SpawnLoc += CachedCaster->GetActorRightVector() * (Index * 15.f);

	FVector TargetLoc;
	if (Target)
	{
		// Lock-on aiming
		TargetLoc = Target->GetActorLocation() + FVector(0.f, 0.f, 50.f);
		CachedCaster->Client_SetTargetOutline(Target, false, 0); 
	}
	else
	{
		if (UCombatComponent* Combat = CachedCaster->GetCombatComponent())
		{
			TargetLoc = Combat->GetHitTarget();
		}
		else
		{
			// Absolute fallback if combat component is missing
			TargetLoc = SpawnLoc + (CachedCaster->GetActorForwardVector() * 5000.f); 
		}
	}

	FVector LaunchDir = (TargetLoc - SpawnLoc).GetSafeNormal();
	FTransform SpawnTransform(LaunchDir.Rotation(), SpawnLoc);

	UClass* ClassToSpawn = IcicleProjectileClass ? IcicleProjectileClass.Get() : AFrostIcicleProjectile::StaticClass();
	AFrostIcicleProjectile* Icicle = CachedCaster->GetWorld()->SpawnActorDeferred<AFrostIcicleProjectile>(
		ClassToSpawn, SpawnTransform);

	if (Icicle)
	{
		Icicle->Initialize(CachedCaster, Target, this);
		Icicle->FinishSpawning(SpawnTransform);

		if (CastSound)
		{
			UGameplayStatics::PlaySoundAtLocation(CachedCaster, CastSound, SpawnLoc);
		}
	}
}

void UFrostLockSpell::ApplyFreezeToTarget(AGASPBaseCharacter* Target)
{
	if (!Target) return;

	// APPLY DAMAGE ON HIT
	UGameplayStatics::ApplyDamage(Target, InitialDamage, CachedCaster ? CachedCaster->GetController() : nullptr, CachedCaster, UDamageType::StaticClass());

	// START CONTINUOUS DAMAGE
	FTimerHandle DamageTimer;
	FTimerDelegate DamageDel = FTimerDelegate::CreateUObject(this, &UFrostLockSpell::ContinuousDamageTick, Target);
	Target->GetWorldTimerManager().SetTimer(DamageTimer, DamageDel, TickInterval, true);
	ContinuousDamageTimers.Add(Target, DamageTimer);

	// Original Audio/Stun logic...
	UAISense_Hearing::ReportNoiseEvent(Target->GetWorld(), Target->GetActorLocation(), 1.f, CachedCaster, 300.f, FName("Distraction"));
	
	if (UCombatComponent* Combat = Target->GetCombatComponent()) Combat->HandleFire(false);

	if (AGASPAIController* AICon = Cast<AGASPAIController>(Target->GetController()))
	{
		if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
			BB->SetValueAsBool(AGASPAIController::BB_bIsFrozen, true);
	}

	if (UMoverComponent* TargetMover = Target->FindComponentByClass<UMoverComponent>())
	{
		TSharedPtr<FLayeredMove_LinearVelocity> FreezeMove = MakeShared<FLayeredMove_LinearVelocity>();
		FreezeMove->Velocity = FVector::ZeroVector;
		FreezeMove->DurationMs = FreezeDuration * 1000.f;
		FreezeMove->MixMode = EMoveMixMode::OverrideVelocity;
		TargetMover->QueueLayeredMove(FreezeMove);
	}

	Target->Multicast_SetFrozen(true, FreezeMaterial, FrozenSkeletalVFX);
	Target->OnFrostLocked(false);

	FTimerHandle ShatterTimer;
	FTimerDelegate ShatterDel = FTimerDelegate::CreateUObject(this, &UFrostLockSpell::ShatterTarget, Target);
	Target->GetWorldTimerManager().SetTimer(ShatterTimer, ShatterDel, FreezeDuration, false);
}

void UFrostLockSpell::ContinuousDamageTick(AGASPBaseCharacter* Target)
{
	if (Target)
	{
		UGameplayStatics::ApplyDamage(Target, DamagePerTick, CachedCaster ? CachedCaster->GetController() : nullptr, CachedCaster, UDamageType::StaticClass());
	}
}

void UFrostLockSpell::ShatterTarget(AGASPBaseCharacter* Target)
{
	if (!Target) return;

	// CLEAR CONTINUOUS DAMAGE
	if (ContinuousDamageTimers.Contains(Target))
	{
		Target->GetWorldTimerManager().ClearTimer(ContinuousDamageTimers[Target]);
		ContinuousDamageTimers.Remove(Target);
	}

	if (AGASPAIController* AICon = Cast<AGASPAIController>(Target->GetController()))
	{
		if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
			BB->SetValueAsBool(AGASPAIController::BB_bIsFrozen, false);
	}

	Target->Multicast_SetFrozen(false);

	if (ShatterSound) UGameplayStatics::PlaySoundAtLocation(Target, ShatterSound, Target->GetActorLocation());
	UAISense_Hearing::ReportNoiseEvent(Target->GetWorld(), Target->GetActorLocation(), 1.f, CachedCaster, 600.f, FName("Distraction"));

	UGameplayStatics::ApplyDamage(Target, DamagePerTarget, CachedCaster ? CachedCaster->GetController() : nullptr, CachedCaster, UDamageType::StaticClass());
}