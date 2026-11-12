#include "MedkitPickup.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"

AMedkitPickup::AMedkitPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	RootComponent = InteractionBox;
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // hit by the interaction focus line trace

	MedkitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MedkitMesh"));
	MedkitMesh->SetupAttachment(RootComponent);
	MedkitMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AMedkitPickup::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority()) return;

	if (AGASPBaseCharacter* Character = Cast<AGASPBaseCharacter>(Interactor))
	{
		Character->AddMedkit(MedkitAmount);
		Destroy();
	}
}

FText AMedkitPickup::GetInteractionText_Implementation() const
{
	return FText::FromString(TEXT("Pick Up Medkit"));
}