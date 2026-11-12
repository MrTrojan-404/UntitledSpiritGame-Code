#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GASPStageShield.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPStageShield : public AActor
{
	GENERATED_BODY()
	
public:	
	AGASPStageShield();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Stage Shield")
	void SetShieldActive(bool bActive);

protected:
	virtual void BeginPlay() override; 
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* ShieldMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Stage Shield")
	class UNiagaraSystem* ImpactVFX;

	UPROPERTY(ReplicatedUsing = OnRep_IsActive)
	bool bIsActive = true;

	UFUNCTION()
	void OnRep_IsActive();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayImpactVFX(FVector ImpactLocation, FVector ImpactNormal);

private:
	// --- SMOOTH DISSOLVE VARIABLES ---
	UPROPERTY()
	class UMaterialInstanceDynamic* DynamicShieldMat;

	float CurrentDissolve = -0.5f;
	float TargetDissolve = -0.5f;
};