#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SquadCommandComponent.generated.h"

// 1. Move the Enum here!
UENUM(BlueprintType)
enum class ETargetingMode : uint8
{
    ETM_None UMETA(DisplayName = "None"),
    ETM_CompanionMoveToAll UMETA(DisplayName = "Squad Move To"),
    ETM_CompanionMoveToSpecific UMETA(DisplayName = "Specific Move To"),
    ETM_Airstrike UMETA(DisplayName = "Airstrike")
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class GAMEANIMATIONSAMPLE_API USquadCommandComponent : public UActorComponent
{
    GENERATED_BODY()

public:	
    USquadCommandComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // --- TARGETING SYSTEM ---
    void StartTargetingMode(ETargetingMode Mode);
    void CancelTargetingMode();
    void ConfirmTargetLocation();

    // --- SQUAD STATE ---
    UPROPERTY(BlueprintReadOnly, Category = "Squad")
    TArray<class AGASPBaseCharacter*> AvailableCompanions; // Changed to Base!

    UPROPERTY(BlueprintReadOnly, Category = "Squad")
    AGASPBaseCharacter* ActiveSelectedCompanion = nullptr; // Changed to Base!

    int32 SelectedCompanionIndex = 0;
    ETargetingMode CurrentTargetingMode = ETargetingMode::ETM_None;
    AGASPBaseCharacter* HoveredDownedAlly = nullptr;

    // --- SERVER RPCs ---
    UFUNCTION(Server, Reliable)
    void ServerIssueSquadOrder(int32 SelectedSegment, FVector OrderLocation);

    UFUNCTION(Server, Reliable)
    void ServerIssueSpecificOrder(int32 SelectedSegment, AGASPBaseCharacter* TargetCompanion, FVector OrderLocation); // Changed to Base!

    UFUNCTION(Server, Reliable)
    void ServerDispatchMedicToAlly(class AGASPBaseCharacter* DownedAlly, AGASPBaseCharacter* SpecificMedic = nullptr); // Changed to Base!

    UFUNCTION(Server, Reliable)
    void ServerCallForMedic(class AGASPBaseCharacter* DownedAlly);
    
    UFUNCTION(BlueprintCallable, Category = "Squad")
    void ToggleFollowStandBy();
    
    // Helper to ask the current body where it's aiming
    FVector GetCrosshairHitTarget() const;

    // --- TARGETING SYSTEM OUTLINES ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Outlines")
    int32 NormalCompanionStencil = 252;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Outlines")
    int32 DownedAllyStencil = 254;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Outlines")
    int32 HoveredDownedAllyStencil = 253;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Outlines")
    int32 MarkedHostileStencil = 251;
    
protected:
    virtual void BeginPlay() override;

    void HighlightAllCompanions(bool bEnable);

    // --- DECAL ---
    // Instead of forcing the Character Blueprint to have a decal, 
    // the Component will just spawn one dynamically!
    UPROPERTY(EditDefaultsOnly, Category = "Squad|Targeting")
    UMaterialInterface* TargetingDecalMaterial;

    // Expose the dimensions to the Blueprint with a safe default size
    UPROPERTY(EditDefaultsOnly, Category = "Squad|Targeting")
    FVector TargetingDecalSize = FVector(128.f, 128.f, 128.f);
    
    UPROPERTY()
    class ADecalActor* TargetingDecalActor;

    AGASPBaseCharacter* GetCurrentCompanion() const;

    FTimerHandle MarkExecuteTimer;

    UFUNCTION()
    void OnLineupComplete();

    FTimerHandle MarkLineupTimer;
    bool bMarkReadyToExecute = false;

    bool bCompanionCurrentlyFollowing = true; // starts following by default

    bool bHasInitializedCompanionOrder = false;
    
public:
    // --- AUTO STEALTH-DRIVEN FIRE STATE ---

    UPROPERTY(EditDefaultsOnly, Category = "Squad|Stealth")
    float ExposureThresholdForCombat = 0.6f; // tune to your ExposureLevel range

    // --- MARK & EXECUTE ---
    UFUNCTION(BlueprintCallable, Category = "Squad|Mark")
    void TryMarkTarget(AActor* Target);

    UFUNCTION(BlueprintCallable, Category = "Squad|Mark")
    void ClearMark();

    UFUNCTION(BlueprintCallable, Category = "Squad|Mark")
    void ExecuteMark();

    UPROPERTY(BlueprintReadOnly, Category = "Squad|Mark")
    AActor* MarkedTarget = nullptr;

    bool HasLineOfSight(AGASPBaseCharacter* From, AActor* Target) const;

    UFUNCTION(BlueprintPure, Category = "Squad|Mark")
    bool HasMarkedTarget() const { return MarkedTarget != nullptr; }

    UPROPERTY(EditDefaultsOnly, Category = "Squad|Mark")
    float MarkLineupDuration = 3.0f;
    
    UFUNCTION(BlueprintPure, Category = "Squad|Mark")
    bool IsMarkReady() const { return bMarkReadyToExecute; }
    
    UPROPERTY(EditDefaultsOnly, Category = "Squad|Mark|Outlines")
    int32 MarkedLiningUpStencil = 253;  // e.g. yellow in your PP material

    UPROPERTY(EditDefaultsOnly, Category = "Squad|Mark|Outlines")
    int32 MarkedReadyStencil = 254;     // e.g. red/white — "go" signal
};