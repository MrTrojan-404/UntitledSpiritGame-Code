// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileRocket.h"
#include "Sound/SoundCue.h"
#include "NiagaraComponent.h"
#include "NiagaraSystemInstance.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "GameAnimationSample/Component/RocketMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Niagara/Public/NiagaraSystemInstanceController.h"

AProjectileRocket::AProjectileRocket()
{
	PrimaryActorTick.bCanEverTick = true;
	
	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rocket Mesh"));
	ProjectileMesh->SetupAttachment(RootComponent);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	RocketMovementComponent = CreateDefaultSubobject<URocketMovementComponent>(TEXT("RocketMovementComponent"));
	RocketMovementComponent->bRotationFollowsVelocity = true;
	RocketMovementComponent->SetIsReplicated(true);

	// 1. Tell the small physics box to IGNORE the crosshair trace so they don't fight
	CollisionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

	// 2. Create the massive Interaction Box
	InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
	InteractBox->SetupAttachment(RootComponent);
	
	// Make this huge! (Adjust these numbers to make grabbing as forgiving as you want)
	InteractBox->SetBoxExtent(FVector(150.f, 80.f, 80.f)); 
	
	InteractBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	
	// ONLY block the visibility trace for interaction
	InteractBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AProjectileRocket::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Align rocket with the player's crosshair continuously
	if (bIsBeingRedirected && AimingController)
	{
		SetActorRotation(AimingController->GetControlRotation());
	}
}

FText AProjectileRocket::GetInteractionText_Implementation() const
{
	if (bIsInStasis)
	{
		return bIsBeingRedirected ? FText::FromString("Release to Fire") : FText::FromString("Hold to Redirect");
	}
	return FText::GetEmpty();
}

void AProjectileRocket::Interact_Implementation(AActor* Interactor)
{
	if (!bIsInStasis) return;

	// The player pressed Interact. Grab the player's controller and start the redirect.
	if (APawn* PawnInstigator = Cast<APawn>(Interactor))
	{
		if (APlayerController* PC = Cast<APlayerController>(PawnInstigator->GetController()))
		{
			BeginRedirect(PC);
		}
	}
}

void AProjectileRocket::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		CollisionBox->OnComponentHit.AddDynamic(this, &AProjectileRocket::OnHit);
	}

	SpawnTrailSystem();

	if (ProjectileLoop && LoopingSoundAttenuation)
	{
		ProjectileLoopComponent = UGameplayStatics::SpawnSoundAttached(
			ProjectileLoop,
			GetRootComponent(),
			FName(),
			GetActorLocation(),
			EAttachLocation::KeepWorldPosition,
			false,
			1.f,
			1.f,
			0.f,
			LoopingSoundAttenuation,
			(USoundConcurrency*)nullptr,
			false
		);
	}
}

void AProjectileRocket::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor == GetOwner()) return;

	ExplodeDamage();
	
	// Reset our Tick fuse so the actor stays alive for 3 more seconds to let FX/Audio play
	TimeAlive = 0.f;
	DestroyTime = 3.f;
	
	if (ImpactParticle)
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticle, GetActorTransform());
	
	if (ImpactSound)
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	
	if (ProjectileMesh)
		ProjectileMesh->SetVisibility(false);
	
	if (CollisionBox)
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	if (TrailSystemComponent && TrailSystemComponent->GetSystemInstanceController())
	{
		TrailSystemComponent->GetSystemInstanceController()->Deactivate();
		TrailSystemComponent->DeactivateImmediate(); 
	}
	
	if (ProjectileLoopComponent && ProjectileLoopComponent->IsPlaying())
		ProjectileLoopComponent->Stop();

	if (RocketMovementComponent)
	{
		RocketMovementComponent->StopMovementImmediately();
		RocketMovementComponent->SetActive(false);
	}
}

void AProjectileRocket::BeginRedirect(APlayerController* PC)
{
	if (!bIsInStasis || !PC) return;

	bIsBeingRedirected = true;
	AimingController = PC;

	if (RocketMovementComponent)
	{
		RocketMovementComponent->Velocity = FVector::ZeroVector;
	}
}

void AProjectileRocket::ExecuteRedirect()
{
	if (!bIsBeingRedirected || !AimingController) return;
	
	bIsBeingRedirected = false;

	if (RocketMovementComponent)
	{
		// Fire it forward based on where the player is looking
		FVector NewDirection = AimingController->GetControlRotation().Vector();
		RocketMovementComponent->Velocity = NewDirection * RocketMovementComponent->InitialSpeed;
	}

	if (APawn* PlayerPawn = AimingController->GetPawn())
	{
		SetInstigator(PlayerPawn);
	}
}
void AProjectileRocket::Destroyed()
{
	
}