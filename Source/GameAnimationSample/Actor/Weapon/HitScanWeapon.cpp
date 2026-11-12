// Fill out your copyright notice in the Description page of Project Settings.


#include "HitScanWeapon.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/Component/LagCompenstionComponent.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "GameAnimationSample/SpellAbility/SpellComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Projectile/BulletTrail/BulletTrailActor.h"
#include "Sound/SoundBase.h"


void AHitScanWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget); // Base handles flash, sound, and stealth noise!
 
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr) return;
	AController* InstigatorController = OwnerPawn->GetController();
 
	const USkeletalMeshSocket* MuzzleFlashSocket = GetItemMesh()->GetSocketByName("MuzzleFlash");
	if (!MuzzleFlashSocket) return;
 
	FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetItemMesh());
	FVector Start = SocketTransform.GetLocation();
 
	FHitResult FireHit;
	ABulletTrailActor* SpawnedTrail = nullptr;
	// This instantly calculates the hit and spawns your ABulletTrailActor or BeamParticle!
	WeaponTraceHit(Start, HitTarget, FireHit, &SpawnedTrail);
	
	UE_LOG(LogTemp, Warning, TEXT("Fire: SpawnedTrail = %s"), SpawnedTrail ? TEXT("valid") : TEXT("NULL"));

	// --- STASIS BUFFERING LOGIC ---
	if (bIsWeaponInStasis)
	{
		FBufferedShot NewShot;
		NewShot.HitResult = FireHit;
		NewShot.TraceStart = Start;
		NewShot.InstigatorController = InstigatorController;
		NewShot.TrailActor = SpawnedTrail;
		if (USkeletalMeshComponent* SkelMesh = Cast<USkeletalMeshComponent>(FireHit.GetComponent()))
		{
			const int32 BoneIndex = SkelMesh->GetBoneIndex(FireHit.BoneName);
			if (BoneIndex != INDEX_NONE)
			{
				NewShot.HitMeshComp = SkelMesh;
				NewShot.HitBoneName = FireHit.BoneName;
				NewShot.LocalHitOffset = SkelMesh->GetBoneTransform(BoneIndex).InverseTransformPosition(FireHit.ImpactPoint);
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("Buffering shot, TrailActor IsValid = %s"), NewShot.TrailActor.IsValid() ? TEXT("true") : TEXT("false"));

		BufferedShots.Add(NewShot);
	}
	else
	{
		ProcessShotImpact(FireHit, Start, InstigatorController);
	}
}

