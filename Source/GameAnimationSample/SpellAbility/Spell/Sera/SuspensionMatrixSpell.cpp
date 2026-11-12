#include "SuspensionMatrixSpell.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "GameAnimationSample/SpellAbility/Projectile/SuspensionMatrixProjectile.h"
#include "Kismet/GameplayStatics.h"

void USuspensionMatrixSpell::Execute_Implementation(AGASPBaseCharacter* Caster)
{
	if (!Caster || !Caster->HasAuthority()) return;
    
	// 1. Calculate the Left Shoulder Spawn Location
	// Push it up 70 units (chest/shoulder height) and 40 units to the Left (-RightVector).
	FVector SpawnLoc = Caster->GetActorLocation() + FVector(0.f, 0.f, 70.f) - (Caster->GetActorRightVector() * 40.f);

	// 2. Get the exact Crosshair target from the Combat Component
	FVector TargetLocation = SpawnLoc + (Caster->GetActorForwardVector() * 3000.f); // Fallback
	if (UCombatComponent* Combat = Caster->GetCombatComponent())
	{
		TargetLocation = Combat->GetHitTarget();
	}

	// 3. Aim the projectile perfectly from the shoulder to the crosshair target
	FVector LaunchDirection = (TargetLocation - SpawnLoc).GetSafeNormal();
	FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLoc);

	// 4. Fire the Projectile
	UClass* ClassToSpawn = MatrixProjectileClass ? MatrixProjectileClass.Get() : ASuspensionMatrixProjectile::StaticClass();
	ASuspensionMatrixProjectile* Projectile = Caster->GetWorld()->SpawnActorDeferred<ASuspensionMatrixProjectile>(ClassToSpawn, SpawnTransform);

	if (Projectile)
	{
		Projectile->Initialize(Caster);
		Projectile->FinishSpawning(SpawnTransform);
        
		// Play the launch sound exactly when the projectile leaves the shoulder
		if (CastSound)
		{
			UGameplayStatics::PlaySoundAtLocation(Caster, CastSound, SpawnLoc);
		}
	}
}
