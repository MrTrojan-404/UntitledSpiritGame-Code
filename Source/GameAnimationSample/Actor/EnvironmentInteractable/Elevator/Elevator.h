#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameAnimationSample/Interface/InteractionInterface.h"
#include "Elevator.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AElevator : public AActor, public IInteractionInterface
{
	GENERATED_BODY()
	
public:	
	AElevator();
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- Interaction Interface ---
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionText_Implementation() const override;
	
protected:
	virtual void BeginPlay() override;

	// --- Components ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* PlatformMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* SwitchMesh;

	// --- Elevator Settings ---
	// This creates a 3D diamond in the Editor that you can drag to set the top floor!
	UPROPERTY(EditAnywhere, meta = (MakeEditWidget = true), Category = "Elevator Settings")
	FVector TopLocationOffset;

	// Creates a second 3D diamond to set the Bottom Floor
	UPROPERTY(EditAnywhere, meta = (MakeEditWidget = true), Category = "Elevator Settings")
	FVector BottomLocationOffset;
	
	UPROPERTY(EditAnywhere, Category = "Elevator Settings")
	float MoveSpeed = 300.f;

	// --- State ---
	UPROPERTY(Replicated)
	bool bIsAtBottom = true;

	UPROPERTY(Replicated)
	bool bIsMoving = false;

private:
	FVector BottomWorldLocation;
	FVector TopWorldLocation;
	FVector CurrentTargetLocation;
};