// Loop through every saved shot and fire them off at once!
void AHitScanWeapon::ReleaseBufferedShots()
{
	for (const FBufferedShot& Shot : BufferedShots)
	{
		FHitResult AdjustedHit = Shot.HitResult;

		if (USkeletalMeshComponent* SkelMesh = Shot.HitMeshComp.Get())
		{
			const int32 BoneIndex = SkelMesh->GetBoneIndex(Shot.HitBoneName);
			if (BoneIndex != INDEX_NONE)
			{
				const FTransform CurrentBoneXform = SkelMesh->GetBoneTransform(BoneIndex);
				AdjustedHit.ImpactPoint = CurrentBoneXform.TransformPosition(Shot.LocalHitOffset);
				AdjustedHit.Location = AdjustedHit.ImpactPoint;
			}
		}

		ProcessShotImpact(AdjustedHit, Shot.TraceStart, Shot.InstigatorController);

		if (ABulletTrailActor* Trail = Shot.TrailActor.Get())
		{
			Trail->StartTravel();
		}
		UE_LOG(LogTemp, Warning, TEXT("Release: Trail ptr = %s"), Shot.TrailActor.Get() ? TEXT("valid") : TEXT("NULL"));
	}
	BufferedShots.Empty();
}
// This contains ALL your original HitScan damage, Niagara, and Decal logic!
void AHitScanWeapon::ProcessShotImpact(const FHitResult& FireHit, const FVector& Start, AController* InstigatorController)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr) return;

	// ── Damage ────────────────────────────────────────────────────────────
	AActor* HitActor = FireHit.GetActor(); 
	if (HitActor && InstigatorController)
	{
		bool bCauseAuthDamage = !bUseServerSideRewind || OwnerPawn->IsLocallyControlled();
		if (HasAuthority() && bCauseAuthDamage)
		{
			static const FName HeadBoneName(TEXT("head"));
			const bool bHeadshot = FireHit.BoneName == HeadBoneName;
			const float DamageToCause = CalculateDamage(FireHit.BoneName);
			FVector ShotDirection = (FireHit.ImpactPoint - Start).GetSafeNormal();
 
			UGameplayStatics::ApplyPointDamage(HitActor, DamageToCause, ShotDirection, FireHit, InstigatorController, this, UDamageType::StaticClass());

			if (AGASPBaseCharacter* HitCharacter = Cast<AGASPBaseCharacter>(HitActor))
			{
				if (AGASPBaseCharacter* AttackerChar = Cast<AGASPBaseCharacter>(InstigatorController->GetPawn()))
				{
					if (USpellComponent* SpellComp = AttackerChar->FindComponentByClass<USpellComponent>())
					{
						SpellComp->OnDamageDealt(DamageToCause);
					}
				}
				if (AGASPCorePlayerController* AttackerPC = Cast<AGASPCorePlayerController>(InstigatorController))
					AttackerPC->Client_ShowHitMarker(bHeadshot);
			}
		}
		
		// 3. LAG COMPENSATION
		if (!HasAuthority() && bUseServerSideRewind)
		{
			if (AGASPBaseCharacter* HitCharacter = Cast<AGASPBaseCharacter>(HitActor))
			{
				ShooterOwnerCharacter = ShooterOwnerCharacter == nullptr ? Cast<AGASPBaseCharacter>(OwnerPawn) : ShooterOwnerCharacter;
				ShooterOwnerController = ShooterOwnerController == nullptr ? Cast<AGASPCorePlayerController>(InstigatorController) : ShooterOwnerController;
				if (ShooterOwnerController && ShooterOwnerCharacter && ShooterOwnerCharacter->GetLagCompensation())
				{
					ShooterOwnerCharacter->GetLagCompensation()->ServerScoreRequest(HitCharacter, Start, FireHit.ImpactPoint, ShooterOwnerController->GetServerTime() - ShooterOwnerController->SingleTripTime);
				}
			}
		}
	}
 
	// ── Impact FX, Decals & Noise ─────────────────────────────────────────────────────────
	if (FireHit.bBlockingHit)
	{
		EPhysicalSurface SurfaceType = SurfaceType_Default;
		if (FireHit.PhysMaterial.IsValid()) 
		{
			SurfaceType = UPhysicalMaterial::DetermineSurfaceType(FireHit.PhysMaterial.Get());
		}

		UNiagaraSystem* NiagaraToPlay = DefaultImpactNiagara;
		if (SurfaceImpactNiagara.Contains(SurfaceType)) NiagaraToPlay = SurfaceImpactNiagara[SurfaceType];

		UParticleSystem* ParticleToPlay = DefaultImpactParticles;
		if (SurfaceImpactParticles.Contains(SurfaceType)) ParticleToPlay = SurfaceImpactParticles[SurfaceType];

		USoundBase* SoundToPlay = DefaultImpactSound;
		if (SurfaceImpactSounds.Contains(SurfaceType)) SoundToPlay = SurfaceImpactSounds[SurfaceType];

		UMaterialInterface* DecalToPlay = DefaultImpactDecal;
		if (SurfaceImpactDecals.Contains(SurfaceType)) DecalToPlay = SurfaceImpactDecals[SurfaceType];

		FVector TraceDirection = (FireHit.ImpactPoint - Start).GetSafeNormal(); 
		FRotator SplatterRotation = (TraceDirection * -1.f).Rotation();

		if (NiagaraToPlay) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), NiagaraToPlay, FireHit.ImpactPoint, SplatterRotation);
		else if (ParticleToPlay) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ParticleToPlay, FireHit.ImpactPoint, SplatterRotation);

		if (SoundToPlay) UGameplayStatics::PlaySoundAtLocation(this, SoundToPlay, FireHit.ImpactPoint);

		if (DecalToPlay)
		{
			if (FireHit.GetActor() && FireHit.GetActor()->IsA(AGASPBaseCharacter::StaticClass())) 
			{
				FVector ScatterOffset = FVector(FMath::RandRange(-60.f, 60.f), FMath::RandRange(-60.f, 60.f), 0.f);
				FVector FloorTraceStart = FireHit.ImpactPoint + ScatterOffset;
				FVector FloorTraceEnd = FloorTraceStart + (FVector::DownVector * 400.f); 

				FHitResult FloorHit;
				FCollisionQueryParams IgnoreParams;
				IgnoreParams.AddIgnoredActor(FireHit.GetActor()); 

				if (GetWorld()->LineTraceSingleByChannel(FloorHit, FloorTraceStart, FloorTraceEnd, ECC_Visibility, IgnoreParams))
				{
					FRotator FloorDecalRotation = (-FloorHit.ImpactNormal).Rotation();
					FloorDecalRotation.Roll = FMath::RandRange(0.f, 360.f);
					UGameplayStatics::SpawnDecalAtLocation(GetWorld(), DecalToPlay, DecalSize, FloorHit.ImpactPoint, FloorDecalRotation, DecalLifeSpan);
				}

				FVector WallTraceEnd = FireHit.ImpactPoint + (TraceDirection * 300.f);
				FHitResult WallHit;
				if (GetWorld()->LineTraceSingleByChannel(WallHit, FireHit.ImpactPoint, WallTraceEnd, ECC_Visibility, IgnoreParams))
				{
					FRotator WallDecalRotation = (-WallHit.ImpactNormal).Rotation();
					WallDecalRotation.Roll = FMath::RandRange(0.f, 360.f);
					UGameplayStatics::SpawnDecalAttached(DecalToPlay, DecalSize * 1.5f, WallHit.GetComponent(), NAME_None, WallHit.ImpactPoint, WallDecalRotation, EAttachLocation::KeepWorldPosition, DecalLifeSpan);
				}
			}
			else
			{
				FRotator DecalRotation = (-FireHit.ImpactNormal).Rotation();
				DecalRotation.Roll = FMath::RandRange(0.f, 360.f);
				UGameplayStatics::SpawnDecalAttached(DecalToPlay, DecalSize, FireHit.GetComponent(), NAME_None, FireHit.ImpactPoint, DecalRotation, EAttachLocation::KeepWorldPosition, DecalLifeSpan);
			}
		}

		if (HasAuthority() && InstigatorController)
		{
			UAISense_Hearing::ReportNoiseEvent(GetWorld(), FireHit.ImpactPoint, 1.f, InstigatorController->GetPawn(), 800.f, FName("BulletImpact"));
		}
	}
}

