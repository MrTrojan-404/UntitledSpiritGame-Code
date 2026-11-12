#include "IgnitionEffect.h"

#include "Engine/OverlapResult.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/AI/Character/GASPAICharacterBase.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"

AIgnitionEffect::AIgnitionEffect()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

void AIgnitionEffect::Initialize(AGASPBaseCharacter* InCaster, AGASPBaseCharacter* InTarget, float InDamagePerTick, float InTickInterval, float InExplosionDamage, float InExplosionRadius)
{
	Caster = InCaster;
	Target = InTarget;
	DamagePerTick = InDamagePerTick;
	TickInterval = InTickInterval;
	ExplosionDamage = InExplosionDamage;
	ExplosionRadius = InExplosionRadius;
}

void AIgnitionEffect::BeginPlay()
{
	Super::BeginPlay();

	if (!IsValid(Target) || !HasAuthority()) 
	{
		Destroy();
		return;
	}

	AttachToActor(Target, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	GetWorld()->GetTimerManager().SetTimer(DamageTickTimer, this, &AIgnitionEffect::DamageTick, TickInterval, true);
}

void AIgnitionEffect::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority()) return;

	// FALLBACK: If the enemy actor was destroyed entirely, detonate instantly.
	if (!IsValid(Target))
	{
		Explode();
		return;
	}

	// STANDARD: Check if they died from external damage (e.g., being shot)
	if (AGASPAICharacterBase* AITarget = Cast<AGASPAICharacterBase>(Target))
	{
		if (!AITarget->IsAlive())
		{
			Explode();
		}
	}
}

void AIgnitionEffect::DamageTick()
{
	// If the target is missing, detonate
	if (!IsValid(Target)) 
	{
		Explode();
		return;
	}

	// Apply the burn damage
	UGameplayStatics::ApplyDamage(
		Target, 
		DamagePerTick, 
		Caster ? Caster->GetController() : nullptr, 
		Caster, 
		UDamageType::StaticClass()
	);

	// IMMEDIATELY check if that burn tick was the killing blow
	if (AGASPAICharacterBase* AITarget = Cast<AGASPAICharacterBase>(Target))
	{
		if (!AITarget->IsAlive())
		{
			Explode();
		}
	}
}

void AIgnitionEffect::Explode()
{
	// Prevent double detonation if Tick and DamageTick both catch the death
	if (bHasExploded) return;
	bHasExploded = true;

	SetActorTickEnabled(false);
	GetWorld()->GetTimerManager().ClearTimer(DamageTickTimer);

	// Get location and raise it slightly so the blast isn't buried in the floor
	FVector ExplosionLocation = IsValid(Target) ? Target->GetActorLocation() : GetActorLocation();
	ExplosionLocation.Z += 50.f;

	// Massive combat explosion noise
	UAISense_Hearing::ReportNoiseEvent(GetWorld(), ExplosionLocation, 1.f, Caster, 4000.f, FName("Explosion"));
	
	// Set up our sweep parameters
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (IsValid(Target)) Params.AddIgnoredActor(Target); // Don't damage the dead body
	if (Caster) Params.AddIgnoredActor(Caster); // Don't blow up Abel

	TSet<AActor*> ProcessedActors; // Tracks who we hit so we don't double-damage overlapping hitboxes

	// ==========================================
	// 1. SWEEP FOR ENEMIES (DAMAGE & SPREAD BURN)
	// ==========================================
	TArray<FOverlapResult> PawnOverlaps;
	GetWorld()->OverlapMultiByObjectType(
		PawnOverlaps, 
		ExplosionLocation, 
		FQuat::Identity, 
		FCollisionObjectQueryParams(ECC_Pawn), 
		FCollisionShape::MakeSphere(ExplosionRadius), 
		Params
	);

	for (const FOverlapResult& Overlap : PawnOverlaps)
	{
		AGASPBaseCharacter* HitChar = Cast<AGASPBaseCharacter>(Overlap.GetActor());
		
		if (HitChar && !HitChar->IsPlayerAlly() && !ProcessedActors.Contains(HitChar))
		{
			ProcessedActors.Add(HitChar);

			// A. Apply the instant blast damage
			UGameplayStatics::ApplyDamage(HitChar, ExplosionDamage, Caster ? Caster->GetController() : nullptr, Caster, UDamageType::StaticClass());

			// B. SPREAD THE CONTAGION (If they survived the initial blast)
			if (AGASPAICharacterBase* AIHit = Cast<AGASPAICharacterBase>(HitChar))
			{
				if (AIHit->IsAlive())
				{
					// Check if they are already burning so we don't stack infinite DOTs
					bool bAlreadyBurning = false;
					TArray<AActor*> AttachedActors;
					HitChar->GetAttachedActors(AttachedActors);
					
					for (AActor* Attached : AttachedActors)
					{
						if (Attached->IsA(AIgnitionEffect::StaticClass()))
						{
							bAlreadyBurning = true;
							break;
						}
					}

					// If they aren't burning, infect them!
					if (!bAlreadyBurning)
					{
						FTransform SpawnTransform(FRotator::ZeroRotator, HitChar->GetActorLocation());
						
						// GetClass() allows the effect to clone itself without needing headers
						AIgnitionEffect* NewEffect = GetWorld()->SpawnActorDeferred<AIgnitionEffect>(GetClass(), SpawnTransform);
						
						if (NewEffect)
						{
							NewEffect->Initialize(Caster, HitChar, DamagePerTick, TickInterval, ExplosionDamage, ExplosionRadius);
							NewEffect->FinishSpawning(SpawnTransform);
						}
					}
				}
			}
		}
	}

	// ==========================================
	// 2. SWEEP FOR PHYSICS PROPS & BARRELS
	// ==========================================
	TArray<FOverlapResult> PhysicsOverlaps;
	FCollisionObjectQueryParams PhysicsObjectParams;
	PhysicsObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	PhysicsObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	GetWorld()->OverlapMultiByObjectType(
		PhysicsOverlaps, 
		ExplosionLocation, 
		FQuat::Identity, 
		PhysicsObjectParams, 
		FCollisionShape::MakeSphere(ExplosionRadius), 
		Params
	);

	for (const FOverlapResult& Overlap : PhysicsOverlaps)
	{
		// Apply physical knockback to props
		if (UPrimitiveComponent* PrimComp = Overlap.GetComponent())
		{
			if (PrimComp->IsSimulatingPhysics())
			{
				// Multiply damage to get a noticeable physics impulse
				PrimComp->AddRadialImpulse(ExplosionLocation, ExplosionRadius, ExplosionDamage * 10.f, ERadialImpulseFalloff::RIF_Constant, true);
			}
		}

		// Apply damage to props (like explosive barrels to chain-react them too)
		AActor* HitActor = Overlap.GetActor();
		if (HitActor && !ProcessedActors.Contains(HitActor) && HitActor != Target)
		{
			ProcessedActors.Add(HitActor);
			UGameplayStatics::ApplyDamage(HitActor, ExplosionDamage, Caster ? Caster->GetController() : nullptr, Caster, UDamageType::StaticClass());
		}
	}

	// TODO: Spawn Explosion VFX and SFX here

	Destroy();
}