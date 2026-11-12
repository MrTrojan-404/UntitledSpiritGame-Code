// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/Interface/InteractionInterface.h"
#include "GameFramework/Actor.h"
#include "Item.generated.h"

class UBoxComponent;
class UPickupComponent;

UCLASS()
class GAMEANIMATIONSAMPLE_API AItem : public AActor, public IInteractionInterface
{
	GENERATED_BODY()

public:
	AItem();
	
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionText_Implementation() const override;
	
	void EnableOutline(bool bEnable);

	void EnableCustomDepth(bool bEnable);
	
	FORCEINLINE USkeletalMeshComponent* GetItemMesh() const { return ItemMesh; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Outlines")
	int32 DroppedItemStencil = 251;
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USkeletalMeshComponent* ItemMesh;

	UPROPERTY(VisibleAnywhere, Category="Interaction")
	UBoxComponent* TraceBox;
	
};