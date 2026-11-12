// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileBullet.h"

#include "NiagaraFunctionLibrary.h"
#include "Components/BoxComponent.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/Component/LagCompenstionComponent.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"


AProjectileBullet::AProjectileBullet()
{
	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	ProjectileMovementComponent->SetIsReplicated(true);
	ProjectileMovementComponent->InitialSpeed = InitialSpeed;
	ProjectileMovementComponent->MaxSpeed = InitialSpeed;

	// Enable physics material trace for the inherited CollisionBox
	if (CollisionBox)
	{
		CollisionBox->bReturnMaterialOnMove = true;
	}
}

#if WITH_EDITOR
void AProjectileBullet::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);

	FName PropertyName = Event.Property != nullptr ? Event.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AProjectileBullet, InitialSpeed))
	{
		if (ProjectileMovementComponent)
		{
			ProjectileMovementComponent->InitialSpeed = InitialSpeed;
			ProjectileMovementComponent->MaxSpeed = InitialSpeed;
		}
	}
}
#endif

void AProjectileBullet::BeginPlay()
{
	Super::BeginPlay();
	SpawnTrailSystem(); // also spawn Niagara trail if TrailSystem is set in BP
}

void AProjectileBullet::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	AGASPBaseCharacter* OwnerCharacter = Cast<AGASPBaseCharacter>(GetOwner());
	if (OtherActor == GetOwner() || OtherActor == GetInstigator()) return;

	// Prevent base projectiles from detonating on each other
	if (OtherActor && OtherActor->IsA(AProjectile::StaticClass())) return;
	
	// 1. Determine the Surface Type
	EPhysicalSurface SurfaceType = SurfaceType_Default;
	if (Hit.PhysMaterial.IsValid()) 
	{
		SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
	}

	// 2. Resolve Assets (Niagara > Cascade > Audio > Decals)
	UNiagaraSystem* NiagaraToPlay = DefaultImpactNiagara;
	if (SurfaceImpactNiagara.Contains(SurfaceType)) NiagaraToPlay = SurfaceImpactNiagara[SurfaceType];

	UParticleSystem* ParticleToPlay = DefaultImpactParticle;
	if (SurfaceImpactParticles.Contains(SurfaceType)) ParticleToPlay = SurfaceImpactParticles[SurfaceType];

	USoundBase* SoundToPlay = DefaultImpactSound;
	if (SurfaceImpactSounds.Contains(SurfaceType)) SoundToPlay = SurfaceImpactSounds[SurfaceType];

	UMaterialInterface* DecalToPlay = DefaultImpactDecal;
	if (SurfaceImpactDecals.Contains(SurfaceType)) DecalToPlay = SurfaceImpactDecals[SurfaceType];


	// 3. Play VFX & Audio (Reversing the Shot Direction for Splatter Entry Wounds)
	FVector BulletDirection = GetActorForwardVector(); // Renamed to avoid C4456 conflict
	FRotator SplatterRotation = (BulletDirection * -1.f).Rotation();

	if (NiagaraToPlay)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), NiagaraToPlay, Hit.ImpactPoint, SplatterRotation);
	}
	else if (ParticleToPlay)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ParticleToPlay, Hit.ImpactPoint, SplatterRotation);
	}

	if (SoundToPlay)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SoundToPlay, Hit.ImpactPoint);
	}


	// 4. Spawn Decals (Environmental Pooling & Wall Splatters)
	if (DecalToPlay)
	{
		// Check if we hit a Character (Flesh)
		if (Hit.GetActor() && Hit.GetActor()->IsA(AGASPBaseCharacter::StaticClass())) 
		{
			// --- A. BLOOD POOL ON THE FLOOR ---
			float RandomOffsetX = FMath::RandRange(-60.f, 60.f);
			float RandomOffsetY = FMath::RandRange(-60.f, 60.f);
			FVector ScatterOffset = FVector(RandomOffsetX, RandomOffsetY, 0.f);

			FVector FloorTraceStart = Hit.ImpactPoint + ScatterOffset;
			FVector FloorTraceEnd = FloorTraceStart + (FVector::DownVector * 400.f); 

			FHitResult FloorHit;
			FCollisionQueryParams IgnoreParams;
			IgnoreParams.AddIgnoredActor(Hit.GetActor()); 

			if (GetWorld()->LineTraceSingleByChannel(FloorHit, FloorTraceStart, FloorTraceEnd, ECC_Visibility, IgnoreParams))
			{
				FRotator FloorDecalRotation = (-FloorHit.ImpactNormal).Rotation();
				FloorDecalRotation.Roll = FMath::RandRange(0.f, 360.f);

				UGameplayStatics::SpawnDecalAtLocation(
					GetWorld(), DecalToPlay, DecalSize, FloorHit.ImpactPoint, FloorDecalRotation, DecalLifeSpan
				);
			}

			// --- B. BLOOD SPLATTER ON THE WALL BEHIND ---
			// Continue tracing 3 meters forward along the bullet's path
			FVector WallTraceStart = Hit.ImpactPoint;
			FVector WallTraceEnd = WallTraceStart + (BulletDirection * 300.f);

			FHitResult WallHit;
			if (GetWorld()->LineTraceSingleByChannel(WallHit, WallTraceStart, WallTraceEnd, ECC_Visibility, IgnoreParams))
			{
				FRotator WallDecalRotation = (-WallHit.ImpactNormal).Rotation();
				WallDecalRotation.Roll = FMath::RandRange(0.f, 360.f);

				// Attach it so if the wall is a moving elevator/door, the blood moves with it
				UGameplayStatics::SpawnDecalAttached(
					DecalToPlay, 
					DecalSize * 1.5f, // Make the wall splatter slightly larger for dramatic effect!
					WallHit.GetComponent(), 
					NAME_None, 
					WallHit.ImpactPoint, 
					WallDecalRotation, 
					EAttachLocation::KeepWorldPosition, 
					DecalLifeSpan
				);
			}
		}
		else
		{
			// --- C. REGULAR BULLET HOLES ---
			// We hit a static environment object (Wall/Floor/Prop). 
			// Because the surface isn't "Flesh", DecalToPlay will automatically be your Concrete/Metal bullet hole!
			FRotator DecalRotation = (-Hit.ImpactNormal).Rotation();
			DecalRotation.Roll = FMath::RandRange(0.f, 360.f);

			UGameplayStatics::SpawnDecalAttached(
				DecalToPlay,
				DecalSize,
				Hit.GetComponent(),
				NAME_None, 
				Hit.ImpactPoint,
				DecalRotation,
				EAttachLocation::KeepWorldPosition,
				DecalLifeSpan
			);
		}
	}

	if (OwnerCharacter)
	{
		AController* OwnerController = OwnerCharacter->GetController();

		if (OwnerController)
		{
			AGASPBaseCharacter* HitCharacter = Cast<AGASPBaseCharacter>(OtherActor);

			if (OwnerCharacter->HasAuthority() && !bUseServerSideRewind)
			{
				const bool bHeadshot = Hit.BoneName == FName("head");
				const float DamageToCause = bHeadshot ? HeadShotDamage : Damage;
             
				FVector ShotDirection = GetActorForwardVector();
				UGameplayStatics::ApplyPointDamage(OtherActor,
					DamageToCause,
					ShotDirection,
					Hit,
					OwnerController,
					this,
					UDamageType::StaticClass());

				if (HitCharacter)
				{
					if (AGASPCorePlayerController* AttackerPC = Cast<AGASPCorePlayerController>(OwnerController))
					{
						AttackerPC->Client_ShowHitMarker(bHeadshot);
					}
				}
				
				// Replaced early return with a call to Super::OnHit so standard destruction happens
				Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
				return;
			}
			
			AGASPCorePlayerController* PlayerController = Cast<AGASPCorePlayerController>(OwnerController);
			
			if (bUseServerSideRewind && OwnerCharacter->GetLagCompensation() && OwnerCharacter->IsLocallyControlled() && HitCharacter && PlayerController)
			{
				OwnerCharacter->GetLagCompensation()->ProjectileServerScoreRequest(
					HitCharacter,
					TraceStart,
					InitialVelocity,
					PlayerController->GetServerTime() - PlayerController->SingleTripTime
				);
			}
		}
	}

	if (HasAuthority())
	{
		UAISense_Hearing::ReportNoiseEvent(
			GetWorld(),
			Hit.ImpactPoint,
			1.f,
			GetInstigator(), 
			800.f,
			FName("BulletImpact")
		);
	}

	Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
}