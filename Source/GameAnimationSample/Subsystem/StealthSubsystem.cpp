#include "StealthSubsystem.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"
#include "TimerManager.h"
 
// ── Global alert (unchanged) ───────────────────────────────────────────────
void UStealthSubsystem::TriggerGlobalAlert(AActor* Instigator, float AlertRadius)
{
    if (!GetWorld()) return;
    bGlobalAlertActive = true;
    OnGlobalAlert.Broadcast(Instigator, AlertRadius);
 
    GetWorld()->GetTimerManager().SetTimer(AlertClearTimer, this,
        &UStealthSubsystem::ClearGlobalAlert, GlobalAlertDuration, false);
}
 
void UStealthSubsystem::ClearGlobalAlert()
{
    bGlobalAlertActive = false;
    OnGlobalAlertCleared.Broadcast();
}
 
// ── Per-AI detection registry ───────────────────────────────────────
void UStealthSubsystem::RegisterDetection(AGASPAIController* AIC, float Level, uint8 AlertPhaseRaw)
{
    if (!AIC) return;

    FStealthDetectionData Data;
    Data.DetectionLevel = Level;
    Data.AlertPhase = AlertPhaseRaw;
    
    ActiveDetections.Add(AIC, Data);
    
    RecomputeMaxDetection();
}

void UStealthSubsystem::RemoveZeroDetections()
{
    for (auto It = ActiveDetections.CreateIterator(); It; ++It)
    {
        // If the meter is 0, delete it from memory!
        if (It.Value().DetectionLevel <= 0.01f && It.Value().AlertPhase == 0)
        {
            It.RemoveCurrent();
        }
    }
    RecomputeMaxDetection();
}

void UStealthSubsystem::UnregisterDetection(AGASPAIController* AIC)
{
    if (!AIC) return;
    
    ActiveDetections.Remove(AIC);
    RecomputeMaxDetection();
}

void UStealthSubsystem::RecomputeMaxDetection()
{
    MaxDetectionLevel    = 0.f;
    HighestAlertPhaseRaw = 0;

    // Loop through the NEW map instead of the old DetectionRegistry
    for (auto It = ActiveDetections.CreateIterator(); It; ++It)
    {
        // Clean up stale (destroyed) controllers while we're iterating
        if (!It.Key())
        {
            It.RemoveCurrent();
            continue;
        }
        
        // Find the highest values to keep your global subsystem states accurate
        if (It.Value().DetectionLevel > MaxDetectionLevel)
        {
            MaxDetectionLevel = It.Value().DetectionLevel;
            HighestAlertPhaseRaw = It.Value().AlertPhase;
        }
    }
}