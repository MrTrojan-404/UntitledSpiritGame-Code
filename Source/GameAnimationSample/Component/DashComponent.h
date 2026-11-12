#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DashComponent.generated.h"

class UMoverComponent;
class AGASPBaseCharacter;
class UNiagaraSystem;
class UNiagaraComponent;
class UAudioComponent;
class USoundBase;
class UAnimMontage;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class GAMEANIMATIONSAMPLE_API UDashComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDashComponent();

	UFUNCTION(BlueprintCallable, Category = "Movement|Dash")
	void TryDash();

	UFUNCTION(BlueprintPure, Category = "Movement|Dash")
	bool IsDashing() const { return bIsDashing; }
	
protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Dash")
	float DashSpeed = 2000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Dash")
	float DashDurationMs = 270.f; // matches your LandingClamp convention

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Dash")
	float DashCooldown = 0.6f;

	void ExecuteDash(FVector Direction);

	UFUNCTION(Server, Reliable)
	void ServerDash(FVector ClientDirection);

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Dash")
	float PostDashLandingSpeed = 600.f; // matches WallRunComponent's GroundLandingSpeed convention

	FTimerHandle DashClampTimer;
	void ClampVelocityAfterDash();

    // ==========================================
    // FX AND ANIMATION
    // ==========================================
    
    void PlayDashFX(FVector Direction);
    void EndDashFX();

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_DashFX(FVector Direction);

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_EndDashFX();

    // --- Animations ---
    UPROPERTY(EditDefaultsOnly, Category = "Dash|Animation")
    UAnimMontage* DashForwardAnim;
    
    UPROPERTY(EditDefaultsOnly, Category = "Dash|Animation")
    UAnimMontage* DashBackwardAnim;
    
    UPROPERTY(EditDefaultsOnly, Category = "Dash|Animation")
    UAnimMontage* DashLeftAnim;
    
    UPROPERTY(EditDefaultsOnly, Category = "Dash|Animation")
    UAnimMontage* DashRightAnim;

    // --- VFX ---
    UPROPERTY(EditDefaultsOnly, Category = "Dash|Visuals")
    UNiagaraSystem* DashStartVFX;
    
    UPROPERTY(EditDefaultsOnly, Category = "Dash|Visuals")
    UNiagaraSystem* DashEndVFX;
    
    UPROPERTY(EditDefaultsOnly, Category = "Dash|Visuals")
    UNiagaraSystem* DashTrailVFX;

    UPROPERTY()
    UNiagaraComponent* ActiveTrailVFX;

	// --- Cascade VFX ---
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Visuals|Cascade")
	UParticleSystem* DashStartCascadeVFX;

	UPROPERTY(EditDefaultsOnly, Category = "Dash|Visuals|Cascade")
	UParticleSystem* DashEndCascadeVFX;

    // --- Sound ---
    UPROPERTY(EditDefaultsOnly, Category = "Dash|Audio")
    USoundBase* DashStartSound;
    
    UPROPERTY(EditDefaultsOnly, Category = "Dash|Audio")
    USoundBase* DashEndSound;
    
    UPROPERTY(EditDefaultsOnly, Category = "Dash|Audio")
    USoundBase* DashTrailSound;

    UPROPERTY()
    UAudioComponent* ActiveTrailSound;

	// --- Afterimage ---
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Visuals")
	UNiagaraSystem* AfterimageVFX;

	// At 270ms dash duration, 0.085s will spawn exactly 3 afterimages during the movement
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Visuals")
	float AfterimageInterval = 0.085f; 

	FTimerHandle AfterimageTimer;
	void SpawnAfterimage();
	
private:
	UPROPERTY()
	AGASPBaseCharacter* CharacterOwner;

	UPROPERTY()
	UMoverComponent* MoverComp;

	bool bDashOnCooldown = false;
	bool bHasAirDashed = false; // resets when grounded — one dash per jump

	FTimerHandle DashCooldownTimer;
	void ClearDashCooldown();

	bool bWasWallRunningLastTick = false;

	bool bInterruptedReload = false;

	bool bIsDashing = false;
	
};