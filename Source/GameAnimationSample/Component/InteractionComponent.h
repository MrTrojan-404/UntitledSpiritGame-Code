// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionComponent.generated.h"

class AItem;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAMEANIMATIONSAMPLE_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent();

	void TryInteract();
	void TryInteractReleased();
	
	UPROPERTY()
	AActor* FocusedItem;

	UPROPERTY()
	TArray<AActor*> NearbyItem;

	UPROPERTY()
	AActor* CurrentInteractingActor;
	
protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	AActor* OwnerActor;

	// Server RPC
	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* TargetActor);

	UFUNCTION(Server, Reliable)
	void ServerInteractReleased(AActor* TargetActor);
};