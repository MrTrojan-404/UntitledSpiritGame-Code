#include "ExplosiveBarrel.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Engine/DamageEvents.h"
#include "Perception/AISense_Hearing.h"

AExplosiveBarrel::AExplosiveBarrel()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(true);
	
	BarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrelMesh"));
	RootComponent = BarrelMesh;
	
	BarrelMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BarrelMesh->SetCollisionObjectType(ECC_WorldDynamic);
	BarrelMesh->SetCollisionResponseToAllChannels(ECR_Block);
	BarrelMesh->SetSimulatePhysics(true);
	BarrelMesh->SetMassOverrideInKg(NAME_None, 30.f, true);

	DefaultFusePoint = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultFusePoint"));
	DefaultFusePoint->SetupAttachment(BarrelMesh);
}

void AExplosiveBarrel::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
}

void AExplosiveBarrel::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsFuseActive && !bHasExploded)
	{
		FuseTimeAlive += DeltaTime;

		if (FuseTimeAlive >= FuseDuration)
		{
			Explode();
		}
	}
}

void AExplosiveBarrel::SetStasis(bool bInStasis, float DilationFactor)
{
	// Dynamically scale based on the spell's settings
	CustomTimeDilation = bInStasis ? DilationFactor : 1.0f; 
}

float AExplosiveBarrel::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bHasExploded) return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	CurrentHealth -= DamageAmount;

	if (CurrentHealth <= 0.f && !bIsFuseActive)
	{
		// 1. Check if the damage is strictly Point Damage (e.g., a Bullet/Sniper)
		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			const FPointDamageEvent* PointDamageEvent = static_cast<const FPointDamageEvent*>(&DamageEvent);
		
			MulticastStartFuse(PointDamageEvent->HitInfo.ImpactPoint, PointDamageEvent->HitInfo.ImpactNormal, false);
		}
		// 2. If it is Radial Damage (Inferno Wave), or Generic Damage, use the top fuse
		else
		{
			MulticastStartFuse(FVector::ZeroVector, FVector::ZeroVector, true);
		}
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AExplosiveBarrel::MulticastStartFuse_Implementation(FVector HitLocation, FVector HitNormal, bool bUseDefaultLocation)
{
	bIsFuseActive = true;

	if (FuseVFX)
	{
		if (bUseDefaultLocation && DefaultFusePoint)
		{
			ActiveFuseVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
				FuseVFX,
				DefaultFusePoint,
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget,
				true
			);
		}
		else
		{
			FTransform MeshTransform = BarrelMesh->GetComponentTransform();
			FVector RelativeLoc = MeshTransform.InverseTransformPosition(HitLocation);
			FRotator RelativeRot = MeshTransform.InverseTransformRotation(HitNormal.Rotation().Quaternion()).Rotator();

			ActiveFuseVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
				FuseVFX,
				BarrelMesh,
				NAME_None,
				RelativeLoc,
				RelativeRot,
				EAttachLocation::KeepRelativeOffset,
				true
			);
		}
	}
}

void AExplosiveBarrel::Explode()
{
	bHasExploded = true;

	if (HasAuthority())
	{
		TArray<AActor*> IgnoredActors;
		IgnoredActors.Add(this);

		UGameplayStatics::ApplyRadialDamage(
			GetWorld(), BaseDamage, GetActorLocation(), DamageRadius, 
			UDamageType::StaticClass(), IgnoredActors, this, nullptr, false, ECC_Visibility
		);

		// Massive explosion noise(40 meters)
		// We pass 'this' as the instigator so the AI looks at where the barrel was.
		UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), 1.f, this, 4000.f, FName("Explosion"));

		Destroy(); 
	}
}

void AExplosiveBarrel::Destroyed()
{
	// EVERYONE (Server and Clients) plays the visuals and sound right as the barrel is deleted
	if (ActiveFuseVFX)
	{
		ActiveFuseVFX->Deactivate();
	}

	if (ExplosionParticle)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionParticle, GetActorTransform());
	}

	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, GetActorLocation());
	}
	
	Super::Destroyed();
}