#include "IgnitionSpell.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/SpellAbility/SpellComponent.h"
#include "GameAnimationSample/SpellAbility/Effect/IgnitionEffect.h"

void UIgnitionSpell::Execute_Implementation(AGASPBaseCharacter* Caster)
{
	if (!Caster || !Caster->HasAuthority()) return;

	CachedCaster = Caster;
	
	// Reset any hanging target
	if (CachedTarget.IsValid())
	{
		Caster->Client_SetTargetOutline(CachedTarget.Get(), false, 0);
		CachedTarget.Reset();
	}

	// Do an immediate scan the moment the button is pressed
	ScanForTarget();

	// Start the looping scan while the button is held down
	Caster->GetWorldTimerManager().SetTimer(ScanTimer, this, &UIgnitionSpell::ScanForTarget, ScanInterval, true);
}

void UIgnitionSpell::ScanForTarget()
{
	if (!CachedCaster) return;

	AController* Controller = CachedCaster->GetController();
	if (!Controller) return;

	FVector ViewLoc;
	FRotator ViewRot;
	Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
	
	FVector TraceEnd = ViewLoc + (ViewRot.Vector() * TraceRange);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CachedCaster);

	FHitResult Hit;
	float TargetingRadius = 50.f;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(TargetingRadius);

	bool bHit = CachedCaster->GetWorld()->SweepSingleByChannel(
		Hit, 
		ViewLoc, 
		TraceEnd, 
		FQuat::Identity, 
		ECC_Visibility, 
		Sphere, 
		Params
	);

	AGASPBaseCharacter* TargetChar = bHit ? Cast<AGASPBaseCharacter>(Hit.GetActor()) : nullptr;

	// 1. If we are looking at a valid enemy
	if (TargetChar && !TargetChar->IsPlayerAlly())
	{
		// Is it a DIFFERENT enemy than the one we currently have marked?
		if (CachedTarget.Get() != TargetChar)
		{
			if (CachedTarget.IsValid())
			{
				CachedCaster->Client_SetTargetOutline(CachedTarget.Get(), false, 0);
			}
			CachedTarget = TargetChar;
			CachedCaster->Client_SetTargetOutline(TargetChar, true, IgnitionStencil);
		}
	}
	// 2. If we are looking at nothing, a wall, or an ally
	else
	{
		// Turn off the outline and clear the cache so releasing the button does nothing
		if (CachedTarget.IsValid())
		{
			CachedCaster->Client_SetTargetOutline(CachedTarget.Get(), false, 0);
			CachedTarget.Reset();
		}
	}
}

void UIgnitionSpell::OnReleased_Implementation(AGASPBaseCharacter* Caster)
{
	if (!Caster || !Caster->HasAuthority()) return;

	if (!CachedCaster) return;
	
	// Stop scanning
	Caster->GetWorldTimerManager().ClearTimer(ScanTimer);

	// If we have a valid enemy marked when the button is released
	if (CachedTarget.IsValid())
	{
		AGASPBaseCharacter* TargetChar = CachedTarget.Get();
		// Remove the outline
		Caster->Client_SetTargetOutline(TargetChar, false, 0);

		if (IsValid(TargetChar))
		{
			UClass* ClassToSpawn = IgnitionEffectClass ? IgnitionEffectClass.Get() : AIgnitionEffect::StaticClass();
			FTransform SpawnTransform(FRotator::ZeroRotator, TargetChar->GetActorLocation());

			AIgnitionEffect* Effect = Caster->GetWorld()->SpawnActorDeferred<AIgnitionEffect>(ClassToSpawn, SpawnTransform);

			if (Effect)
			{
				Effect->Initialize(Caster, TargetChar, BaseDamagePerTick, TickInterval, ExplosionDamage, ExplosionRadius);
				Effect->FinishSpawning(SpawnTransform);
			}
		}
	}
	else
	{
		// ==========================================
		// REFUND ENERGY IF NO TARGET WAS ACQUIRED
		// ==========================================
		if (USpellComponent* SpellComp = Caster->FindComponentByClass<USpellComponent>())
		{
			SpellComp->RefundEnergySegments(EnergySegmentCost);
		}
	}

	// Clean up
	CachedTarget.Reset();
	CachedCaster = nullptr;
}