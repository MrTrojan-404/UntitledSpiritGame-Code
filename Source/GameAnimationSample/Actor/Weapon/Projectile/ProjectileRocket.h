// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Projectile.h"
#include "GameAnimationSample/Interface/InteractionInterface.h"
#include "ProjectileRocket.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AProjectileRocket : public AProjectile, public IInteractionInterface
{
	GENERATED_BODY()

public:
	AProjectileRocket();
	virtual void Destroyed() override;

	// We need Tick to force the rocket to rotate with the camera
	virtual void Tick(float DeltaTime) override;

	// --- Interaction Interface ---
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionText_Implementation() const override;
	
	// Called when player HOLDS the interact button on a stasis rocket
	UFUNCTION(BlueprintCallable, Category = "Redirect")
	void BeginRedirect(APlayerController* PC);

	// Called when player RELEASES the interact button
	UFUNCTION(BlueprintCallable, Category = "Redirect")
	void ExecuteRedirect();
	
protected:
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere)
	class USoundBase* ProjectileLoop;

	UPROPERTY()
	UAudioComponent* ProjectileLoopComponent;

	UPROPERTY(EditAnywhere)
	USoundAttenuation* LoopingSoundAttenuation;

	UPROPERTY(VisibleAnywhere)
	class URocketMovementComponent* RocketMovementComponent;

	UPROPERTY(VisibleAnywhere)
	class UBoxComponent* InteractBox;
	
private:
	bool bIsBeingRedirected = false;
	
	UPROPERTY()
	APlayerController* AimingController = nullptr;
};