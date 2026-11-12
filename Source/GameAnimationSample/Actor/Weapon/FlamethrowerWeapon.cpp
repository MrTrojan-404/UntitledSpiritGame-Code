#include "FlamethrowerWeapon.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Kismet/GameplayStatics.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"

AFlamethrowerWeapon::AFlamethrowerWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	
	// A Flamethrower shoots extremely fast! 
	FireDelay = 0.1f; // Ticks damage 10 times a second
	bAutomatic = true;
}

void AFlamethrowerWeapon::BeginPlay()
{
	Super::BeginPlay();

	if (FlameStreamVFX)
	{
		// Notice the TWO 'false' parameters at the end
		// 1st false = bAutoDestroy
		// 2nd false = bAutoActivate (Spawns completely asleep)
		ActiveFlameVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
			FlameStreamVFX, GetItemMesh(), FName("MuzzleFlash"), 
			FVector::ZeroVector, FRotator::ZeroRotator, 
			EAttachLocation::SnapToTarget, false, false 
		);
	}
}

void AFlamethrowerWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// The Timeout Check: If we haven't fired in slightly longer than our FireDelay, 
	// it means the Boss let go of the trigger. Shut down the VFX!
	if (bIsSpraying)
	{
		TimeSinceLastFired += DeltaTime;
		
		if (TimeSinceLastFired > (FireDelay + 0.05f))
		{
			StopSpraying();
		}
	}
}

void AFlamethrowerWeapon::Fire(const FVector& HitTarget)
{
	// 1. Reset the timeout so the VFX stays alive
	TimeSinceLastFired = 0.f;
	if (!bIsSpraying) StartSpraying();

	// 2. Consume Ammo
	SpendRound();

	// 3. The Damage Sweep!
	if (const USkeletalMeshSocket* MuzzleSocket = GetItemMesh()->GetSocketByName(FName("MuzzleFlash")))
	{
		FTransform SocketTransform = MuzzleSocket->GetSocketTransform(GetItemMesh());
		FVector Start = SocketTransform.GetLocation();
		
		FVector Direction = (HitTarget - Start).GetSafeNormal();
		FVector End = Start + (Direction * FlameRange);

		TArray<FHitResult> HitResults;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		QueryParams.AddIgnoredActor(GetOwner());

		// Sweep against Visibility. Walls and Shields will Block. Players will Overlap.
		bool bHit = GetWorld()->SweepMultiByChannel(
			HitResults, 
			Start, 
			End, 
			FQuat::Identity, 
			ECC_Visibility,
			FCollisionShape::MakeSphere(FlameRadius), 
			QueryParams
		);

		float ActualRange = FlameRange;

		if (bHit && HasAuthority())
		{
			TSet<AActor*> DamagedActors; 
			
			for (const FHitResult& Hit : HitResults)
			{
				AActor* HitActor = Hit.GetActor();
				if (!HitActor || DamagedActors.Contains(HitActor)) continue;

				// DID WE HIT A WALL OR A SHIELD?
				if (Hit.bBlockingHit)
				{
					// Cut the flame's range off at this exact distance!
					ActualRange = Hit.Distance;
					DamagedActors.Add(HitActor);

					// Apply POINT damage. This passes the exact Hit Location and Normal to the Shield, 
					// fixing the bug where the ripples spawn at the center of the sphere!
					UGameplayStatics::ApplyPointDamage(HitActor, FlameTickDamage, Direction, Hit, ShooterOwnerController, this, UDamageType::StaticClass());
					
					// Break the loop! The fire cannot pass through this blocking object.
					break; 
				}
				// WE OVERLAPPED A PLAYER OR ENEMY
				else
				{
					DamagedActors.Add(HitActor);
					UGameplayStatics::ApplyDamage(HitActor, FlameTickDamage, ShooterOwnerController, this, UDamageType::StaticClass());
				}
			}
		}

		// --- UPDATE THE VISUALS ---
		// Tell the Niagara system exactly how far it is allowed to travel before hitting a wall!
		if (ActiveFlameVFX)
		{
			ActiveFlameVFX->SetFloatParameter(FName("FlameLength"), ActualRange);
		}

		// --- THE DEBUG DRAW ---
		if (bShowDebugHitbox)
		{
			FVector ActualEnd = Start + (Direction * ActualRange);
			DrawDebugCylinder(GetWorld(), Start, ActualEnd, FlameRadius, 12, FColor::Orange, false, 0.15f, 0, 2.f);
			DrawDebugSphere(GetWorld(), ActualEnd, FlameRadius, 12, FColor::Red, false, 0.15f, 0, 2.f);
		}
	}
}

void AFlamethrowerWeapon::StartSpraying()
{
	bIsSpraying = true;
	
	if (ActiveFlameVFX)
	{
		ActiveFlameVFX->Activate(true);
	}

	// 1. Play the explosive "Whoosh" start sound!
	if (FlameStartSound)
	{
		UGameplayStatics::SpawnSoundAttached(FlameStartSound, GetItemMesh(), FName("MuzzleFlash"));
	}

	// 2. Start the continuous loop
	if (FlameLoopSound)
	{
		ActiveFlameAudio = UGameplayStatics::SpawnSoundAttached(FlameLoopSound, GetItemMesh(), FName("MuzzleFlash"));
	}
}

void AFlamethrowerWeapon::StopSpraying()
{
	bIsSpraying = false;
	
	if (ActiveFlameVFX)
	{
		ActiveFlameVFX->Deactivate();
	}
	
	// 3. Kill the loop immediately
	if (ActiveFlameAudio)
	{
		ActiveFlameAudio->Stop();
		ActiveFlameAudio = nullptr; 
	}

	// 4. Play the sizzle/wind-down sound!
	if (FlameEndSound)
	{
		UGameplayStatics::SpawnSoundAttached(FlameEndSound, GetItemMesh(), FName("MuzzleFlash"));
	}
}