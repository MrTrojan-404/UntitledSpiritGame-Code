#include "Elevator.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"

AElevator::AElevator()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
	SetRootComponent(RootComp);

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	PlatformMesh->SetupAttachment(RootComp);

	SwitchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwitchMesh"));
	SwitchMesh->SetupAttachment(PlatformMesh); 

	// Default values
	BottomLocationOffset = FVector::ZeroVector; // Defaults to the center of the Actor
	TopLocationOffset = FVector(0.f, 0.f, 1000.f); 
}

void AElevator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AElevator, bIsAtBottom);
	DOREPLIFETIME(AElevator, bIsMoving);
}

void AElevator::BeginPlay()
{
	Super::BeginPlay();

	FVector ActorOrigin = GetActorLocation();

	// Calculate both targets based on the editable offset widgets
	BottomWorldLocation = ActorOrigin + BottomLocationOffset;
	TopWorldLocation = ActorOrigin + TopLocationOffset;

	// Snap the elevator to the bottom location right when the game starts!
	if (HasAuthority())
	{
		SetActorLocation(BottomWorldLocation);
	}
}

void AElevator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Only the Server moves the elevator; bReplicateMovement handles syncing to clients
	if (HasAuthority() && bIsMoving)
	{
		// Constant interpolation for smooth, linear elevator movement
		FVector NewLoc = FMath::VInterpConstantTo(GetActorLocation(), CurrentTargetLocation, DeltaTime, MoveSpeed);
		SetActorLocation(NewLoc);

		// Check if we reached the destination
		if (FVector::Dist(GetActorLocation(), CurrentTargetLocation) < 1.f)
		{
			SetActorLocation(CurrentTargetLocation); // Snap to exact location
			bIsMoving = false;
		}
	}
}

void AElevator::Interact_Implementation(AActor* Interactor)
{
	// Ensure we only process this on the server and ignore spam clicks while moving
	if (!HasAuthority() || bIsMoving) return;

	bIsMoving = true;

	// Toggle destination
	if (bIsAtBottom)
	{
		CurrentTargetLocation = TopWorldLocation;
		bIsAtBottom = false;
	}
	else
	{
		CurrentTargetLocation = BottomWorldLocation;
		bIsAtBottom = true;
	}
}

FText AElevator::GetInteractionText_Implementation() const
{
	if (bIsMoving)
	{
		return FText::FromString("Elevator Moving...");
	}

	return bIsAtBottom ? FText::FromString("Call Elevator (Up)") : FText::FromString("Call Elevator (Down)");
}