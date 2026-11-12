#include "LaserTurret.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "GameAnimationSample/AI/Boss/Boss1/GASPStageShield.h"
#include "Net/UnrealNetwork.h"

ALaserTurret::ALaserTurret()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	TurretMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TurretMesh"));
	RootComponent = TurretMesh;
	
	// 1. Revert to WorldDynamic so Projectiles hit it!
	TurretMesh->SetCollisionObjectType(ECC_WorldDynamic);
	TurretMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	
	// 2. Force it to block EVERYTHING (Hitscan Visibility, Flamethrower Sweeps, and Projectiles)
	TurretMesh->SetCollisionResponseToAllChannels(ECR_Block);
	
	// Optional: Ignore the camera so players don't get zoomed in when standing near it
	TurretMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// The anticipation laser
	TrackingBeamComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrackingBeamComp"));
	TrackingBeamComp->SetupAttachment(TurretMesh, FName("Muzzle"));
	TrackingBeamComp->bAutoActivate = false; 
}

void ALaserTurret::BeginPlay()
{
	Super::BeginPlay();
	
	if (HasAuthority())
	{
		Health = MaxHealth;
		IgnoredActors.Add(this);

		TArray<AActor*> FoundShields;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPStageShield::StaticClass(), FoundShields);
		for (AActor* Shield : FoundShields) IgnoredActors.Add(Shield);
	}
	// Look straight ahead by default
	ClampedTargetLocation = GetActorLocation() + (GetActorForwardVector() * 1000.f);
	TargetAimLocation = ClampedTargetLocation;
}

void ALaserTurret::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALaserTurret, Health);
	DOREPLIFETIME(ALaserTurret, CurrentTarget);
	DOREPLIFETIME(ALaserTurret, bIsActive);
	DOREPLIFETIME(ALaserTurret, bIsLocked);
	DOREPLIFETIME(ALaserTurret, bIsCoolingDown);
	DOREPLIFETIME(ALaserTurret, bIsDestroyed);
	DOREPLIFETIME(ALaserTurret, AvailableTargets);
}

