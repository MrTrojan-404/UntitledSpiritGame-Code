#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StealthComponent.generated.h"
 
UENUM(BlueprintType)
enum class ENoiseType : uint8
{
    Gunshot             UMETA(DisplayName = "Gunshot"),
    SuppressedGunshot   UMETA(DisplayName = "Suppressed Gunshot"),   // << NEW
    Landing             UMETA(DisplayName = "Landing"),
};
 
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAMEANIMATIONSAMPLE_API UStealthComponent : public UActorComponent
{
    GENERATED_BODY()
 
public:
    UStealthComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
 
    /** Called by the weapon/character on relevant events (gunshot, landing). Server RPC. */
    UFUNCTION(Server, Reliable)
    void Server_EmitNoise(ENoiseType NoiseType, FVector Location);
 
    float GetExposureLevel() const { return ExposureLevel; }
 
protected:
    virtual void BeginPlay() override;
 
private:
    void UpdateExposureLevel(float DeltaTime);
    void UpdateMovementNoise(float DeltaTime);
 
    UPROPERTY(Replicated)
    float CurrentNoiseRadius = 0.f;
 
    float FootstepTimer = 0.f;
 
    UPROPERTY(EditDefaultsOnly, Category="Stealth|Noise")
    float WalkNoiseRadius = 300.f;
 
    UPROPERTY(EditDefaultsOnly, Category="Stealth|Noise")
    float SprintNoiseRadius = 700.f;
 
    UPROPERTY(EditDefaultsOnly, Category="Stealth|Noise")
    float GunshotNoiseRadius = 5000.f;
 
    /** Gunshot noise radius when suppressor is attached. Much shorter range. */
    UPROPERTY(EditDefaultsOnly, Category="Stealth|Noise")
    float SuppressedGunshotNoiseRadius = 600.f;              // << NEW
 
    UPROPERTY(EditDefaultsOnly, Category="Stealth|Noise")
    float LandingNoiseRadius = 800.f;
 
    UPROPERTY(EditDefaultsOnly, Category="Stealth|Noise")
    float FootstepInterval_Walk = 0.55f;
 
    UPROPERTY(EditDefaultsOnly, Category="Stealth|Noise")
    float FootstepInterval_Sprint = 0.30f;
 
    UPROPERTY(EditDefaultsOnly, Category="Stealth|Noise")
    float NoiseDecayRate = 400.f;
 
    UPROPERTY(EditDefaultsOnly, Category="Stealth|Exposure")
    float CrouchExposureMultiplier = 0.4f;
 
    UPROPERTY(EditDefaultsOnly, Category="Stealth|Exposure")
    float SprintExposureMultiplier = 1.4f;

public:
    UPROPERTY(Replicated)
    float ExposureLevel = 1.f;
};
 