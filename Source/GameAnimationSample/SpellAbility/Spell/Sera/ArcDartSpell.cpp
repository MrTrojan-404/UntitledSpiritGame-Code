#include "ArcDartSpell.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "GameAnimationSample/SpellAbility/Projectile/ArcDartProjectile.h"
#include "Kismet/GameplayStatics.h"

void UArcDartSpell::Execute_Implementation(AGASPBaseCharacter* Caster)
{
	if (!Caster || !Caster->HasAuthority()) return;
    
	// Calculate Right Shoulder/Wrist Spawn Location
	FVector SpawnLoc = Caster->GetActorLocation() + FVector(0.f, 0.f, 70.f) + (Caster->GetActorRightVector() * 30.f);

	// Get the exact Crosshair target from the Combat Component
	FVector TargetLocation = SpawnLoc + (Caster->GetActorForwardVector() * 5000.f); // Fallback
	if (UCombatComponent* Combat = Caster->GetCombatComponent())
	{
		TargetLocation = Combat->GetHitTarget();
	}

	// Aim the projectile perfectly from the wrist to the crosshair target
	FVector LaunchDirection = (TargetLocation - SpawnLoc).GetSafeNormal();
	FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLoc);

	UClass* ClassToSpawn = DartProjectileClass ? DartProjectileClass.Get() : AArcDartProjectile::StaticClass();
	AArcDartProjectile* Dart = Caster->GetWorld()->SpawnActorDeferred<AArcDartProjectile>(ClassToSpawn, SpawnTransform);

	if (Dart)
	{
		Dart->Initialize(Caster);
		Dart->FinishSpawning(SpawnTransform);
	}
}