void AHitScanWeapon::WeaponTraceHit(const FVector& TraceStart, const FVector& HitTarget, FHitResult& OutHit, ABulletTrailActor** OutTrailActor)
{
	UWorld* World = GetWorld();
	if (World)
	{
	   FVector End = TraceStart + (HitTarget - TraceStart) * 1.25f;

	   FCollisionQueryParams QueryParams;
	   QueryParams.AddIgnoredActor(GetOwner());
	   QueryParams.AddIgnoredActor(this);
		QueryParams.bReturnPhysicalMaterial = true;
	   
	   World->LineTraceSingleByChannel(
		  OutHit,
		  TraceStart,
		  End,
		  ECollisionChannel::ECC_Visibility,
		  QueryParams 
	   );
	   
	   FVector BeamEnd = End;
	   if (OutHit.bBlockingHit)
	   {
		  BeamEnd = OutHit.ImpactPoint;
	   }
		
	   FVector BeamDirection = (BeamEnd - TraceStart).GetSafeNormal();
	   FVector VisualStart = TraceStart + (BeamDirection * 80.f);
	   float DistanceToTarget = (BeamEnd - TraceStart).Size();
	   if (DistanceToTarget < 80.f)
	   {
		   VisualStart = TraceStart; 
	   }

	   if (BeamParticles)
	   {
		  UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(
			 World,
			 BeamParticles,
			 VisualStart,
			 FRotator::ZeroRotator,
			 true
		  );
		  if (Beam)
		  {
			 Beam->SetVectorParameter(FName("Target"), BeamEnd);
		  }
	   }
		if (BulletTrailSystem)
		{
			FActorSpawnParameters TrailParams;
			TrailParams.Owner = this;
			TrailParams.SpawnCollisionHandlingOverride = 
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			if (ABulletTrailActor* Trail = World->SpawnActor<ABulletTrailActor>(ABulletTrailActor::StaticClass(), VisualStart, FRotator::ZeroRotator, TrailParams))
			{
				Trail->InitTrail(BulletTrailSystem, VisualStart, BeamEnd, BulletTrailSpeed, bIsWeaponInStasis);
				if (OutTrailActor) *OutTrailActor = Trail;
			}
		}
	}
}