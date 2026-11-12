#include "ArcDartProjectile.h"

#include "BrainComponent.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "Components/BoxComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameAnimationSample/AI/Character/GASPAICharacterBase.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"

AArcDartProjectile::AArcDartProjectile()
{
	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	
	// High speed, no gravity for a straight, fast dart shot
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	ProjectileMovementComponent->InitialSpeed = 8000.f;
	ProjectileMovementComponent->MaxSpeed = 8000.f;
	ProjectileMovementComponent->ProjectileGravityScale = 0.f; 

	// Set the base class DestroyTime instead of using SetLifeSpan
	DestroyTime = 3.f; 
}

void AArcDartProjectile::Initialize(AGASPBaseCharacter* InCaster)
{
	Caster = InCaster;

	if (Caster && CollisionBox)
	{
		CollisionBox->IgnoreActorWhenMoving(Caster, true);

		// ADD THIS: Ignore her equipped weapon so it doesn't instantly detonate!
		TArray<AActor*> AttachedActors;
		Caster->GetAttachedActors(AttachedActors);
		for (AActor* AttachedActor : AttachedActors)
		{
			CollisionBox->IgnoreActorWhenMoving(AttachedActor, true);
		}
	}
}

void AArcDartProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	if (ArcTrailSystem)
	{
		ActiveTrail = UNiagaraFunctionLibrary::SpawnSystemAttached(
			ArcTrailSystem, 
			RootComponent, 
			NAME_None, 
			FVector::ZeroVector, 
			FRotator(0.f, 180.f, 0.f),
			EAttachLocation::KeepRelativeOffset, 
			true
		);
	}
	// EVERYONE plays the sound locally when the dart spawns
	if (LaunchSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LaunchSound, GetActorLocation());
	}
}

void AArcDartProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority()) return;
	if (OtherActor == Caster || OtherActor == this) return;

	// 1. 1-Hit Kill Animation for Standard Enemies
	AGASPAICharacterBase* AITarget = Cast<AGASPAICharacterBase>(OtherActor);
	if (AITarget && !AITarget->bIsHeavyEnemy && !AITarget->IsPlayerAlly())
	{
		// Pass both the VFX and the Sound into the kill function
		AITarget->TriggerElectrocutionDeath(Caster, ElectrocuteVFX, ZapSound);
		Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
		return;
	}

	// 2. Fallback: Heavies, Bosses, or Players take standard damage & 6-second stun
	if (AGASPBaseCharacter* Target = Cast<AGASPBaseCharacter>(OtherActor))
	{
		if (UCombatComponent* Combat = Target->GetCombatComponent())
		{
			Combat->HandleFire(false);
		}

		UGameplayStatics::ApplyDamage(Target, Damage, Caster ? Caster->GetController() : nullptr, Caster, UDamageType::StaticClass());
		Target->OnStaggered(false);

		// If it is a Heavy AI (or Ally AI), play the sparks & sound for the full 6 seconds!
		if (AITarget)
		{
			AITarget->Multicast_PlayElectrocuteFX(ElectrocuteVFX, ZapSound, StunDuration, false);

			// Tell the Heavy's brain they are stunned!
			if (AGASPAIController* AIC = Cast<AGASPAIController>(AITarget->GetController()))
			{
				if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
				{
					BB->SetValueAsBool(AGASPAIController::BB_bIsElectrocuted, true);
				}
				
				// Force them to drop whatever they were doing
				//if (AIC->GetBrainComponent()) AIC->GetBrainComponent()->RestartLogic();
			}
		}
		else if (ZapSound) 
		{
			// Human player fallback
			UGameplayStatics::PlaySoundAtLocation(this, ZapSound, GetActorLocation());
		}
		// Broadcast a short-range noise (e.g., 600 units) so nearby AI hear the zap and investigate!
		UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), 1.0f, Caster, 600.f, FName("Distraction"));
		
		// Lock their movement for 6 seconds
		if (UMoverComponent* TargetMover = Target->FindComponentByClass<UMoverComponent>())
		{
			TSharedPtr<FLayeredMove_LinearVelocity> TaseMove = MakeShared<FLayeredMove_LinearVelocity>();
			TaseMove->Velocity = FVector::ZeroVector;
			TaseMove->DurationMs = StunDuration * 1000.f;
			TaseMove->MixMode = EMoveMixMode::OverrideVelocity;
			TargetMover->QueueLayeredMove(TaseMove);
		}
	}

	Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
}

void AArcDartProjectile::Destroyed()
{
	// Detach the trail so it doesn't pop out of existence when the dart is deleted
	if (ActiveTrail)
	{
		ActiveTrail->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		ActiveTrail->Deactivate();
	}

	Super::Destroyed();
}