#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameAnimationSample/Interface/InteractionInterface.h"
#include "GASPStageGenerator.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGeneratorDisabled, class AGASPStageGenerator*, DisabledGenerator);

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPStageGenerator : public AActor, public IInteractionInterface
{
	GENERATED_BODY()
	
public:	
	AGASPStageGenerator();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "Stage Generator")
	FOnGeneratorDisabled OnGeneratorDisabled;

	// --- INTERACTION INTERFACE ---
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionText_Implementation() const override;

	UFUNCTION(BlueprintCallable, Category = "Stage Generator")
	void ResetGenerator();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* GeneratorMesh;

	// ADDED: The component that holds our looping engine sound
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UAudioComponent* ActiveLoopSoundComp;

	UPROPERTY(ReplicatedUsing = OnRep_IsActive, BlueprintReadOnly, Category = "Stage Generator")
	bool bIsActive = true;

	UFUNCTION()
	void OnRep_IsActive();

	// A clean helper to sync sound and visuals across all machines
	void UpdateVisualsAndAudio();

	// Blueprint Hook: Change the material/lights from Blue (Active) to Black (Disabled) here!
	UFUNCTION(BlueprintImplementableEvent, Category = "Stage Generator")
	void OnGeneratorStateChanged(bool bNewIsActive);

	// The one-shot sound that plays exactly when the player presses 'E'
	UPROPERTY(EditDefaultsOnly, Category = "Stage Generator|Audio")
	class USoundBase* DisableSound;
};