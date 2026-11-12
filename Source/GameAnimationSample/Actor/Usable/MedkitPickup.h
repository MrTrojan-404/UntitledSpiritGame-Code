#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/Interface/InteractionInterface.h"
#include "GameFramework/Actor.h"
#include "MedkitPickup.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AMedkitPickup : public AActor, public IInteractionInterface
{
	GENERATED_BODY()

public:
	AMedkitPickup();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionText_Implementation() const override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	class UBoxComponent* InteractionBox;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	class UStaticMeshComponent* MedkitMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Medkit")
	int32 MedkitAmount = 1;
};