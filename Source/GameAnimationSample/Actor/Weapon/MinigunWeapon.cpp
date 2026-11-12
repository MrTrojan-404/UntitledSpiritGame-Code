#include "MinigunWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Kismet/GameplayStatics.h"
#include "GameAnimationSample/Actor/Weapon/Projectile/Projectile.h"

AMinigunWeapon::AMinigunWeapon()
{
	bAutomatic = true;
	FireDelay = 0.08f; 
}

void AMinigunWeapon::Fire(const FVector& HitTarget)
{
	// 1. Call the base AWeapon::Fire to trigger the FireSound, Casing ejection, 
	// FireAnimation, and SpendRound logic[cite: 1].
	// (We skip AProjectileWeapon::Fire so it doesn't spawn a duplicate bullet)
	AWeapon::Fire(HitTarget);

	// 2. Build the dynamic socket name (e.g., "MuzzleFlash_1")
	FString SocketNameString = BarrelSocketPrefix + FString::FromInt(CurrentBarrelIndex);
	FName MuzzleSocketName = FName(*SocketNameString);

	CurrentBarrelIndex++;
	if (CurrentBarrelIndex > TotalBarrels)
	{
		CurrentBarrelIndex = 1;
	}

	const USkeletalMeshSocket* BarrelSocket = GetItemMesh()->GetSocketByName(MuzzleSocketName);
	if (!BarrelSocket) return;

	FTransform SocketTransform = BarrelSocket->GetSocketTransform(GetItemMesh());

	// 3. Spawn the Muzzle Flash Particle exactly at the active barrel
	// (Since AWeapon::Fire strictly looks for "MuzzleFlash", we must spawn our custom ones manually[cite: 1])
	if (MuzzleFlash && GetWorld())
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, SocketTransform);
	}

	// 4. Spawn the Projectile
	APawn* InstigatorPawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (InstigatorPawn && World)
	{
		FVector ToTarget = HitTarget - SocketTransform.GetLocation();
		FRotator TargetRotation = ToTarget.Rotation();

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.Instigator = InstigatorPawn;

		if (InstigatorPawn->HasAuthority())
		{
			AProjectile* SpawnedProjectile = World->SpawnActor<AProjectile>(
				ProjectileClass, 
				SocketTransform.GetLocation(), 
				TargetRotation, 
				SpawnParams
			);

			if (SpawnedProjectile)
			{
				SpawnedProjectile->Damage = Damage;
				SpawnedProjectile->HeadShotDamage = HeadShotDamage;
				SpawnedProjectile->bUseServerSideRewind = false;
			}
			// Ensure the projectile inherits Stasis immediately to prevent frame-1 jumps!
			if (SpawnedProjectile)
			{
				if (bIsWeaponInStasis) 
				{
					// The Caster is shooting from inside the bubble
					SpawnedProjectile->SetStasis(true, StasisDilation);
				}
				else if (InstigatorPawn && InstigatorPawn->CustomTimeDilation < 1.0f)
				{
					// An Enemy is shooting from inside the bubble
					SpawnedProjectile->SetStasis(true, InstigatorPawn->CustomTimeDilation);
				}
			}
		} 
	}
}