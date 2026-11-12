// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectile.h"

#include "Components/BoxComponent.h"
#include "GameAnimationSample/GameAnimationSample.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AISense_Hearing.h"

AProjectile::AProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	
	CollisionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CollisionBox->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	
	CollisionBox->SetCollisionResponseToChannel(ECC_SkeletalMesh, ECR_Block);
	CollisionBox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	CollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	CollisionBox->SetUseCCD(true);
}

void AProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AProjectile, bIsInStasis);
}

void AProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// DeltaTime is automatically scaled by CustomTimeDilation!
	// If TimeDilation is 0.1, it takes 10x longer to reach DestroyTime.
	TimeAlive += DeltaTime;

	if (TimeAlive >= DestroyTime)
	{
		OnDetonate();
		
		// Push TimeAlive out of range so we don't spam OnDetonate while pending kill
		TimeAlive = -9999.f; 
	}
}

void AProjectile::BeginPlay()
{
	Super::BeginPlay();

	//Prevent this projectile from physically colliding with ANY other projectile
	TArray<AActor*> OtherProjectiles;
	UGameplayStatics::GetAllActorsOfClass(this, AProjectile::StaticClass(), OtherProjectiles);
	
	for (AActor* OtherActor : OtherProjectiles)
	{
		if (OtherActor && OtherActor != this)
		{
			// Tell our collision box to ignore the other bullet
			CollisionBox->IgnoreActorWhenMoving(OtherActor, true);
			
			// Tell the other bullet to ignore us
			if (AProjectile* OtherProj = Cast<AProjectile>(OtherActor))
			{
				if (OtherProj->CollisionBox)
				{
					OtherProj->CollisionBox->IgnoreActorWhenMoving(this, true);
				}
			}
		}
	}
	
	if (Tracer)
	{
		TracerComponent = UGameplayStatics::SpawnEmitterAttached(
		   Tracer, CollisionBox, FName(),
		   GetActorLocation(), GetActorRotation(),
		   EAttachLocation::KeepWorldPosition);
	}

	if (HasAuthority())
	{
		CollisionBox->OnComponentHit.AddDynamic(this, &AProjectile::OnHit);
	}

	AActor* MyOwner = GetOwner();
	if (MyOwner == nullptr) MyOwner = GetInstigator();

	if (MyOwner)
	{
		CollisionBox->IgnoreActorWhenMoving(MyOwner, true);
		TArray<AActor*> AttachedActors;
		MyOwner->GetAttachedActors(AttachedActors);
		for (AActor* AttachedActor : AttachedActors)
		{
			CollisionBox->IgnoreActorWhenMoving(AttachedActor, true);
		}
	}
}

void AProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	Destroy();
}

FVector AProjectile::GetVelocity() const
{
    return Super::GetVelocity();
}

void AProjectile::SpawnTrailSystem()
{
	if (TrailSystem)
	{
		TrailSystemComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			TrailSystem,
			GetRootComponent(),
			FName(),
			GetActorLocation(),
			GetActorRotation(),
			EAttachLocation::KeepWorldPosition,
			false
		);
	}
}

void AProjectile::ExplodeDamage()
{
	APawn* FiringPawn = GetInstigator();
	if (FiringPawn && HasAuthority())
	{
		AController* FiringController = FiringPawn->GetController();
		if (FiringController)
		{
			TArray<AActor*> OverlappingActors;
			UGameplayStatics::GetAllActorsOfClass(
				GetWorld(),
				AGASPPlayerCharacter::StaticClass(),
				OverlappingActors
			);

			FVector ExplosionLocation = GetActorLocation();
		
			for (AActor* Actor : OverlappingActors)
			{
				AGASPPlayerCharacter* Character = Cast<AGASPPlayerCharacter>(Actor);
				if (!Character) continue;

				float Distance = FVector::Dist(
					Character->GetActorLocation(),
					ExplosionLocation
				);

				if (Distance > DamageOuterRadius) continue;

				FVector Direction =
					(Character->GetActorLocation() - ExplosionLocation).GetSafeNormal();

				float Strength = FMath::GetMappedRangeValueClamped(
					FVector2D(0.f, DamageOuterRadius),
					FVector2D(1.f, 0.2f),
					Distance);

				Character->LastHitDirection = Direction * Strength;
			}
			
			UGameplayStatics::ApplyRadialDamageWithFalloff(
				this, 
				Damage, 
				10.f, 
				GetActorLocation(), 
				DamageInnerRadius, 
				DamageOuterRadius, 
				1.f, 
				UDamageType::StaticClass(), 
				TArray<AActor*>(), 
				this, 
				FiringController, 
				ECC_Camera
			);
		}
		// Massive Explosion Noise
		UAISense_Hearing::ReportNoiseEvent(
			GetWorld(),
			GetActorLocation(),
			1.f,
			FiringPawn,
			8000.f, // 80 meters - Everyone in the compound will hear this!
			FName("Explosion")
		);
	}
}

void AProjectile::Destroyed()
{
	Super::Destroyed();

	if (ImpactParticle)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticle, GetActorTransform());
	}
	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	}
	
}

void AProjectile::SetStasis(bool bInStasis, float DilationFactor)
{
	bIsInStasis = bInStasis;
	
	// Dynamically scale based on whatever the spell dictates
	CustomTimeDilation = bInStasis ? DilationFactor : 1.0f; 
}

float AProjectile::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (bIsInStasis)
	{
		Destroy(); 
	}
	return ActualDamage;
}
void AProjectile::OnDetonate()
{
	Destroy();
}