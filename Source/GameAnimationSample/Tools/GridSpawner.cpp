#include "GridSpawner.h"
#include "Components/InstancedStaticMeshComponent.h"

AGridSpawner::AGridSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	InstancedMeshComp = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InstancedMeshComp"));
	RootComponent = InstancedMeshComp;
}

void AGridSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!InstancedMeshComp->GetStaticMesh()) return;

	InstancedMeshComp->ClearInstances();

	for (int32 X = 0; X < GridX; ++X)
	{
		for (int32 Y = 0; Y < GridY; ++Y)
		{
			for (int32 Z = 0; Z < GridZ; ++Z)
			{
				// Calculates X, Y, and Z spacing
				FVector Location = FVector(X * SpacingX, Y * SpacingY, Z * SpacingZ);
                
				FTransform InstanceTransform(RotationOffset, Location, FVector::OneVector);
                
				// Add instance (bWorldSpace = false)
				InstancedMeshComp->AddInstance(InstanceTransform, false); 
			}
		}
	}
}