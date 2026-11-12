#include "HydroCageSpell.h"

#include "GameAnimationSample/Actor/EnvironmentInteractable/Explosives/ExplosiveBarrel.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/SpellAbility/SpellComponent.h"
#include "GameAnimationSample/SpellAbility/Effect/HydroCageEffect.h"
#include "Kismet/GameplayStatics.h"

void UHydroCageSpell::Execute_Implementation(AGASPBaseCharacter* Caster)
{
	if (!Caster || !Caster->HasAuthority()) return;

	// Helper lambda to clean up the code
	auto RefundEnergy = [&]()
	{
		if (USpellComponent* SpellComp = Caster->FindComponentByClass<USpellComponent>())
		{
			SpellComp->RefundEnergySegments(EnergySegmentCost);
		}
	};

	AController* Controller = Caster->GetController();
	if (!Controller) { RefundEnergy(); return; }

	FVector ViewLoc;
	FRotator ViewRot;
	Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
	FVector TraceEnd = ViewLoc + (ViewRot.Vector() * TraceRange);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Caster);

	FHitResult Hit;
	// Refund if we hit absolutely nothing
	if (!Caster->GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc, TraceEnd, ECC_Visibility, Params))
	{
		RefundEnergy();
		return; 
	}

	AActor* HitActor = Hit.GetActor();
	AGASPBaseCharacter* TargetChar = Cast<AGASPBaseCharacter>(HitActor);
	AExplosiveBarrel* TargetBarrel = Cast<AExplosiveBarrel>(HitActor);

	// Abort and refund if it's neither a valid enemy nor a barrel
	if (!TargetChar && !TargetBarrel) { RefundEnergy(); return; }
	if (TargetChar && TargetChar->IsPlayerAlly()) { RefundEnergy(); return; } 
	if (TargetChar && !TargetChar->CanBeCrowdControlled()) { RefundEnergy(); return; } 
	
	UClass* ClassToSpawn = EffectClass ? EffectClass.Get() : AHydroCageEffect::StaticClass();
	FTransform SpawnTransform(FRotator::ZeroRotator, HitActor->GetActorLocation());

	AHydroCageEffect* Effect = Caster->GetWorld()->SpawnActorDeferred<AHydroCageEffect>(ClassToSpawn, SpawnTransform);

	if (Effect)
	{
		Effect->Initialize(Caster, HitActor, CageDuration, DamagePerTick, TickInterval, PullSpeed);
		Effect->FinishSpawning(SpawnTransform);

		if (CastSound)
		{
			UGameplayStatics::PlaySoundAtLocation(Caster, CastSound, Caster->GetActorLocation());
		}
	}
}