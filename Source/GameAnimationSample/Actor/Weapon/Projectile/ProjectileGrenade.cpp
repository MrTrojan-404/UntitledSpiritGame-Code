// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileGrenade.h"

#include "Components/BoxComponent.h"
#include "GameAnimationSample/GameAnimationSample.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "Sound/SoundCue.h"

AProjectileGrenade::AProjectileGrenade()
{
	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Grenade Mesh"));
	ProjectileMesh->SetupAttachment(RootComponent);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	ProjectileMovementComponent->SetIsReplicated(true);
	ProjectileMovementComponent->bShouldBounce = true;

	ProjectileMovementComponent->InitialSpeed = InitialSpeed;
	ProjectileMovementComponent->MaxSpeed = InitialSpeed;
}

void AProjectileGrenade::BeginPlay()
{
	Super::BeginPlay();

	SpawnTrailSystem();

	// If explode on impact is true, set the fuse to 10 seconds as a fallback
	if (bExplodeOnImpact)
	{
		DestroyTime = 10.f; 
	}
	else
	{
		// === NEW: TIMER GRENADE SETUP ===
		CurrentFuseTime = DestroyTime; // Sync visual tracker to the base Projectile's DestroyTime

		// Grab the mesh's current material and make it dynamic!
		if (ProjectileMesh && ProjectileMesh->GetMaterial(0))
		{
			DynamicGrenadeMat = ProjectileMesh->CreateAndSetMaterialInstanceDynamic(0);
		}

		// Kick off the very first beep!
		TriggerBeep();
	}
	

	// If explode on impact is true, set the fuse to 10 seconds as a fallback
	if (bExplodeOnImpact)
	{
		DestroyTime = 10.f; 
	}

	ProjectileMovementComponent->OnProjectileBounce.AddDynamic(this, &AProjectileGrenade::OnBounce);

	if (CollisionBox)
	{
		CollisionBox->SetCollisionResponseToChannel(ECC_SkeletalMesh, ECR_Ignore);
		CollisionBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	}

	GetWorldTimerManager().SetTimer(
		CollisionArmingTimer, 
		this, 
		&AProjectileGrenade::EnableGrenadeCollision, 
		0.05f, 
		false
	);
}

void AProjectileGrenade::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime); 

	// --- EMISSIVE PULSE LOGIC ---
	if (!bExplodeOnImpact && DynamicGrenadeMat && CurrentFuseTime > 0.f)
	{
		CurrentFuseTime -= DeltaTime;

		// 1. Snappy Fade Out: Interpolate the glow back to 0 rapidly
		CurrentGlowPercent = FMath::FInterpTo(CurrentGlowPercent, 0.f, DeltaTime, 5.0f);
		DynamicGrenadeMat->SetScalarParameterValue(TEXT("GlowIntensity"), CurrentGlowPercent * MaxGlowIntensity);
		
		// 2. Smooth Color Shift
		float TimePercent = FMath::Clamp(CurrentFuseTime / DestroyTime, 0.f, 1.f);
		
		// Lerps from Red (0.0 = boom) to Orange (1.0 = just thrown)
		FLinearColor CurrentColor = FMath::Lerp(FLinearColor::Red, FLinearColor(1.f, 0.2f, 0.f), TimePercent);
		DynamicGrenadeMat->SetVectorParameterValue(TEXT("GlowColor"), CurrentColor);
	}
}
void AProjectileGrenade::TriggerBeep()
{
	// Stop beeping immediately if time is up or if the grenade is pending death
	if (CurrentFuseTime <= 0.f || IsPendingKillPending()) return;

	// 1. Play the synchronized Sound
	if (BeepSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BeepSound, GetActorLocation());
	}

	// 2. Spike the visual glow to 100% instantly
	CurrentGlowPercent = 1.0f;

	// 3. PERFECT ACCELERATION: Directly scale the gap based on time remaining!
	// Starts at ~0.6s gaps, smoothly shrinking down to a frantic 0.06s gap.
	float NextDelay = FMath::Max(0.06f, CurrentFuseTime * 0.2f);

	// 4. Schedule the next beep (Removed the bad safety check that was stopping it early)
	GetWorldTimerManager().SetTimer(BeepTimerHandle, this, &AProjectileGrenade::TriggerBeep, NextDelay, false);
}

void AProjectileGrenade::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (bExplodeOnImpact)
	{
		Destroy(); // Calling Destroy() automatically triggers your ExplodeDamage() setup
	}
	// Otherwise, it skips destruction and bounces normally!
}

void AProjectileGrenade::SetStasis(bool bInStasis, float DilationFactor)
{
	Super::SetStasis(bInStasis, DilationFactor); 

	if (bInStasis)
	{
		GetWorldTimerManager().PauseTimer(CollisionArmingTimer);
		GetWorldTimerManager().PauseTimer(BeepTimerHandle); // Pause the beeping during Stasis
	}
	else
	{
		GetWorldTimerManager().UnPauseTimer(CollisionArmingTimer);
		GetWorldTimerManager().UnPauseTimer(BeepTimerHandle); // Unpause the beeping
	}
}

void AProjectileGrenade::EnableGrenadeCollision()
{
	if (CollisionBox)
	{
		// Turns back on for live enemies
		CollisionBox->SetCollisionResponseToChannel(ECC_SkeletalMesh, ECR_Block);
		
		// Turns back on for the environment
		CollisionBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		
		// Turn back on for dead Ragdolls!
		CollisionBox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	}
}

void AProjectileGrenade::OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	if (BounceSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			BounceSound,
			GetActorLocation()
		);
	}
	// Metallic clink noise to distract guards
	if (HasAuthority() && GetInstigator())
	{
		UAISense_Hearing::ReportNoiseEvent(
			GetWorld(),
			GetActorLocation(),
			1.f,
			GetInstigator(),
			1200.f, // 12 meters - Enough to pull a guard's attention
			FName("GrenadeBounce")
		);
	}
}

void AProjectileGrenade::Destroyed()
{
	GetWorldTimerManager().ClearTimer(BeepTimerHandle); // Clear the audio timer so a deleted grenade doesn't beep!
	ExplodeDamage();
	Super::Destroyed();
}