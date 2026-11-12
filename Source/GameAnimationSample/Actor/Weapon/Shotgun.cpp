// Fill out your copyright notice in the Description page of Project Settings.


#include "Shotgun.h"

#include "NiagaraFunctionLibrary.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/Component/LagCompenstionComponent.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Perception/AISense_Hearing.h"
#include "Projectile/BulletTrail/BulletTrailActor.h"


void AShotgun::FireShotgun(const TArray<FVector_NetQuantize100>& HitTargets)
{
	AWeapon::Fire(FVector()); 
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr) return;
	AController* InstigatorController = OwnerPawn->GetController();

	const USkeletalMeshSocket* MuzzleFlashSocket = GetItemMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket)
	{
		const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetItemMesh());
		const FVector Start = SocketTransform.GetLocation();

		TArray<FHitResult> FrameHits;
		TArray<ABulletTrailActor*> FrameTrails;
		TArray<TWeakObjectPtr<USkeletalMeshComponent>> FrameMeshComps;
		TArray<FName> FrameBoneNames;
		TArray<FVector> FrameLocalOffsets;

		for (FVector_NetQuantize100 HitTarget : HitTargets)
		{
			FHitResult FireHit;
			ABulletTrailActor* SpawnedTrail = nullptr;
			WeaponTraceHit(Start, HitTarget, FireHit, &SpawnedTrail);
			FrameHits.Add(FireHit);
			FrameTrails.Add(SpawnedTrail);

			TWeakObjectPtr<USkeletalMeshComponent> MeshComp;
			FName BoneName = NAME_None;
			FVector LocalOffset = FVector::ZeroVector;

			if (USkeletalMeshComponent* SkelMesh = Cast<USkeletalMeshComponent>(FireHit.GetComponent()))
			{
				const int32 BoneIndex = SkelMesh->GetBoneIndex(FireHit.BoneName);
				if (BoneIndex != INDEX_NONE)
				{
					MeshComp = SkelMesh;
					BoneName = FireHit.BoneName;
					LocalOffset = SkelMesh->GetBoneTransform(BoneIndex).InverseTransformPosition(FireHit.ImpactPoint);
				}
			}
			FrameMeshComps.Add(MeshComp);
			FrameBoneNames.Add(BoneName);
			FrameLocalOffsets.Add(LocalOffset);
		}

		if (bIsWeaponInStasis)
		{
			FBufferedShotgunShot NewShot;
			NewShot.HitResults = FrameHits;
			NewShot.HitTargets = HitTargets;
			NewShot.TraceStart = Start;
			NewShot.InstigatorController = InstigatorController;
			for (ABulletTrailActor* Trail : FrameTrails) NewShot.TrailActors.Add(Trail);
			NewShot.HitMeshComps    = FrameMeshComps;
			NewShot.HitBoneNames    = FrameBoneNames;
			NewShot.LocalHitOffsets = FrameLocalOffsets;
			BufferedShotgunShots.Add(NewShot);
		}
		else
		{
			ProcessShotgunImpacts(FrameHits, HitTargets, Start, InstigatorController);
		}
	}
}

void AShotgun::ReleaseBufferedShots()
{
	Super::ReleaseBufferedShots();

	for (const FBufferedShotgunShot& Shot : BufferedShotgunShots)
	{
		TArray<FHitResult> AdjustedHits = Shot.HitResults;

		for (int32 i = 0; i < AdjustedHits.Num(); ++i)
		{
			USkeletalMeshComponent* SkelMesh = Shot.HitMeshComps.IsValidIndex(i) ? Shot.HitMeshComps[i].Get() : nullptr;
			if (!SkelMesh) continue;

			const int32 BoneIndex = SkelMesh->GetBoneIndex(Shot.HitBoneNames[i]);
			if (BoneIndex == INDEX_NONE) continue;

			const FVector NewImpactPoint = SkelMesh->GetBoneTransform(BoneIndex).TransformPosition(Shot.LocalHitOffsets[i]);
			AdjustedHits[i].ImpactPoint = NewImpactPoint;
			AdjustedHits[i].Location    = NewImpactPoint;
		}

		ProcessShotgunImpacts(AdjustedHits, Shot.HitTargets, Shot.TraceStart, Shot.InstigatorController);

		for (const TWeakObjectPtr<ABulletTrailActor>& TrailPtr : Shot.TrailActors)
		{
			if (ABulletTrailActor* Trail = TrailPtr.Get())
			{
				Trail->StartTravel();
			}
		}
	}
	BufferedShotgunShots.Empty();
}

