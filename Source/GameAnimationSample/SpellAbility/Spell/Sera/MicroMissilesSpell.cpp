#include "MicroMissilesSpell.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/SpellAbility/Projectile/MicroMissileProjectile.h"

void UMicroMissilesSpell::Execute_Implementation(AGASPBaseCharacter* Caster)
{
	if (!Caster || !Caster->HasAuthority()) return;

	CachedCaster = Caster;
	
	for (TWeakObjectPtr<AGASPBaseCharacter> TargetPtr : LockedTargets)
	{
		if (AGASPBaseCharacter* OldTarget = TargetPtr.Get())
		{
			Caster->Client_SetTargetOutline(OldTarget, false, 0);
		}
	}
	LockedTargets.Empty();

	ScanForTarget();
	Caster->GetWorldTimerManager().SetTimer(ScanTimer, this, &UMicroMissilesSpell::ScanForTarget, ScanInterval, true);
}

void UMicroMissilesSpell::ScanForTarget()
{
	if (!CachedCaster || LockedTargets.Num() >= MaxLocks) return;

	AController* Controller = CachedCaster->GetController();
	if (!Controller) return;

	FVector ViewLoc;
	FRotator ViewRot;
	Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
	FVector TraceEnd = ViewLoc + (ViewRot.Vector() * LockTraceRange);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CachedCaster);

	for (const auto& LockedPtr : LockedTargets)
	{
		if (AGASPBaseCharacter* LockedChar = LockedPtr.Get())
		{
			Params.AddIgnoredActor(LockedChar);
		}
	}

	FHitResult Hit;
	float TargetingRadius = 75.f; 
	FCollisionShape Sphere = FCollisionShape::MakeSphere(TargetingRadius);

	bool bHit = CachedCaster->GetWorld()->SweepSingleByChannel(
		Hit, ViewLoc, TraceEnd, FQuat::Identity, ECC_Visibility, Sphere, Params
	);

	if (bHit)
	{
		AGASPBaseCharacter* TargetChar = Cast<AGASPBaseCharacter>(Hit.GetActor());

		if (TargetChar && !TargetChar->IsPlayerAlly())
		{
			LockedTargets.Add(TargetChar);
			CachedCaster->Client_SetTargetOutline(TargetChar, true, LockStencil);
		}
	}
}

void UMicroMissilesSpell::OnReleased_Implementation(AGASPBaseCharacter* Caster)
{
	if (!Caster || !Caster->HasAuthority()) return;

	// Ensure the spell actually successfully started ===
	if (!CachedCaster) return;

	Caster->GetWorldTimerManager().ClearTimer(ScanTimer);

	// DUMBFIRE MODE
	if (LockedTargets.Num() == 0)
	{
		for (int32 i = 0; i < MaxLocks; i++)
		{
			FTimerHandle LaunchDelayHandle;
			float Delay = FMath::Max(LaunchStaggerDelay * i, 0.01f); 
			
			FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &UMicroMissilesSpell::LaunchMissile, Caster, (AGASPBaseCharacter*)nullptr, i);
			Caster->GetWorldTimerManager().SetTimer(LaunchDelayHandle, Del, Delay, false);
		}
		CachedCaster = nullptr; // Reset for next cast
		return;
	}

	for (int32 i = 0; i < MaxLocks; i++)
	{
		int32 TargetIndex = i % LockedTargets.Num();
		
		if (AGASPBaseCharacter* Target = LockedTargets[TargetIndex].Get())
		{
			FTimerHandle LaunchDelayHandle;
			float Delay = FMath::Max(LaunchStaggerDelay * i, 0.01f); 
			
			FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &UMicroMissilesSpell::LaunchMissile, Caster, Target, i);
			Caster->GetWorldTimerManager().SetTimer(LaunchDelayHandle, Del, Delay, false);
		}
	}

	LockedTargets.Empty();
	CachedCaster = nullptr;
}

void UMicroMissilesSpell::LaunchMissile(AGASPBaseCharacter* Caster, AGASPBaseCharacter* Target, int32 Index)
{
	if (!IsValid(Caster)) return;

	if (Target)
	{
		Caster->Client_SetTargetOutline(Target, false, 0);
	}

	UClass* ClassToSpawn = MissileClass ? MissileClass.Get() : AMicroMissileProjectile::StaticClass();
	
	float HorizontalOffset = (Index % 2 == 0) ? -25.f : 25.f;

	FVector SpawnLoc = Caster->GetActorLocation() + FVector(0.f, 0.f, 70.f) + (Caster->GetActorForwardVector() * 50.f) + (Caster->GetActorRightVector() * HorizontalOffset);
	FTransform SpawnTransform(Caster->GetActorRotation(), SpawnLoc);

	AMicroMissileProjectile* Missile = Caster->GetWorld()->SpawnActorDeferred<AMicroMissileProjectile>(
		ClassToSpawn, SpawnTransform);

	if (Missile)
	{
		Missile->Initialize(Caster, Target);
		Missile->FinishSpawning(SpawnTransform);
	}
}