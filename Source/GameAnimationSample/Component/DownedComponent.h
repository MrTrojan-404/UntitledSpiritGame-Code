#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameAnimationSample/Interface/InteractionInterface.h"
#include "DownedComponent.generated.h"

class AGASPCorePlayerController;
class AGASPPlayerCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerDowned,
    class AGASPBaseCharacter*, DownedCharacter);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerRevived,
    class AGASPBaseCharacter*, RevivedCharacter);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBleedOutExpired, class AGASPBaseCharacter*, DownedCharacter);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReviveProgress,
    float, Progress);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAMEANIMATIONSAMPLE_API UDownedComponent : public UActorComponent,
    public IInteractionInterface

{
    GENERATED_BODY()

public:
    UDownedComponent();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Called when player hits 0 health
    // replaces Elim in coop
    void GoDown();

    // Called by reviver's interaction
    void StartRevive(AGASPBaseCharacter* Reviver);
    void CancelRevive();

    // IInteractionInterface
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionText_Implementation() const override;

    FORCEINLINE bool IsDowned() const { return bIsDowned; }
    FORCEINLINE int32 GetDownCount() const { return DownCount; }
    FORCEINLINE AGASPBaseCharacter* GetCurrentReviver() const { return CurrentReviver; }

    // GM and HUD listen
    UPROPERTY(BlueprintAssignable, Category = "Downed")
    FOnPlayerDowned OnPlayerDowned;

    UPROPERTY(BlueprintAssignable, Category = "Downed")
    FOnPlayerRevived OnPlayerRevived;

    UPROPERTY(BlueprintAssignable, Category = "Downed")
    FOnBleedOutExpired OnBleedOutExpired;

    // HUD listens for progress bar
    UPROPERTY(BlueprintAssignable, Category = "Downed")
    FOnReviveProgress OnReviveProgress;

#pragma region Configuration

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Downed")
    float BaseBleedOutTime = 60.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Downed")
    float BleedOutReductionPerDown = 15.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Downed")
    float MinBleedOutTime = 15.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Downed")
    float BaseReviveTime = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Downed")
    float ReviveTimeIncreasePerDown = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Downed")
    float MaxReviveTime = 30.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Downed")
    float BaseReviveHealth = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Downed")
    float ReviveHealthReductionPerDown = 0.1f;

#pragma endregion Configuration

    void Reset();

    virtual void ShowInteractionPrompt_Implementation(
        bool bShow) override {}

    float CalculateBleedOutTime() const;
    FORCEINLINE float GetBleedOutTimeRemaining() const { return BleedOutTimeRemaining; }

    
    UPROPERTY(Replicated)
    bool bIsBeingRevived = false;

    float CalculateReviveTime() const;
    
    UPROPERTY(Replicated)
    float ReviveProgress = 0.f;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:

#pragma region State

    UPROPERTY(ReplicatedUsing = OnRep_IsDowned)
    bool bIsDowned = false;

    UFUNCTION()
    void OnRep_IsDowned();

    UPROPERTY(Replicated)
    int32 DownCount = 0;

    UPROPERTY(Replicated)
    float CurrentBleedOutTime = 0.f;

    UPROPERTY(Replicated)
    float BleedOutTimeRemaining = 0.f;

    UPROPERTY()
    AGASPBaseCharacter* OwnerCharacter = nullptr;
    
    UPROPERTY()
    AGASPCorePlayerController* OwnerController = nullptr;

    UPROPERTY()
    AGASPBaseCharacter* CurrentReviver = nullptr;

#pragma endregion State
    
    float CalculateReviveHealth() const;

    void EnterDownedState();
    void ExitDownedState(float RestoreHealth);
    void HandleBleedOut();

    // Multicast for ragdoll and camera
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_EnterDowned();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ExitDowned(float RestoreHealth);

    // Broadcasts revive progress to owner client only
    UFUNCTION(Client, Unreliable)
    void Client_UpdateReviveProgress(float Progress);

    FORCEINLINE bool HasAuthority() const
    {
        return GetOwner() && GetOwner()->HasAuthority();
    }
};