#include "ChargeLaserWeapon.h"

#include "Components/AudioComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Perception/AISense_Hearing.h"

AChargeLaserWeapon::AChargeLaserWeapon()
{
	bIsChargeWeapon = true; 
	bAutomatic = false;
}

void AChargeLaserWeapon::StartCharging()
{
	const USkeletalMeshSocket* MuzzleSocket = GetItemMesh()->GetSocketByName("MuzzleFlash");
	if (!MuzzleSocket) return;

	if (ChargeVFX)
	{
		ActiveChargeVFX = UGameplayStatics::SpawnEmitterAttached(ChargeVFX, GetItemMesh(), "MuzzleFlash", FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget);
	}
	
	if (ChargeSound)
	{
		ActiveChargeSound = UGameplayStatics::SpawnSoundAttached(ChargeSound, GetItemMesh(), "MuzzleFlash");
	}
}

void AChargeLaserWeapon::CancelCharging()
{
	// 1. Stop the looping charge effects
	if (ActiveChargeVFX)
	{
		ActiveChargeVFX->Deactivate();
		ActiveChargeVFX = nullptr;
	}
	if (ActiveChargeSound)
	{
		ActiveChargeSound->Stop();
		ActiveChargeSound = nullptr;
	}

	// 2. Play the quick fizzle/dismiss burst
	if (DismissVFX)
	{
		UGameplayStatics::SpawnEmitterAttached(DismissVFX, GetItemMesh(), "MuzzleFlash", FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget);
	}
	if (DismissSound)
	{
		UGameplayStatics::SpawnSoundAttached(DismissSound, GetItemMesh(), "MuzzleFlash");
	}
}

void AChargeLaserWeapon::Fire(const FVector& HitTarget)
{
	//Shut off the charging effects so they don't linger while the laser is firing!
	if (ActiveChargeVFX)
	{
		ActiveChargeVFX->Deactivate();
		ActiveChargeVFX = nullptr;
	}
	if (ActiveChargeSound)
	{
		ActiveChargeSound->Stop();
		ActiveChargeSound = nullptr;
	}

	AWeapon::Fire(HitTarget);

	const USkeletalMeshSocket* MuzzleSocket = GetItemMesh()->GetSocketByName("MuzzleFlash");
	if (!MuzzleSocket) return;

	FVector Start = MuzzleSocket->GetSocketTransform(GetItemMesh()).GetLocation();
	FVector Direction = (HitTarget - Start).GetSafeNormal();

	TArray<AActor*> DamagedActors; 
	ExecuteBeamSegment(Start, Direction, LaserRange, 1, DamagedActors); 
}

void AChargeLaserWeapon::ExecuteBeamSegment(const FVector& Start, const FVector& Direction, float Range, int32 BouncesRemaining, TArray<AActor*>& AlreadyHitActors)
{
	FVector SegmentEnd = Start + (Direction * Range);
	bool bHitWall = false;
	FVector HitNormal = FVector::ZeroVector;

	// FIX: Safely get the controller from the Owner, not the Instigator!
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* InstigatorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	while (true)
	{
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(GetOwner());
		Params.AddIgnoredActor(this);
		Params.AddIgnoredActors(AlreadyHitActors);

		bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, SegmentEnd, ECC_Visibility, Params);

		if (!bHit)
		{
			break; 
		}

		AActor* HitActor = Hit.GetActor();
		
		bool bShouldPenetrate = false;
		if (HitActor)
		{
			if (HitActor->IsA(AGASPBaseCharacter::StaticClass())) bShouldPenetrate = true;
			if (HitActor->IsA(AItem::StaticClass())) bShouldPenetrate = true;
		}

		if (bShouldPenetrate)
		{
			// FIX: Now we use the InstigatorController we grabbed at the top of the function
			if (HitActor->IsA(AGASPBaseCharacter::StaticClass()) && HasAuthority() && InstigatorController)
			{
				const bool bHeadshot = Hit.BoneName == FName("head");
				const float DamageToCause = bHeadshot ? HeadShotDamage : Damage;
				
				UGameplayStatics::ApplyPointDamage(HitActor, DamageToCause, Direction, Hit, InstigatorController, this, UDamageType::StaticClass());
				
				// Optional: Trigger your custom hit markers for the UI!
				if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(InstigatorController))
				{
					PC->Client_ShowHitMarker(bHeadshot);
				}
			}

			AlreadyHitActors.Add(HitActor);
		}
		else 
		{
			SegmentEnd = Hit.ImpactPoint;
			bHitWall = true;
			HitNormal = Hit.ImpactNormal;

			if (ImpactParticles)
			{
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
			}
			if (HitSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, HitSound, Hit.ImpactPoint);
			}
			if (HasAuthority() && OwnerPawn)
			{
				// FIX: Use OwnerPawn for the noise Instigator
				UAISense_Hearing::ReportNoiseEvent(GetWorld(), Hit.ImpactPoint, 1.f, OwnerPawn, 800.f, FName("BulletImpact"));
			}

			break; 
		}
	}

	if (BeamParticles)
	{
		UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BeamParticles, Start, FRotator::ZeroRotator, true);
		if (Beam) Beam->SetVectorParameter(FName("Target"), SegmentEnd);
	}

	if (bHitWall && BouncesRemaining > 0)
	{
		float DistanceTraveled = (SegmentEnd - Start).Size();
		float RemainingRange = Range - DistanceTraveled;

		if (RemainingRange > 0.f)
		{
			FVector ReflectedDir = FMath::GetReflectionVector(Direction, HitNormal);
			FVector BounceStart = SegmentEnd + (HitNormal * 5.0f);
			ExecuteBeamSegment(BounceStart, ReflectedDir, RemainingRange, BouncesRemaining - 1, AlreadyHitActors);
		}
	}
}