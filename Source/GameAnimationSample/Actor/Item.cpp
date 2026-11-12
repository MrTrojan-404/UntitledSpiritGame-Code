// Fill out your copyright notice in the Description page of Project Settings.


#include "Item.h"

#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "GameAnimationSample/Interface/CombatInterface.h"
#include "GameAnimationSample/UI/Widgets/InteractWidget.h"


AItem::AItem()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	ItemMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ItemMeshComponent"));
	RootComponent = ItemMesh;

	ItemMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	ItemMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ItemMesh->SetGenerateOverlapEvents(false);
	ItemMesh->CanCharacterStepUpOn = ECB_No;
	
	EnableCustomDepth(true);
	ItemMesh->SetCustomDepthStencilValue(DroppedItemStencil);
	ItemMesh->MarkRenderStateDirty();
	
	TraceBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TraceBox"));
	TraceBox->SetupAttachment(RootComponent);
	TraceBox->SetBoxExtent(FVector(20.f, 55.f, 25.f));
	TraceBox->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	TraceBox->CanCharacterStepUpOn = ECB_No;
	TraceBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TraceBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TraceBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	TraceBox->SetGenerateOverlapEvents(false);
}

void AItem::EnableCustomDepth(bool bEnable)
{
	if (ItemMesh)
	{
		ItemMesh->SetRenderCustomDepth(bEnable);
	}
}

void AItem::Interact_Implementation(AActor* Interactor)
{
	if (!Interactor) return;
	if (Interactor->GetClass()->ImplementsInterface(UCombatInterface::StaticClass()))
	{
		ICombatInterface::Execute_OnEquipItem(Interactor, this);
	}
}


FText AItem::GetInteractionText_Implementation() const
{
	return FText::FromString("Pick Up");
}

void AItem::EnableOutline(bool bEnable)
{
	ItemMesh->SetRenderCustomDepth(bEnable);
}