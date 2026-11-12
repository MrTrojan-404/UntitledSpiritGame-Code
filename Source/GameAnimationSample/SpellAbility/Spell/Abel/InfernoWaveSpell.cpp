#include "InfernoWaveSpell.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "Engine/OverlapResult.h"
#include "GameAnimationSample/Actor/EnvironmentInteractable/Explosives/ExplosiveBarrel.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/AI/Character/GASPAICharacterBase.h"
#include "GameAnimationSample/SpellAbility/SpellComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"

void UInfernoWaveSpell::Execute_Implementation(AGASPBaseCharacter* Caster)
{
	if (!Caster || !Caster->HasAuthority()) return;

	FVector Origin = Caster->GetActorLocation();
	FVector Forward = Caster->GetActorForwardVector();
	float ConeDot = FMath::Cos(FMath::DegreesToRadians(ConeHalfAngleDegrees));

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Caster);

	// --- 1. Add ECC_WorldDynamic to the overlap query ---
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	TArray<FOverlapResult> Overlaps;
	Caster->GetWorld()->OverlapMultiByObjectType(
		Overlaps, Origin, FQuat::Identity,
		ObjectParams,
		FCollisionShape::MakeSphere(ConeRange),
		Params
	);

	Caster->OnInfernoWave(Forward);

	// Creates a 20m distraction noise so enemies walk toward the fire!
	UAISense_Hearing::ReportNoiseEvent(Caster->GetWorld(), Origin, 1.f, Caster, 2000.f, FName("Distraction"));

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor) continue;

		FVector ToTarget = (HitActor->GetActorLocation() - Origin);
		ToTarget.Z = 0.f;
		FVector ToTargetDir = ToTarget.GetSafeNormal();

		if (FVector::DotProduct(Forward, ToTargetDir) < ConeDot) continue;

		// --- 2. Is it a Character? ---
		if (AGASPBaseCharacter* Target = Cast<AGASPBaseCharacter>(HitActor))
		{
			if (Target->IsPlayerAlly()) continue;
			
			UGameplayStatics::ApplyDamage(Target, Damage, Caster->GetController(), Caster, UDamageType::StaticClass());

			if (USpellComponent* SpellComp = Caster->FindComponentByClass<USpellComponent>())
			{
				SpellComp->OnDamageDealt(Damage);
			}

			bool bIsHeavy = Cast<AGASPAICharacterBase>(Target) ? Cast<AGASPAICharacterBase>(Target)->bIsHeavyEnemy : false;

			FVector LaunchDir = ToTargetDir + FVector(0.f, 0.f, 0.4f);
			LaunchDir.Normalize();

			if (bIsHeavy)
			{
				if (UMoverComponent* TargetMover = Target->FindComponentByClass<UMoverComponent>())
				{
					TSharedPtr<FLayeredMove_LinearVelocity> LaunchMove = MakeShared<FLayeredMove_LinearVelocity>();
					LaunchMove->Velocity = LaunchDir * (LaunchSpeed * HeavyStaggerSpeedMultiplier);
					LaunchMove->DurationMs = LaunchDurationMs;
					LaunchMove->MixMode = EMoveMixMode::OverrideVelocity;
					TargetMover->QueueLayeredMove(LaunchMove);
				}
				Target->OnStaggered(true);
			}
			else
			{
				Target->EnterTemporaryRagdoll(LaunchDir, RagdollImpulseStrength, RagdollRecoverDuration);
				Target->OnStaggered(false);
			}
		}
		// --- 3. Is it an Explosive Barrel? ---
		else if (AExplosiveBarrel* Barrel = Cast<AExplosiveBarrel>(HitActor))
		{
			// Damage it to light the fuse
			UGameplayStatics::ApplyDamage(Barrel, Damage, Caster->GetController(), Caster, UDamageType::StaticClass());

			// Punt it backward
			if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Barrel->GetRootComponent()))
			{
				FVector LaunchDir = ToTargetDir + FVector(0.f, 0.f, 0.4f);
				LaunchDir.Normalize();
				
				// 'true' uses a Velocity Change, making the punt consistent regardless of the barrel's mass
				Mesh->AddImpulse(LaunchDir * LaunchSpeed * 1.5f, NAME_None, true);
			}
		}
	}
}
