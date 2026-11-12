// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridSpawner.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API AGridSpawner : public AActor
{
	GENERATED_BODY()

public:    
	AGridSpawner();

	// Fires in the editor whenever the Actor is moved or a property is changed
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UInstancedStaticMeshComponent* InstancedMeshComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
	int32 GridX = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
	int32 GridY = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
	int32 GridZ = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
	float SpacingX = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
	float SpacingY = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
	float SpacingZ = 400.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
	FRotator RotationOffset = FRotator::ZeroRotator;
};