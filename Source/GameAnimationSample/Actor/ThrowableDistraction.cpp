#include "ThrowableDistraction.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Kismet/GameplayStatics.h"

AThrowableDistraction::AThrowableDistraction()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;
	MeshComp->SetCollisionProfileName(TEXT("Projectile")); // Ensures it bounces off walls

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 1200.f;
	ProjectileMovement->MaxSpeed = 1200.f;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.4f; // A heavy coin/rock bounce
	ProjectileMovement->ProjectileGravityScale = 1.2f;
}

void AThrowableDistraction::BeginPlay()
{
	Super::BeginPlay();
	
	// Bind our bounce function to Unreal's physics engine
	ProjectileMovement->OnProjectileBounce.AddDynamic(this, &AThrowableDistraction::OnBounce);
	
	// Delete the coin after 10 seconds so we don't clutter the map memory!
	SetLifeSpan(10.f); 
}

void AThrowableDistraction::OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	// Only trigger the distraction on the very FIRST bounce, not the rolling/settling
	if (!bHasBounced)
	{
		bHasBounced = true;

		// 1. Play the physical "clink" sound for the player to hear
		if (BounceSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, BounceSound, ImpactResult.ImpactPoint);
		}

		// 2. Transmit the noise to the AI's Brain!
		UAISense_Hearing::ReportNoiseEvent(
			GetWorld(),
			ImpactResult.ImpactPoint,
			1.0f,
			GetInstigator(),
			NoiseRange,
			FName("Distraction")
		);
	}
}