void ALaserTurret::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector BaseLoc = GetActorLocation();
	FVector StartLoc = TurretMesh->GetSocketLocation(FName("Muzzle"));

	// IF DESTROYED: Drop the aim to the floor, but KEEP the twist-proof Matrix math!
	if (bIsDestroyed)
	{
		// 1. Find the direction the turret is currently facing, and aim straight down into the floor
		FVector CurrentAimDir = (ClampedTargetLocation - StartLoc).GetSafeNormal2D();
		FVector DroopWorldLoc = BaseLoc + (CurrentAimDir * 1000.f) - FVector(0.f, 0.f, 2000.f);

		// 2. Clamp the Pitch so it physically rests at exactly -40 degrees
		FVector LocalTarget = TurretMesh->GetComponentTransform().InverseTransformPosition(DroopWorldLoc);
		FRotator LocalRot = LocalTarget.Rotation();
		LocalRot.Pitch = FMath::Clamp(LocalRot.Pitch, -38.f, 40.f);
		FVector ClampedLocalTarget = LocalRot.Vector() * LocalTarget.Size();
		TargetAimLocation = TurretMesh->GetComponentTransform().TransformPosition(ClampedLocalTarget); 

		// 3. Smoothly drop the target location
		ClampedTargetLocation = FMath::VInterpTo(ClampedTargetLocation, TargetAimLocation, DeltaTime, 2.0f);

		// 4. Run our Gimbal-Lock-proof Matrix to guarantee ZERO barrel roll!
		FVector Forward = (ClampedTargetLocation - StartLoc).GetSafeNormal();
		FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
		FVector Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();

		FMatrix RotMatrix(-Right, Up, -Forward, FVector::ZeroVector);
		GunBoneRotation = RotMatrix.Rotator();
		
		return; // Abort the rest of the laser tracking code!
	}
	
	bool bIsCurrentlyInDeadzone = false;

	if (bIsActive && CurrentTarget)
	{
		float Dist2D = FVector::Dist2D(BaseLoc, CurrentTarget->GetActorLocation());
		if (Dist2D <= DeadzoneRadius)
		{
			bIsCurrentlyInDeadzone = true;
		}
	}

	// 1. CANCEL ATTACK IF IN DEADZONE
	// If the player just stepped into the deadzone, cancel the attack timers instantly!
	if (bIsCurrentlyInDeadzone && !bWasInDeadzone)
	{
		GetWorldTimerManager().ClearTimer(AttackTimer);
		bIsLocked = false;
		bIsCoolingDown = false;
	}
	// If the player just stepped OUT of the deadzone, restart the attack cycle!
	else if (!bIsCurrentlyInDeadzone && bWasInDeadzone && bIsActive)
	{
		ChargeLaser(); 
	}
	bWasInDeadzone = bIsCurrentlyInDeadzone;

	// 2. SET THE AIM LOCATION (Holding Position on Cooldown)
	if (bIsCurrentlyInDeadzone || !bIsActive || !CurrentTarget)
	{
		TargetAimLocation = BaseLoc + (GetActorForwardVector() * 1000.f);
	}
	else if (!bIsCoolingDown) //  Tracks through both Red and Green phases!
	{
		// TRACKING PHASE
		FVector PlayerVelocity = CurrentTarget->GetVelocity();
		PlayerVelocity.Z = 0.f; 

		FVector TargetWorldLoc = CurrentTarget->GetActorLocation() + (PlayerVelocity * TargetLeadTime);

		// Clamp Pitch (Local Space)
		FVector LocalTarget = TurretMesh->GetComponentTransform().InverseTransformPosition(TargetWorldLoc);
		FRotator LocalRot = LocalTarget.Rotation();
		LocalRot.Pitch = FMath::Clamp(LocalRot.Pitch, -40.f, 40.f);
		
		FVector ClampedLocalTarget = LocalRot.Vector() * LocalTarget.Size();
		TargetAimLocation = TurretMesh->GetComponentTransform().TransformPosition(ClampedLocalTarget); 
	}
	
	// NOTICE: If bIsCoolingDown is true, we do absolutely nothing. 
	// This forces TargetAimLocation to remain exactly where it was, holding its position!

	// 3. SNAPPY INTERPOLATION
	ClampedTargetLocation = FMath::VInterpTo(ClampedTargetLocation, TargetAimLocation, DeltaTime, TrackingSpeed);

	// ALWAYS CALCULATE THE MATRIX (Prevents looking at the dirt)
	FVector Forward = (ClampedTargetLocation - StartLoc).GetSafeNormal();
	FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	FVector Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();

	FMatrix RotMatrix(-Right, Up, -Forward, FVector::ZeroVector);
	GunBoneRotation = RotMatrix.Rotator();

	// HANDLE THE TRACKING BEAM
	if (bIsActive && CurrentTarget && !bIsCurrentlyInDeadzone && !bIsCoolingDown)
	{
		TrackingBeamComp->SetVisibility(true);
		if (!TrackingBeamComp->IsActive()) TrackingBeamComp->Activate(true);

		FVector FireDir = TurretMesh->GetSocketTransform(FName("Muzzle")).GetRotation().GetForwardVector();
		FVector EndLoc = StartLoc + (FireDir * LaserRange);

		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActors(IgnoredActors);
		if (GetWorld()->LineTraceSingleByChannel(Hit, StartLoc, EndLoc, ECC_Visibility, Params))
		{
			EndLoc = Hit.ImpactPoint;
		}

		TrackingBeamComp->SetVectorParameter(FName("BeamEnd"), EndLoc);

		FLinearColor BeamColor = FLinearColor::Red;
		if (bIsLocked)
		{
			float ElapsedLockTime = GetWorldTimerManager().GetTimerElapsed(AttackTimer);
			float Alpha = FMath::Clamp(ElapsedLockTime / LockTime, 0.f, 1.0f);
			BeamColor = FMath::Lerp(FLinearColor::Red, FLinearColor::Green, Alpha);
		}
		TrackingBeamComp->SetColorParameter(FName("BeamColor"), BeamColor);
	}
	else
	{
		TrackingBeamComp->SetVisibility(false);
	}
}

void ALaserTurret::ActivateTurret(TArray<AGASPBaseCharacter*> Targets)
{
	if (!HasAuthority() || bIsDestroyed) return;

	AvailableTargets = Targets;
	bIsActive = true;
	
	// Start thinking every 0.5 seconds!
	GetWorldTimerManager().SetTimer(TargetEvalTimer, this, &ALaserTurret::EvaluateTarget, 0.5f, true);
	EvaluateTarget(); 
}

void ALaserTurret::ChargeLaser()
{
	if (bIsDestroyed) return;
	
	bIsCoolingDown = false;
	bIsLocked = false; 
	GetWorldTimerManager().SetTimer(AttackTimer, this, &ALaserTurret::LockLaser, ChargeTime, false);
}

void ALaserTurret::LockLaser()
{
	if (bIsDestroyed) return;

	bIsLocked = true; 
	GetWorldTimerManager().SetTimer(AttackTimer, this, &ALaserTurret::FireLaser, LockTime, false);
}

