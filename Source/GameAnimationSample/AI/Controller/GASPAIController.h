#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameAnimationSample/AI/GASPAITypes.h"
#include "Perception/AIPerceptionComponent.h"
#include "GASPAIController.generated.h"

class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UAISenseConfig_Damage;
class UBehaviorTree;
class AGASPEnemyBase;

UENUM(BlueprintType)
enum class EAlertPhase : uint8
{
    Unaware     UMETA(DisplayName = "Unaware"),
    Suspicious  UMETA(DisplayName = "Suspicious"),   // ? mark
    Caution     UMETA(DisplayName = "Caution"),      // searching
    Alert       UMETA(DisplayName = "Alert"),        // full combat
};

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPAIController : public AAIController
{
    GENERATED_BODY()

public:
    AGASPAIController();
    virtual void Tick(float DeltaSeconds) override;
    
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

    // Called by enemy when it dies
    void OnEnemyDied();

    // --- COMPANION KEYS ---
    static const FName BB_CompanionOrder;
    static const FName BB_OrderLocation;
    static const FName BB_LeaderActor;
    static const FName BB_bWeaponsFree;
    static const FName BB_AllyToRevive;
    static const FName BB_MarkedAttackTarget;
    
    // The function the Player will call to command this AI
    UFUNCTION(BlueprintCallable, Category = "AI|Command")
    void IssueOrder(ECompanionOrder NewOrder, FVector Location = FVector::ZeroVector, AActor* TargetActor = nullptr);

    EAlertPhase GetAlertPhase() const { return AlertPhase; }

    UFUNCTION()
    void OnGlobalAlert(AActor* AlertInstigator, float AlertRadius);

    // Expose the current detection fill [0..1] so the HUD subsystem can read it.
    float GetDetectionMeter() const { return DetectionMeter; }
    
#pragma region BlackboardKeys

    static const FName BB_TargetActor;
    static const FName BB_LastKnownLocation;
    static const FName BB_bCanSeeTarget;
    static const FName BB_bHeardSound;
    static const FName BB_SoundLocation;
    static const FName BB_WaypointIndex;
    static const FName BB_bPingPongForward;
    static const FName BB_AIState;
    static const FName BB_bIsInGracePeriod;
    static const FName BB_EnemyType;
    static const FName BB_PreferredRange;
    static const FName BB_CombatStyle;
    static const FName BB_WaypointWaitTime;
    static const FName BB_bIsCaged;
    static const FName BB_bIsFrozen;
    static const FName BB_bIsInStasis;
    static const FName BB_bIsInRagdoll;
    static const FName BB_bIsSuspended;
    static const FName BB_bIsElectrocuted;
    static const FName BB_DistanceToTarget;
    
#pragma endregion BlackboardKeys

#pragma region BlackboardHelpers

    void SetTargetActor(AActor* Target);
    void ClearTarget();
    void SetLastKnownLocation(const FVector& Location);
    void SetAIState(EAIState NewState);
    void SetCanSeeTarget(bool bCanSee);
    void SetHeardSound(bool bHeard, const FVector& Location);
    void SetWaypointIndex(int32 Index);
    void SetPingPongForward(bool bForward);
    void SetIsInGracePeriod(bool bInGrace);
    void SetPreferredRange(float Range);

    EAIState GetAIState() const;

    UFUNCTION(BlueprintPure, Category = "AI|Stasis")
    bool IsInStasis() const;

#pragma endregion BlackboardHelpers

    UFUNCTION(BlueprintPure, Category = "Stealth")
    AActor* GetSuspectedActor() const { return SuspectedActor; }

    // Caches the body so the UI can still clear itself after the brain detaches!
    UPROPERTY()
    APawn* CachedUIBody = nullptr;
    
protected:
    virtual void BeginPlay() override;
    
    UPROPERTY(EditAnywhere, Category = "AI")
    UBehaviorTree* EnemyBehaviorTree = nullptr;

    UPROPERTY(EditAnywhere, Category = "AI")
    UBehaviorTree* CompanionBehaviorTree = nullptr;
    
    UPROPERTY(VisibleAnywhere, Category = "AI")
    UAIPerceptionComponent* AIPerception = nullptr;

    FTimerHandle TargetSweepTimer;
    void PerformTargetSweep();

    FTimerHandle ClearUITimer;
    void ClearStealthUI();

    // Timer for the delay between full detection and alerting the entire camp
    FTimerHandle GlobalAlertDelayTimer;

    // The actual function that fires the global alert
    void BroadcastGlobalAlert();

    // Prevents the AI from endlessly spamming the subsystem
    bool bHasCalledForBackup = false;

    // --- STEALTH TIMERS ---
    
    // How long the red UI arrow stays on screen after full detection
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Stealth|Timers")
    float StealthUIClearTime = 1.0f;

    // How long the enemy waits after spotting you to shout/radio for backup
    UPROPERTY(EditDefaultsOnly, Category="Stealth")
    float TimeToCallForBackup = 1.5f;

    // How long the enemy physically freezes in surprise before shooting/moving
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Stealth|Timers")
    float SurpriseFreezeTime = 1.0f;

    void SetAlertPhase(EAlertPhase NewPhase);

private:

#pragma region Perception

    UPROPERTY()
    UAISenseConfig_Sight* SightConfig = nullptr;

    UPROPERTY()
    UAISenseConfig_Hearing* HearingConfig = nullptr;

    UPROPERTY()
    UAISenseConfig_Damage* DamageConfig = nullptr;

    UFUNCTION()
    void OnTargetPerceptionUpdated(
        AActor* Actor,
        FAIStimulus Stimulus);

    void HandleSightStimulus(
        AActor* Actor,
        const FAIStimulus& Stimulus);

    void HandleHearingStimulus(
        AActor* Actor,
        const FAIStimulus& Stimulus);

    void HandleDamageStimulus(
        AActor* Actor,
        const FAIStimulus& Stimulus);

#pragma endregion Perception

#pragma region GracePeriod

    FTimerHandle GracePeriodTimer;

    // Configurable on enemy subclass
    float GracePeriodDuration = 2.f;

    void StartGracePeriod();
    void OnGracePeriodExpired();

#pragma endregion GracePeriod

    UPROPERTY()
    class AGASPBaseCharacter* OwnedBaseChar = nullptr; // Broadened the scope!

    void InitializePerception(AGASPBaseCharacter* BaseChar);
    void SetupBlackboard(AGASPBaseCharacter* BaseChar);
    
    void SetEnemyType(int32 Type);

    void LogAIStatus(const FString& Context) const;

    FTimerHandle AlertTimer;
    void StartAlertTimer();
    void OnAlertExpired();

    void UpdateDetectionMeter(float DeltaTime);
    float CalculateDetectionRate(AActor* TargetActor) const;

    EAlertPhase AlertPhase = EAlertPhase::Unaware;
    float DetectionMeter   = 0.f;
    bool  bHasLOSToSuspected = false;
    float LOSCheckTimer    = 0.f;

    UPROPERTY()
    AActor* SuspectedActor = nullptr;

    FVector InvestigateLocation = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly, Category="Stealth")
    float BaseDetectionRate    = 1.0f; // meter fill per second at baseline
    UPROPERTY(EditDefaultsOnly, Category="Stealth")
    float DetectionDecayRate   = 0.15f;
    UPROPERTY(EditDefaultsOnly, Category="Stealth")
    float SuspiciousThreshold  = 0.5f;
    UPROPERTY(EditDefaultsOnly, Category="Stealth")
    float CautionThreshold     = 0.65f;
    UPROPERTY(EditDefaultsOnly, Category="Stealth")
    float AlertPropagationRadius = 10000; // -1 = global

    UPROPERTY()
    bool bIsVisuallySensingTarget = false;

    // Prevents the meter from drawing during an active gunfight
    UPROPERTY()
    bool bIsStealthUIHidden = false;

    // --- DISTRACTION SPAM FILTER ---
    int32 CurrentDistractionCount = 0;

    // How many coins the player can throw before the AI realizes it's a trick
    UPROPERTY(EditDefaultsOnly, Category="Stealth|Timers")
    int32 MaxDistractionsAllowed = 2;

    UFUNCTION()
    void OnGlobalAlertCleared();
};