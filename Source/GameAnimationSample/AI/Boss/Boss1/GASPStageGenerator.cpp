#include "GASPStageGenerator.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AGASPStageGenerator::AGASPStageGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	GeneratorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GeneratorMesh"));
	RootComponent = GeneratorMesh;
	
	GeneratorMesh->SetCollisionObjectType(ECC_WorldDynamic);
	GeneratorMesh->SetCollisionResponseToAllChannels(ECR_Block);

	// Initialize the looping audio component
	ActiveLoopSoundComp = CreateDefaultSubobject<UAudioComponent>(TEXT("ActiveLoopSoundComp"));
	ActiveLoopSoundComp->SetupAttachment(RootComponent);
}

void AGASPStageGenerator::BeginPlay()
{
	Super::BeginPlay();
	
	// Set the initial visual and audio state on spawn
	UpdateVisualsAndAudio();
}

void AGASPStageGenerator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGASPStageGenerator, bIsActive);
}

void AGASPStageGenerator::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority() || !bIsActive) return;

	bIsActive = false;
	
	// Manually update the server's visuals and turn off the loop
	UpdateVisualsAndAudio(); 

	// Play the interaction "Shut Down" sound!
	if (DisableSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DisableSound, GetActorLocation());
	}

	// Tell the Manager!
	OnGeneratorDisabled.Broadcast(this);
}

FText AGASPStageGenerator::GetInteractionText_Implementation() const
{
	if (bIsActive)
	{
		return FText::FromString(TEXT("Disable Generator"));
	}
	
	return FText::GetEmpty();
}

void AGASPStageGenerator::ResetGenerator()
{
	if (!HasAuthority()) return;
	
	bIsActive = true;
	
	// Manually update the server's visuals and turn the loop back on
	UpdateVisualsAndAudio(); 
}

void AGASPStageGenerator::OnRep_IsActive()
{
	// The Client automatically runs this when the server changes 'bIsActive'
	UpdateVisualsAndAudio();
}

void AGASPStageGenerator::UpdateVisualsAndAudio()
{
	// 1. Handle the Looping Audio
	if (bIsActive)
	{
		if (ActiveLoopSoundComp && !ActiveLoopSoundComp->IsPlaying())
		{
			ActiveLoopSoundComp->Play();
		}
	}
	else
	{
		if (ActiveLoopSoundComp && ActiveLoopSoundComp->IsPlaying())
		{
			ActiveLoopSoundComp->Stop();
		}
	}

	// 2. Trigger your Blueprint event so you can swap glowing materials
	OnGeneratorStateChanged(bIsActive);
}