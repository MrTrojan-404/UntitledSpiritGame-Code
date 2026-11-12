#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "StealthSubsystem.generated.h"

USTRUCT()
struct FStealthDetectionData
{
    GENERATED_BODY()
    float DetectionLevel = 0.f;
    uint8 AlertPhase = 0;
};

class AGASPAIController;   // forward declare — no circular dependency
 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGlobalAlert, AActor*, Instigator, float, AlertRadius);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGlobalAlertCleared);

UCLASS()
class GAMEANIMATIONSAMPLE_API UStealthSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
 
public:
    // ── Global alert (unchanged) ───────────────────────────────────────────
    void TriggerGlobalAlert(AActor* Instigator, float AlertRadius = -1.f);
    void ClearGlobalAlert();
    bool IsGlobalAlertActive() const { return bGlobalAlertActive; }
 
    UPROPERTY(BlueprintAssignable)
    FOnGlobalAlert OnGlobalAlert;

    UPROPERTY(BlueprintAssignable)
    FOnGlobalAlertCleared OnGlobalAlertCleared;
    
    // ── Per-AI detection registry (NEW) ───────────────────────────────────
    /**
     * Called every tick by each AGASPAIController with its current DetectionMeter [0..1].
     * The subsystem keeps a live map and tracks the highest level across all enemies.
     * Phase is passed as uint8 to avoid a hard header dependency.
     */
    void RegisterDetection(AGASPAIController* AIC, float Level, uint8 AlertPhaseRaw);
 
    /** Call from OnEnemyDied / OnUnPossess so dead/inactive AI are removed. */
    void UnregisterDetection(AGASPAIController* AIC);
 
    /** Highest detection level (0–1) across all living enemies. */
    float GetHighestDetectionLevel() const { return MaxDetectionLevel; }
 
    /** EAlertPhase of the most alert enemy, as raw uint8 (avoids header dependency). */
    uint8 GetHighestAlertPhaseRaw() const  { return HighestAlertPhaseRaw; }

    // A map holding all active enemies tracking the player
    TMap<class AController*, FStealthDetectionData> ActiveDetections;

    const TMap<AController*, FStealthDetectionData>& GetActiveDetections() const { return ActiveDetections; }

    // Sweeps the map and removes 0-values AFTER the UI has been updated
    void RemoveZeroDetections();
private:
    bool bGlobalAlertActive = false;
    FTimerHandle AlertClearTimer;
 
    UPROPERTY(EditDefaultsOnly)
    float GlobalAlertDuration = 30.f;
 
    float MaxDetectionLevel     = 0.f;
    uint8 HighestAlertPhaseRaw  = 0;   // 0 = Unaware
 
    void RecomputeMaxDetection();
};