void ALaserTurret::FireLaser()
{
	if (bIsDestroyed) return;

	bIsCoolingDown = true; // Starts cooldown
	TrackingBeamComp->SetVisibility(false); // Instantly hide the tracking beam!

	FVector StartLoc = TurretMesh->GetSocketLocation(FName("Muzzle"));
	
	// Shoot exactly down the Socket's X-Axis
	FVector FireDir = TurretMesh->GetSocketTransform(FName("Muzzle")).GetRotation().GetForwardVector();
	FVector EndLoc = StartLoc + (FireDir * LaserRange);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActors(IgnoredActors);

	if (GetWorld()->LineTraceSingleByChannel(Hit, StartLoc, EndLoc, ECC_Visibility, Params))
	{
		EndLoc = Hit.ImpactPoint;
		if (AActor* HitActor = Hit.GetActor())
		{
			UGameplayStatics::ApplyDamage(HitActor, LaserDamage, nullptr, this, UDamageType::StaticClass());
		}
	}

	Multicast_PlayFireVFX(EndLoc);
	GetWorldTimerManager().SetTimer(AttackTimer, this, &ALaserTurret::ChargeLaser, CooldownTime, false);
	
}

void ALaserTurret::Multicast_PlayFireVFX_Implementation(FVector EndPoint)
{
	FVector StartLoc = TurretMesh->GetSocketLocation(FName("Muzzle"));

	if (FireBeamVFX)
	{
		UNiagaraComponent* Beam = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), FireBeamVFX, StartLoc);
		if (Beam) Beam->SetVectorParameter(FName("BeamEnd"), EndPoint);
	}
	if (FireSound) UGameplayStatics::PlaySoundAtLocation(this, FireSound, StartLoc);
}

float ALaserTurret::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || bIsDestroyed) return 0.f;

	Health -= DamageAmount;

	if (Health <= 0.f)
	{
		bIsDestroyed = true;
		GetWorldTimerManager().ClearAllTimersForObject(this);
		Multicast_DestroyTurret();
	}
	return DamageAmount;
}

void ALaserTurret::Multicast_DestroyTurret_Implementation()
{
	if (DestroySound) UGameplayStatics::PlaySoundAtLocation(this, DestroySound, GetActorLocation());

	// 1. PLAY THE EXPLOSION (Facing Upwards)
	if (ExplosionVFX) 
	{
		// Pitching 90 degrees forces the X-Axis of the Niagara system to point straight up into the sky!
		FRotator UpwardRotation = FRotator(90.f, 0.f, 0.f); 
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ExplosionVFX, GetActorLocation(), UpwardRotation);
	}

	// 2. PLAY THE ELECTROCUTION (Attached to the Mesh)
	if (ElectrocutionVFX && TurretMesh)
	{
		ActiveElectrocuteVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
			ElectrocutionVFX, 
			TurretMesh, 
			NAME_None, 
			FVector::ZeroVector, 
			FRotator::ZeroRotator, 
			EAttachLocation::SnapToTarget, 
			true
		);
	}

	// 3. SET THE 6-SECOND TIMER TO KILL THE SPARKS
	GetWorldTimerManager().SetTimer(ElectrocutionTimer, this, &ALaserTurret::StopElectrocution, 6.0f, false);

	// 4. OBLITERATE THE TRACKING BEAM
	if (TrackingBeamComp) 
	{
		TrackingBeamComp->SetVisibility(false);
		TrackingBeamComp->DestroyComponent();
	}
	
	// Turn off collision so players can walk through the wreckage, but leave it visible!
	//TurretMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ALaserTurret::StopElectrocution()
{
	if (ActiveElectrocuteVFX)
	{
		ActiveElectrocuteVFX->Deactivate();
		ActiveElectrocuteVFX = nullptr;
	}
}

void ALaserTurret::EvaluateTarget()
{
	if (bIsDestroyed || !bIsActive) return;

	AGASPBaseCharacter* BestTarget = nullptr;
	float ClosestDist = LaserRange; 

	// 1. Loop through everyone and find the absolute closest alive player
	for (AGASPBaseCharacter* PotentialTarget : AvailableTargets)
	{
		if (PotentialTarget && PotentialTarget->Health > 0.f)
		{
			float Dist = FVector::Dist(GetActorLocation(), PotentialTarget->GetActorLocation());
			if (Dist < ClosestDist)
			{
				ClosestDist = Dist;
				BestTarget = PotentialTarget;
			}
		}
	}

	// 2. Assign the target
	if (BestTarget)
	{
		// If we found a NEW closest target, switch to them immediately!
		if (CurrentTarget != BestTarget)
		{
			CurrentTarget = BestTarget;
			
			// If the turret was resting, wake it up and start the attack cycle
			if (!bIsLocked && !bIsCoolingDown && !GetWorldTimerManager().IsTimerActive(AttackTimer))
			{
				ChargeLaser(); 
			}
		}
	}
	else
	{
		// 3. No one is alive or in range. Shut down.
		if (CurrentTarget)
		{
			CurrentTarget = nullptr;
			GetWorldTimerManager().ClearTimer(AttackTimer); 
			bIsLocked = false;
			bIsCoolingDown = false;
		}
	}
}