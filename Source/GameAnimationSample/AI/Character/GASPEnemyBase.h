#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/AI/Character/GASPAICharacterBase.h"
#include "GASPEnemyBase.generated.h"

class UObjectiveTriggerComponent;
class AGASPWaypointPath;
class UBehaviorTree;

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPEnemyBase : public AGASPAICharacterBase
{
    GENERATED_BODY()

public:
    AGASPEnemyBase();

    virtual void Tick(float DeltaTime) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Perception")
    float GracePeriodDuration = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Patrol")
    AGASPWaypointPath* WaypointPath = nullptr;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
    float MaxChaseDistance = 3000.f;
    
    FVector PatrolOrigin;

    UPROPERTY(BlueprintReadWrite, Category = "AI")
    bool bIsResetting = false;

    UFUNCTION(BlueprintCallable, Category = "AI")
    void TriggerLeashReset();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Objective")
    UObjectiveTriggerComponent* ObjectiveTrigger = nullptr;
    
protected:
    virtual void BeginPlay() override;
    virtual void Destroyed() override;

    virtual void HandleDeath(AController* InstigatorController, AActor* DamageCauser) override;
};