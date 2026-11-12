// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ThrowableDistraction.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AThrowableDistraction : public AActor
{
	GENERATED_BODY()

public:	
	AThrowableDistraction();

protected:
	virtual void BeginPlay() override;

	// The function that fires when the coin hits a wall or the floor
	UFUNCTION()
	void OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UProjectileMovementComponent* ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, Category = "Distraction")
	float NoiseRange = 1500.f; // 15 meters

	UPROPERTY(EditDefaultsOnly, Category = "Distraction")
	float InitialSpeed = 1300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Distraction")
	float MaxSpeed = 1300.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Distraction")
	USoundBase* BounceSound;

private:
	bool bHasBounced = false;
};