void AShotgun::ProcessShotgunImpacts(const TArray<FHitResult>& HitResults, const TArray<FVector_NetQuantize100>& HitTargets, const FVector& Start, AController* InstigatorController)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr) return;

	TMap<AActor*, uint32> HitMap;
	TMap<AActor*, uint32> HeadShotHitMap;
	bool bHasPlayedImpactNoise = false;
       
	for (const FHitResult& FireHit : HitResults)
	{
		if (FireHit.bBlockingHit)
		{
			EPhysicalSurface SurfaceType = SurfaceType_Default;
			if (FireHit.PhysMaterial.IsValid()) SurfaceType = UPhysicalMaterial::DetermineSurfaceType(FireHit.PhysMaterial.Get());

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
             
			if (HitSound) UGameplayStatics::PlaySoundAtLocation(this, SoundToPlay, FireHit.ImpactPoint, .5f, FMath::FRandRange(-.5f, .5f));

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

			if (!bHasPlayedImpactNoise && HasAuthority() && InstigatorController)
			{
				UAISense_Hearing::ReportNoiseEvent(GetWorld(), FireHit.ImpactPoint, 1.f, InstigatorController->GetPawn(), 800.f, FName("BulletImpact"));
				bHasPlayedImpactNoise = true;
			}
		}
          
		if (AActor* HitActor = FireHit.GetActor())
		{
			const bool bHeadShot = FireHit.BoneName == FName("head");

			if (bHeadShot)
			{
				if (HeadShotHitMap.Contains(HitActor)) HeadShotHitMap[HitActor]++;
				else HeadShotHitMap.Emplace(HitActor, 1);
			}
			else
			{
				if (HitMap.Contains(HitActor)) HitMap[HitActor]++;
				else HitMap.Emplace(HitActor, 1);
			}
		}
	}
       
	TArray<AGASPBaseCharacter*> HitCharacters; 
	TMap<AActor*, float> DamageMap;            

	for (auto HitPair : HitMap)
	{
		if (HitPair.Key)
		{
			DamageMap.Emplace(HitPair.Key, HitPair.Value * Damage);
			if (AGASPBaseCharacter* HitChar = Cast<AGASPBaseCharacter>(HitPair.Key)) HitCharacters.AddUnique(HitChar);
		}
	}

	for (auto HeadShotHitPair : HeadShotHitMap)
	{
		if (HeadShotHitPair.Key)
		{
			if (DamageMap.Contains(HeadShotHitPair.Key)) DamageMap[HeadShotHitPair.Key] += HeadShotHitPair.Value * HeadShotDamage;
			else DamageMap.Emplace(HeadShotHitPair.Key, HeadShotHitPair.Value * HeadShotDamage);

			if (AGASPBaseCharacter* HitChar = Cast<AGASPBaseCharacter>(HeadShotHitPair.Key)) HitCharacters.AddUnique(HitChar);
		}
	}

	for (auto DamagePair : DamageMap)
	{
		if (DamagePair.Key && InstigatorController)
		{
			bool bCauseAuthDamage = !bUseServerSideRewind || OwnerPawn->IsLocallyControlled();
			if (HasAuthority() && bCauseAuthDamage)
			{
				UGameplayStatics::ApplyDamage(DamagePair.Key, DamagePair.Value, InstigatorController, this, UDamageType::StaticClass());
			}
		}
	}

	if (!HasAuthority() && bUseServerSideRewind)
	{
		ShooterOwnerCharacter = ShooterOwnerCharacter == nullptr ? Cast<AGASPBaseCharacter>(OwnerPawn) : ShooterOwnerCharacter;
		ShooterOwnerController = ShooterOwnerController == nullptr ? Cast<AGASPCorePlayerController>(InstigatorController) : ShooterOwnerController;
		if (ShooterOwnerController && ShooterOwnerCharacter && ShooterOwnerCharacter->GetLagCompensation() && ShooterOwnerCharacter->IsLocallyControlled())
		{
			ShooterOwnerCharacter->GetLagCompensation()->ShotgunServerScoreRequest(
				HitCharacters, Start, HitTargets, ShooterOwnerController->GetServerTime() - ShooterOwnerController->SingleTripTime
			);
		}
	}
}
void AShotgun::ShotgunTraceEndWithScatter(const FVector& HitTarget, TArray<FVector_NetQuantize100>& HitTargets, float SpreadMultiplier)
{
	const USkeletalMeshSocket* MuzzleFlashSocket = GetItemMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket == nullptr) return;

	const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetItemMesh());
	const FVector TraceStart = SocketTransform.GetLocation();

	const FVector ToTargetNormalized = (HitTarget - TraceStart).GetSafeNormal();
	const FVector SphereCenter = TraceStart + ToTargetNormalized * DistanceToSphere;

	for (uint32 i = 0; i < NumberOfPellets; i++)
	{
		const FVector RandVec = UKismetMathLibrary::RandomUnitVector() * FMath::FRandRange(0.f, SphereRadius * SpreadMultiplier);
		const FVector EndLoc = SphereCenter + RandVec;
		FVector ToEndLoc = EndLoc - TraceStart;
		ToEndLoc = TraceStart + ToEndLoc * TRACE_LENGTH / ToEndLoc.Size();

		HitTargets.Add(ToEndLoc);
	}
}
