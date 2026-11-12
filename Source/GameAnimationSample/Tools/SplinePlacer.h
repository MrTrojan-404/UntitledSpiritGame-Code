// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SplinePlacer.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API ASplinePlacer : public AActor
{
	GENERATED_BODY()
    
public:    
	ASplinePlacer();
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USplineComponent* SplineComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UInstancedStaticMeshComponent* InstancedMeshComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Settings|Transform")
	float MeshLength = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Settings|Transform")
	float SpacingGap = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Settings|Transform")
	FVector PositionOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Settings|Transform")
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Settings|Terrain")
	bool bSnapToTerrain = false;

	// Tilts the mesh to match the angle of the ground
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Settings|Terrain", meta=(EditCondition="bSnapToTerrain"))
	bool bAlignToTerrainSlope = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Settings|Terrain", meta=(EditCondition="bSnapToTerrain"))
	float TraceDistance = 1000.0f;
};