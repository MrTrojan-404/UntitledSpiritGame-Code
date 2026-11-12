// Fill out your copyright notice in the Description page of Project Settings.


#include "InteractionComponent.h"

#include "GameAnimationSample/GameAnimationSample.h"
#include "GameAnimationSample/Actor/Weapon/Projectile/ProjectileRocket.h"
#include "GameAnimationSample/Interface/InteractionInterface.h"


UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerActor = GetOwner();
}

void UInteractionComponent::TryInteract()
{
	if (!FocusedItem) return;
	
	// Lock in the target before we start looking around to aim!
	CurrentInteractingActor = FocusedItem; 
	ServerInteract(FocusedItem);
}

void UInteractionComponent::TryInteractReleased()
{
	// Check the locked actor, NOT the current crosshair focus
	if (!CurrentInteractingActor) return;
	
	ServerInteractReleased(CurrentInteractingActor);
	
	// Clear it out now that we are done
	CurrentInteractingActor = nullptr; 
}

void UInteractionComponent::ServerInteractReleased_Implementation(AActor* TargetActor)
{
	if (!TargetActor || !OwnerActor) return;

	if (AProjectileRocket* Rocket = Cast<AProjectileRocket>(TargetActor))
	{
		Rocket->ExecuteRedirect();
	}
}

void UInteractionComponent::ServerInteract_Implementation(AActor* TargetActor)
{
	if (!TargetActor || !OwnerActor) return;

	// Must be interactable
	if (!TargetActor->GetClass()->ImplementsInterface(UInteractionInterface::StaticClass()))
		return;

	// Must be close enough
	const float Distance = FVector::Dist(
		OwnerActor->GetActorLocation(),
		TargetActor->GetActorLocation()
	);

	if (Distance > INTERACTION_DISTANCE) return;

	// Optional: line of sight check
	FHitResult Hit;
	FVector Start = OwnerActor->GetActorLocation();
	FVector End = TargetActor->GetActorLocation();

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwnerActor);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit, Start, End, ECC_Visibility, Params
	);

	if (bHit && Hit.GetActor() != TargetActor)
		return; // blocked by wall or object

	// Server approves interaction
	IInteractionInterface::Execute_Interact(TargetActor, OwnerActor);
}