#include "DarkSunOrb.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"
#include "GameAnimationSample/AI/Character/GASPAICharacterBase.h"
#include "Perception/AISense_Hearing.h"

ADarkSunOrb::ADarkSunOrb()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent = CollisionSphere;
	CollisionSphere->SetSphereRadius(50.f);
	CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	
	// Dark Sun is a slow-moving orb that drifts through crowds
	ProjectileMovement->InitialSpeed = 300.f;
	ProjectileMovement->MaxSpeed = 300.f;
	ProjectileMovement->ProjectileGravityScale = 0.f; // Zero gravity so it floats straight
}

void ADarkSunOrb::Initialize(AGASPBaseCharacter* InCaster)
{
	Caster = InCaster;
}

void ADarkSunOrb::BeginPlay()
{
	Super::BeginPlay();

	// EVERYONE starts the continuous pull/physics loop
	GetWorldTimerManager().SetTimer(BurnTimerHandle, this, &ADarkSunOrb::ApplyGravityPull, TickInterval, true);
	
	// ONLY THE SERVER schedules the final detonation (so damage isn't applied twice)
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(DetonationTimerHandle, this, &ADarkSunOrb::Detonate, OrbLifetime, false);
	}
}

void ADarkSunOrb::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ADarkSunOrb::ApplyGravityPull()
{
	FVector Epicenter = GetActorLocation();
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (Caster) Params.AddIgnoredActor(Caster); 

	TSet<AActor*> DamagedActorsThisTick;

	// ==========================================
	// 1. FIND CHARACTERS (SERVER ONLY)
	// Only the Server applies damage, slides Heavies, and triggers Ragdolls
	// ==========================================
	if (HasAuthority())
	{
		TArray<FOverlapResult> PawnOverlaps;
		GetWorld()->OverlapMultiByObjectType(
			PawnOverlaps, Epicenter, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(GravityRadius), Params
		);

		for (const FOverlapResult& Overlap : PawnOverlaps)
		{
			AGASPBaseCharacter* Target = Cast<AGASPBaseCharacter>(Overlap.GetActor());
			if (Target && !Target->IsPlayerAlly())
			{
				// A. Apply DOT Burn
				if (!DamagedActorsThisTick.Contains(Target))
				{
					UGameplayStatics::ApplyDamage(Target, BurnDamagePerTick, Caster ? Caster->GetController() : nullptr, this, UDamageType::StaticClass());
					DamagedActorsThisTick.Add(Target);
				}

				// B. Check if they are a Boss/Heavy
				bool bIsHeavy = false;
				if (AGASPAICharacterBase* AITarget = Cast<AGASPAICharacterBase>(Target))
				{
					bIsHeavy = AITarget->bIsHeavyEnemy;
				}

				if (bIsHeavy)
				{
					// --- HEAVY ENEMY: Slide them on their feet ---
					FVector PullDir = (Epicenter - Target->GetActorLocation());
					PullDir.Z = 0.f; 
					PullDir.Normalize();

					if (UMoverComponent* TargetMover = Target->FindComponentByClass<UMoverComponent>())
					{
						TSharedPtr<FLayeredMove_LinearVelocity> PullMove = MakeShared<FLayeredMove_LinearVelocity>();
						PullMove->Velocity = PullDir * PullStrength;
						PullMove->DurationMs = (TickInterval * 1000.f) + 50.f; 
						PullMove->MixMode = EMoveMixMode::OverrideVelocity;
						TargetMover->QueueLayeredMove(PullMove);
					}
				}
				else
				{
					// --- LIGHT ENEMY: Trigger the ragdoll state ---
					bool bAlreadyRagdolled = RagdolledTargets.ContainsByPredicate(
						[Target](const TWeakObjectPtr<AGASPBaseCharacter>& P) { return P.Get() == Target; });

					if (!bAlreadyRagdolled)
					{
						FVector PullDir = (Epicenter - Target->GetActorLocation()).GetSafeNormal();
						Target->BeginRagdoll(PullDir, 300.f);
						RagdolledTargets.Add(Target);
					}
				}
			}
		}
	}

	// ==========================================
	// 2. PULL PHYSICS OBJECTS (EVERYONE RUNS THIS)
	// Server and Client both apply local physics impulses to the ragdolls!
	// ==========================================
	TArray<FOverlapResult> PhysicsOverlaps;
	FCollisionObjectQueryParams PhysicsObjectParams;
	PhysicsObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	PhysicsObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	GetWorld()->OverlapMultiByObjectType(
		PhysicsOverlaps, Epicenter, FQuat::Identity, PhysicsObjectParams, FCollisionShape::MakeSphere(GravityRadius), Params
	);

	for (const FOverlapResult& Overlap : PhysicsOverlaps)
	{
		if (UPrimitiveComponent* PrimComp = Overlap.GetComponent())
		{
			if (PrimComp->IsSimulatingPhysics())
			{
				AActor* HitActor = Overlap.GetActor();

				// 1. Is it a Light Character's Ragdoll?
				if (HitActor && HitActor->IsA(AGASPBaseCharacter::StaticClass()))
				{
					PrimComp->AddRadialImpulse(Epicenter, GravityRadius, -(PullStrength * 0.05f), ERadialImpulseFalloff::RIF_Constant, true);
				}
				// 2. It's a standard prop (like a barrel)
				else
				{
					PrimComp->AddRadialImpulse(Epicenter, GravityRadius, -PullStrength, ERadialImpulseFalloff::RIF_Constant, true);
				}
			}
		}
	}
}

void ADarkSunOrb::Detonate()
{
	// Stop pulling and burning
	GetWorldTimerManager().ClearTimer(BurnTimerHandle);

	FVector Epicenter = GetActorLocation();

	TArray<AActor*> IgnoredActors;
	IgnoredActors.Add(this);
	if (Caster) IgnoredActors.Add(Caster);

	// Massive final blast
	UGameplayStatics::ApplyRadialDamage(
		GetWorld(), 
		ExplosionDamage, 
		Epicenter, 
		ExplosionRadius, 
		UDamageType::StaticClass(), 
		IgnoredActors, 
		this, 
		Caster ? Caster->GetController() : nullptr, 
		false, 
		ECC_Visibility
	);
	// The ultimate explosion noise - 60 meters!
	UAISense_Hearing::ReportNoiseEvent(GetWorld(), Epicenter, 1.f, Caster, 6000.f, FName("Explosion"));

	// TODO: Spawn Final Detonation VFX/SFX here

	// RELEASE SURVIVORS
	for (TWeakObjectPtr<AGASPBaseCharacter> TargetPtr : RagdolledTargets)
	{
		if (AGASPBaseCharacter* Target = TargetPtr.Get())
		{
			Target->RecoverFromRagdoll();
		}
	}
	RagdolledTargets.Empty();

	Destroy();
}