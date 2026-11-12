#include "GASPStageShield.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"

AGASPStageShield::AGASPStageShield()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	ShieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
	RootComponent = ShieldMesh;
	
	ShieldMesh->SetCollisionObjectType(ECC_WorldDynamic);
	ShieldMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ShieldMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ShieldMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void AGASPStageShield::BeginPlay()
{
	Super::BeginPlay();

	// Create the Dynamic Material so we can alter the Dissolve scalar
	if (ShieldMesh)
	{
		DynamicShieldMat = ShieldMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynamicShieldMat)
		{
			DynamicShieldMat->SetScalarParameterValue(TEXT("Dissolve"), CurrentDissolve);
		}
	}
}

void AGASPStageShield::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// If our current visual doesn't match our target, smoothly interpolate it!
	if (DynamicShieldMat && CurrentDissolve != TargetDissolve)
	{
		// InterpConstantTo creates a perfectly linear fade. 
		// Speed of 1.0f means it takes exactly 1.0 seconds to go from -0.5 to 0.5.
		CurrentDissolve = FMath::FInterpConstantTo(CurrentDissolve, TargetDissolve, DeltaTime, 1.0f);
		DynamicShieldMat->SetScalarParameterValue(TEXT("Dissolve"), CurrentDissolve);

		// Once fully invisible, completely hide the mesh to save rendering cost
		if (CurrentDissolve >= 0.5f && ShieldMesh->IsVisible())
		{
			ShieldMesh->SetVisibility(false);
		}
	}
}

void AGASPStageShield::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGASPStageShield, bIsActive);
}

void AGASPStageShield::SetShieldActive(bool bActive)
{
	if (!HasAuthority()) return;
	bIsActive = bActive;
	OnRep_IsActive(); 
}

void AGASPStageShield::OnRep_IsActive()
{
	if (bIsActive)
	{
		TargetDissolve = -0.5f; // Solid
		ShieldMesh->SetVisibility(true);
		ShieldMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else
	{
		TargetDissolve = 0.5f; // Dissolved
		// Instantly turn off collision so players can run in, but leave visibility ON so the Tick can fade it out!
		ShieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); 
	}
}

float AGASPStageShield::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (HasAuthority() && bIsActive)
	{
		FVector HitLocation = GetActorLocation();
		FVector HitNormal = GetActorUpVector();

		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			const FPointDamageEvent* PointDamageEvent = static_cast<const FPointDamageEvent*>(&DamageEvent);
			HitLocation = PointDamageEvent->HitInfo.ImpactPoint;
			
			HitNormal = PointDamageEvent->HitInfo.ImpactNormal;

			// 2. If the normal is pointing in the exact same direction the fire/bullet 
			// is traveling, the physics engine gave us an inverted backface. Flip it!
			if (FVector::DotProduct(HitNormal, PointDamageEvent->ShotDirection) > 0.f)
			{
				HitNormal = -HitNormal;
			}
		}
		else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
		{
			const FRadialDamageEvent* RadialDamageEvent = static_cast<const FRadialDamageEvent*>(&DamageEvent);
			HitLocation = RadialDamageEvent->Origin; 
			HitNormal = (GetActorLocation() - RadialDamageEvent->Origin).GetSafeNormal();
		}
		else
		{
			// === THE FLAMETHROWER FIX ===
			// If it's generic damage (Overlap), find the exact point on the shield closest to the fire!
			if (DamageCauser)
			{
				ShieldMesh->GetClosestPointOnCollision(DamageCauser->GetActorLocation(), HitLocation);
				HitNormal = (DamageCauser->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			}
		}

		Multicast_PlayImpactVFX(HitLocation, HitNormal);
	}
	return 0.f;
}

void AGASPStageShield::Multicast_PlayImpactVFX_Implementation(FVector ImpactLocation, FVector ImpactNormal)
{
	if (ImpactVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactVFX, ImpactLocation, ImpactNormal.Rotation());
	}
}