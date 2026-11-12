#include "GravityGrenade.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/OverlapResult.h"

AGravityGrenade::AGravityGrenade()
{
	// Optional: You can override the grenade's initial speed here so it doesn't fly too fast
	// ProjectileMovementComponent->InitialSpeed = 1000.f;
	// ProjectileMovementComponent->MaxSpeed = 1000.f;
}

void AGravityGrenade::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bExplodeOnImpact)
	{
		OnDetonate(); 
		bExplodeOnImpact = false; 
	}
}

// ==========================================
// PHASE 1: VORTEX START
// ==========================================
void AGravityGrenade::OnDetonate()
{
	// 1. Freeze the grenade in place and hide the physical mesh
	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->StopMovementImmediately();
		ProjectileMovementComponent->SetActive(false);
	}
	if (CollisionBox)
	{
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (ProjectileMesh)
	{
		ProjectileMesh->SetVisibility(false);
	}

	// 2. Spawn the looping Vortex VFX and Sound
	if (VortexVFX)
	{
		ActiveVortexVFX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VortexVFX, GetActorLocation());
	}
	if (VortexSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, VortexSound, GetActorLocation());
	}

	// --- Vortex Startup Noise ---
	// Alert nearby guards (15 meters) that the black hole just opened!
	if (HasAuthority())
	{
		UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), 1.f, GetInstigator(), 1500.f, FName("Distraction"));
	}

	// 3. Start the continuous pull & DoT timer
	GetWorldTimerManager().SetTimer(VortexTimerHandle, this, &AGravityGrenade::VortexTick, VortexTickRate, true);
	
	// 4. Schedule Phase 2 (Detonation)
	GetWorldTimerManager().SetTimer(DetonationTimerHandle, this, &AGravityGrenade::DetonateVortex, VortexDuration, false);
}

void AGravityGrenade::SetStasis(bool bInStasis, float DilationFactor)
{
	Super::SetStasis(bInStasis, DilationFactor);

	if (bInStasis)
	{
		GetWorldTimerManager().PauseTimer(VortexTimerHandle);
		GetWorldTimerManager().PauseTimer(DetonationTimerHandle);
	}
	else
	{
		GetWorldTimerManager().UnPauseTimer(VortexTimerHandle);
		GetWorldTimerManager().UnPauseTimer(DetonationTimerHandle);
	}
}

// ==========================================
// THE CONTINUOUS PULL (Runs every 0.1 seconds)
// ==========================================
void AGravityGrenade::VortexTick()
{
    FVector Epicenter = GetActorLocation();
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(GetInstigator());

    // 1. Create a set to track who has already taken damage this specific tick
    TSet<AActor*> DamagedActorsThisTick;

    // ==========================================
    // A. FIND CHARACTERS & RAGDOLL THEM
    // ==========================================
    TArray<FOverlapResult> PawnOverlaps;
    GetWorld()->OverlapMultiByObjectType(
       PawnOverlaps, Epicenter, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(PullRadius), Params
    );

    for (const FOverlapResult& Overlap : PawnOverlaps)
    {
       AGASPBaseCharacter* Target = Cast<AGASPBaseCharacter>(Overlap.GetActor());
       if (Target && !Target->IsPlayerAlly())
       {
          // 2. Check the set before applying damage
          if (!DamagedActorsThisTick.Contains(Target))
          {
              UGameplayStatics::ApplyDamage(Target, DoTDamage, GetInstigatorController(), this, UDamageType::StaticClass());
              DamagedActorsThisTick.Add(Target); // Mark as damaged
          }

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

   // ==========================================
    // B. PULL ALL PHYSICS OBJECTS (Ragdolls + Barrels)
    // ==========================================
    TArray<FOverlapResult> PhysicsOverlaps;
    
    FCollisionObjectQueryParams PhysicsObjectParams;
    PhysicsObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    PhysicsObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody); 

    GetWorld()->OverlapMultiByObjectType(
       PhysicsOverlaps, Epicenter, FQuat::Identity, PhysicsObjectParams, FCollisionShape::MakeSphere(PullRadius), Params
    );

    for (const FOverlapResult& Overlap : PhysicsOverlaps)
    {
       if (UPrimitiveComponent* PrimComp = Overlap.GetComponent())
       {
          if (PrimComp->IsSimulatingPhysics())
          {
             AActor* HitActor = Overlap.GetActor();

             // 1. Is it a Character's Ragdoll?
             if (HitActor && HitActor->IsA(AGASPBaseCharacter::StaticClass()))
             {
                 // Ragdolls have 15+ physics bodies (bones). Scale the force down drastically
                 // so their joints don't fight each other and cause violent shaking.
                 PrimComp->AddRadialImpulse(Epicenter, PullRadius, -(PhysicsPullForce * 0.05f), ERadialImpulseFalloff::RIF_Constant, true);
             }
             // 2. It's a standard prop (like your barrel)
             else
             {
                 // Apply the full force.
                 PrimComp->AddRadialImpulse(Epicenter, PullRadius, -PhysicsPullForce, ERadialImpulseFalloff::RIF_Constant, true);
             }
          }
          
          if (AActor* HitActor = Overlap.GetActor())
          {
              if (HitActor->IsA(AGASPBaseCharacter::StaticClass()))
              {
                  // 3. Check the set AGAIN for the ragdoll loop
                  if (!DamagedActorsThisTick.Contains(HitActor))
                  {
                      UGameplayStatics::ApplyDamage(HitActor, DoTDamage, GetInstigatorController(), this, UDamageType::StaticClass());
                      DamagedActorsThisTick.Add(HitActor); // Mark as damaged
                  }
              }
          }
       }
    }
}

// ==========================================
// PHASE 2: DETONATION TRIGGER
// ==========================================
void AGravityGrenade::DetonateVortex()
{
	// Clear the Phase 1 tick timer so it stops pulling
	GetWorldTimerManager().ClearTimer(VortexTimerHandle);

	if (ActiveVortexVFX)
	{
		ActiveVortexVFX->Deactivate();
	}

	// This removes the actor, which automatically calls AProjectileGrenade::Destroyed(), triggering ExplodeDamage()
	Destroy(); 
}

// ==========================================
// PHASE 2: FINAL EXPLOSION
// ==========================================
void AGravityGrenade::ExplodeDamage()
{
	FVector Epicenter = GetActorLocation();

	// 1. Play Final Burst Visuals & Audio
	if (DetonationVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DetonationVFX, Epicenter);
	}
	if (DetonationSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DetonationSound, Epicenter);
	}

	// 2. Massive Burst Damage to everything clustered in the center
	TArray<AActor*> IgnoredActors;
	IgnoredActors.Add(this);
	IgnoredActors.Add(GetInstigator());

	UGameplayStatics::ApplyRadialDamage(
		GetWorld(), 
		FinalBurstDamage, 
		Epicenter, 
		PullRadius, 
		UDamageType::StaticClass(), 
		IgnoredActors, 
		this, 
		GetInstigatorController(), 
		false, 
		ECC_Visibility
	);

	// 3. Alert AI
	UAISense_Hearing::ReportNoiseEvent(GetWorld(), Epicenter, 1.f, GetInstigator(), 3000.f, FName("GravityDetonation"));

	// Release everyone the vortex caught. Without this they'd stay ragdolled forever.
	for (TWeakObjectPtr<AGASPBaseCharacter> TargetPtr : RagdolledTargets)
	{
		if (AGASPBaseCharacter* Target = TargetPtr.Get())
		{
			Target->RecoverFromRagdoll();
		}
	}
	RagdolledTargets.Empty();
}