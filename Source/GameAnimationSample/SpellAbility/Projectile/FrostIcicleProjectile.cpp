#include "FrostIcicleProjectile.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/AudioComponent.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameAnimationSample/SpellAbility/Spell/Ren/FrostLockSpell.h"
#include "Kismet/GameplayStatics.h"

AFrostIcicleProjectile::AFrostIcicleProjectile()
{
	PrimaryActorTick.bCanEverTick = false; 
	bReplicates = true;
	SetReplicateMovement(true); 

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent = CollisionSphere;
	CollisionSphere->InitSphereRadius(15.f);
	
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	
	CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AFrostIcicleProjectile::OnOverlap);
	CollisionSphere->OnComponentHit.AddDynamic(this, &AFrostIcicleProjectile::OnHit);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->bRotationFollowsVelocity = true; 
	ProjectileMovement->InitialSpeed = 2500.f; 
	ProjectileMovement->MaxSpeed = 2500.f;
	ProjectileMovement->ProjectileGravityScale = 0.f; 
	ProjectileMovement->bIsHomingProjectile = true;
	ProjectileMovement->HomingAccelerationMagnitude = 25000.f; 
}

void AFrostIcicleProjectile::Initialize(AGASPBaseCharacter* InCaster, AGASPBaseCharacter* InTarget, UFrostLockSpell* InOwningSpell)
{
	Caster = InCaster;
	Target = InTarget;
	OwningSpell = InOwningSpell;

	if (Caster) CollisionSphere->IgnoreActorWhenMoving(Caster, true);

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = HomingSpeed;
		ProjectileMovement->MaxSpeed = HomingSpeed;

		if (Target)
		{
			ProjectileMovement->HomingTargetComponent = Target->GetRootComponent();
			ProjectileMovement->bIsHomingProjectile = true;
		}
		else
		{
			// Dumbfire logic overrides
			ProjectileMovement->bIsHomingProjectile = false;
		}
	}
}
void AFrostIcicleProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 1. Play the trail on ALL screens
	if (TrailSystem)
	{
		ActiveTrail = UNiagaraFunctionLibrary::SpawnSystemAttached(
			TrailSystem, 
			RootComponent, 
			NAME_None, 
			FVector::ZeroVector, 
			FRotator(0.f, 180.f, 0.f), // FLIPPED 180 DEGREES
			EAttachLocation::KeepRelativeOffset, 
			true
		);
	}

	// 2. Play the travel sound on ALL screens
	if (TravelSound)
	{
		ActiveTravelSound = UGameplayStatics::SpawnSoundAttached(TravelSound, RootComponent);
	}

	SetLifeSpan(MaxLifetime);
}

void AFrostIcicleProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || bHasHit) return;

	AGASPBaseCharacter* HitChar = Cast<AGASPBaseCharacter>(OtherActor);
	if (!HitChar || HitChar == Caster || HitChar->IsPlayerAlly()) return;
	
	// Prevent freezing the same target twice on overlap
	if (HitActors.Contains(HitChar)) return;

	HitActors.Add(HitChar);

	if (OwningSpell)
	{
		OwningSpell->ApplyFreezeToTarget(HitChar);
	}

	// === PIERCING LOGIC ===
	if (HitActors.Num() >= 2) 
	{
		// Reached Max Pierces (Hit 2 enemies). Stop moving!
		bHasHit = true;
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ProjectileMovement->StopMovementImmediately();

		Multicast_HitTarget();
		SetLifeSpan(2.f);
	}
	else
	{
		// First hit! We pierced! Turn off homing so it shoots straight towards the next victim.
		ProjectileMovement->bIsHomingProjectile = false;
		
		// Keep momentum going straight to prevent it from dropping or spinning
		ProjectileMovement->Velocity = ProjectileMovement->Velocity.GetSafeNormal() * HomingSpeed; 
	}
}

void AFrostIcicleProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Wall Hit
	if (!HasAuthority() || bHasHit) return;

	bHasHit = true;
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMovement->StopMovementImmediately();

	Multicast_HitTarget();
	SetLifeSpan(2.f);
}

void AFrostIcicleProjectile::Multicast_HitTarget_Implementation()
{
	if (ActiveTrail)
	{
		ActiveTrail->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		ActiveTrail->Deactivate();
	}

	if (ActiveTravelSound)
	{
		ActiveTravelSound->Stop();
	}

	if (HitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HitSound, GetActorLocation());
	}
}

void AFrostIcicleProjectile::Destroyed()
{
	// Detach the trail so it fades naturally instead of popping out
	if (ActiveTrail)
	{
		ActiveTrail->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		ActiveTrail->Deactivate();
	}

	//  STOP THE LOOPING TRAVEL SOUND
	if (ActiveTravelSound)
	{
		ActiveTravelSound->Stop();
	}

	Super::Destroyed();
}