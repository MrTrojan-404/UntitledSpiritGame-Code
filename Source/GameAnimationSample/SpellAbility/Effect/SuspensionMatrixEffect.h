#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SuspensionMatrixEffect.generated.h"

class AGASPBaseCharacter;
class USphereComponent;
class UNiagaraSystem;
class UNiagaraComponent;
class USoundBase;
class UAudioComponent;

UCLASS()
class GAMEANIMATIONSAMPLE_API ASuspensionMatrixEffect : public AActor
{
	GENERATED_BODY()

public:	
	ASuspensionMatrixEffect();

	void Initialize(AGASPBaseCharacter* InCaster);

    // Called by the Spell to play FX on the character who threw it
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayCastFX(AGASPBaseCharacter* InCaster);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* ZoneSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* DomeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UNiagaraComponent* ActiveZoneVFX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAudioComponent* ActiveLoopSound;

	// --- VISUALS ---
	UPROPERTY(EditDefaultsOnly, Category = "Suspension Matrix|Visuals")
	float DomeExpansionSpeed = 15.f;
	
	UPROPERTY()
	AGASPBaseCharacter* Caster;

	UPROPERTY(EditDefaultsOnly, Category = "Suspension Matrix")
	float ZoneRadius = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Suspension Matrix")
	float ZoneDuration = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Suspension Matrix")
	float UpwardDriftSpeed = 150.f; // Increased slightly so ragdolls really lift off

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SuspendTarget(AActor* TargetActor, bool bIsCharacter);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BeginHeavyHover(AActor* TargetActor);
	
    // ==========================================
    // VFX & AUDIO
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "Suspension Matrix|VFX")
    UNiagaraSystem* CastVFX;

    UPROPERTY(EditDefaultsOnly, Category = "Suspension Matrix|VFX")
    UNiagaraSystem* DeployVFX;

    UPROPERTY(EditDefaultsOnly, Category = "Suspension Matrix|VFX")
    UNiagaraSystem* ZoneLoopVFX;

    UPROPERTY(EditDefaultsOnly, Category = "Suspension Matrix|VFX")
    UNiagaraSystem* CollapseVFX;

    UPROPERTY(EditDefaultsOnly, Category = "Suspension Matrix|Audio")
    USoundBase* DeploySound;

    UPROPERTY(EditDefaultsOnly, Category = "Suspension Matrix|Audio")
    USoundBase* CollapseSound;

private:
	FTimerHandle ExpirationTimerHandle;
	
	UPROPERTY()
	TSet<UPrimitiveComponent*> SuspendedPhysicsComponents;

	UPROPERTY()
	TSet<AGASPBaseCharacter*> SuspendedCharacters;

	void MaintainZeroG();
	void CollapseZone();

	UPROPERTY()
	TSet<AGASPBaseCharacter*> SuspendedHeavies;

	void MaintainHeavyHover(AGASPBaseCharacter* Target);
};