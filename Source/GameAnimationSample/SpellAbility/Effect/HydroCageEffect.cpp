#include "HydroCageEffect.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameAnimationSample/Actor/EnvironmentInteractable/Explosives/ExplosiveBarrel.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"
#include "GameAnimationSample/AI/Character/GASPAICharacterBase.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "Net/UnrealNetwork.h"

AHydroCageEffect::AHydroCageEffect()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

void AHydroCageEffect::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHydroCageEffect, bIsTrapped);
	DOREPLIFETIME(AHydroCageEffect, Caster);
	DOREPLIFETIME(AHydroCageEffect, Target);
}

void AHydroCageEffect::Initialize(AGASPBaseCharacter* InCaster, AActor* InTarget, float InDuration, float InDamagePerTick, float InTickInterval, float InPullSpeed)
{
	Caster = InCaster; Target = InTarget; Duration = InDuration; 
	DamagePerTick = InDamagePerTick; TickInterval = InTickInterval; PullSpeed = InPullSpeed;
}

void AHydroCageEffect::BeginPlay()
{
	Super::BeginPlay();
	
	if (!HasAuthority()) return;
	if (!Target) { Destroy(); return; }

	if (AGASPBaseCharacter* CharTarget = Cast<AGASPBaseCharacter>(Target))
	{
		if (AGASPAIController* AICon = Cast<AGASPAIController>(CharTarget->GetController()))
		{
			if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
				BB->SetValueAsBool(AGASPAIController::BB_bIsCaged, true);
		}
		CharTarget->OnCaged(true); 
	}

	ApplyPull();

	bIsTrapped = true;
	ApplyTrapVisualsAndPhysics(); // Server calls this manually

	GetWorld()->GetTimerManager().SetTimer(DamageTickTimer, this, &AHydroCageEffect::DamageTick, TickInterval, true);
	GetWorld()->GetTimerManager().SetTimer(ExpireTimer, this, &AHydroCageEffect::EndCage, Duration, false);
}

void AHydroCageEffect::ApplyPull()
{
	if (!Caster || !Target) return;

	if (AGASPBaseCharacter* CharTarget = Cast<AGASPBaseCharacter>(Target))
	{
		if (UCombatComponent* Combat = CharTarget->GetCombatComponent()) Combat->HandleFire(false);
		CharTarget->BeginRagdoll(FVector(0.f, 0.f, 1.f), 50.f); // Tiny impulse just to break them into ragdoll
	}
	else if (AExplosiveBarrel* Barrel = Cast<AExplosiveBarrel>(Target))
	{
		if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Barrel->GetRootComponent()))
		{
			Mesh->AddImpulse(FVector(0.f, 0.f, 1.f) * 50.f, NAME_None, true);
		}
	}
}

void AHydroCageEffect::OnRep_Trapped()
{
	if (bIsTrapped)
	{
		ApplyTrapVisualsAndPhysics(); // Client calls this via replication
	}
}

void AHydroCageEffect::ApplyTrapVisualsAndPhysics()
{
	if (!Target) return;

	if (AGASPBaseCharacter* CharTarget = Cast<AGASPBaseCharacter>(Target))
	{
		if (USkeletalMeshComponent* Mesh = CharTarget->CharacterMesh)
		{
			Mesh->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
			Mesh->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

			Mesh->SetEnableGravity(false);
			
			// Heavy damping stops them from flying away or drifting
			Mesh->SetLinearDamping(4.0f);
			Mesh->SetAngularDamping(2.0f);

			// Lock their hover height strictly to their exact current position + 30 units
			LocalHoverZ = Mesh->GetSocketLocation(FName("pelvis")).Z + 30.f;

			if (WaterDomeVFX)
			{
				ActiveDomeVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
					WaterDomeVFX, Mesh, FName("pelvis"), FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true
				);
			}
			if (LoopSound)
			{
				ActiveLoopSound = UGameplayStatics::SpawnSoundAttached(LoopSound, Mesh, FName("pelvis"));
			}
		}
	}
	else if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Target->GetRootComponent()))
	{
		PrimComp->SetEnableGravity(false);
		PrimComp->SetLinearDamping(4.0f);
		PrimComp->SetAngularDamping(2.0f);

		LocalHoverZ = PrimComp->GetComponentLocation().Z + 30.f;

		if (WaterDomeVFX)
		{
			ActiveDomeVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
				WaterDomeVFX, PrimComp, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true
			);
		}
		if (LoopSound)
		{
			ActiveLoopSound = UGameplayStatics::SpawnSoundAttached(LoopSound, PrimComp);
		}
	}
}

void AHydroCageEffect::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!Target || !bIsTrapped) return;

	// SINE WAVE: Smoothly bobs 15 units up and down 
	float Time = GetWorld()->GetTimeSeconds();
	float SineOffset = FMath::Sin(Time * 3.0f) * 15.f; 

	if (AGASPBaseCharacter* CharTarget = Cast<AGASPBaseCharacter>(Target))
	{
		if (USkeletalMeshComponent* Mesh = CharTarget->CharacterMesh)
		{
			if (!Mesh->IsSimulatingPhysics()) return;

			FVector PelvisLoc = Mesh->GetSocketLocation(FName("pelvis"));
			FVector CurrentVelocity = Mesh->GetPhysicsLinearVelocity(FName("pelvis"));

			float TargetZ = LocalHoverZ + SineOffset;
			float DeltaZ = TargetZ - PelvisLoc.Z;

			float AccelZ = (DeltaZ * SpringStrength) - (CurrentVelocity.Z * SpringDamping);
			
			// Clamp to a slow maximum speed (150) so they never rocket upwards
			float DesiredVelZ = FMath::Clamp(CurrentVelocity.Z + (AccelZ * DeltaTime), -150.f, 150.f);

			// Bleed off any horizontal sliding
			FVector HorizontalVelocity = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0.f) * FMath::Max(0.f, 1.f - (SpringDamping * DeltaTime));

			Mesh->SetPhysicsLinearVelocity(FVector(HorizontalVelocity.X, HorizontalVelocity.Y, DesiredVelZ), false, FName("pelvis"));
		}
	}
	else if (AExplosiveBarrel* Barrel = Cast<AExplosiveBarrel>(Target))
	{
		if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Barrel->GetRootComponent()))
		{
			float TargetZ = LocalHoverZ + SineOffset;
			FVector ToHover(0.f, 0.f, TargetZ - Mesh->GetComponentLocation().Z);
			Mesh->SetPhysicsLinearVelocity(ToHover.GetClampedToMaxSize(150.f));
		}
	}
}

void AHydroCageEffect::DamageTick()
{
	if (AGASPBaseCharacter* CharTarget = Cast<AGASPBaseCharacter>(Target))
	{
		if (Cast<AGASPAICharacterBase>(CharTarget) && !Cast<AGASPAICharacterBase>(CharTarget)->IsAlive())
		{
			EndCage(); return;
		}
		UGameplayStatics::ApplyDamage(Target, DamagePerTick, Caster ? Caster->GetController() : nullptr, Caster, UDamageType::StaticClass());
	}
}

void AHydroCageEffect::EndCage()
{
	GetWorld()->GetTimerManager().ClearTimer(DamageTickTimer);
	GetWorld()->GetTimerManager().ClearTimer(ExpireTimer);

	if (AGASPBaseCharacter* CharTarget = Cast<AGASPBaseCharacter>(Target))
	{
		if (AGASPAIController* AICon = Cast<AGASPAIController>(CharTarget->GetController()))
		{
			if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
				BB->SetValueAsBool(AGASPAIController::BB_bIsCaged, false);
		}
		CharTarget->OnCaged(false);

		if (HasAuthority())
		{
			CharTarget->RecoverFromRagdoll();
		}
	}

	Multicast_ReleaseTarget();
	Destroy();
}

void AHydroCageEffect::Multicast_ReleaseTarget_Implementation()
{
	if (!Target) return;

	if (ActiveDomeVFX) ActiveDomeVFX->Deactivate();
	if (ActiveLoopSound) ActiveLoopSound->Stop();

	if (AGASPBaseCharacter* CharTarget = Cast<AGASPBaseCharacter>(Target))
	{
		if (WaterBurstVFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), WaterBurstVFX, CharTarget->CharacterMesh->GetSocketLocation(FName("pelvis")));
		}
		if (BurstSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, BurstSound, CharTarget->CharacterMesh->GetSocketLocation(FName("pelvis")));
		}

		if (USkeletalMeshComponent* Mesh = CharTarget->CharacterMesh)
		{
			Mesh->SetEnableGravity(true);
			Mesh->SetLinearDamping(0.01f);
			Mesh->SetAngularDamping(0.0f);
		}
	}
	else if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Target->GetRootComponent()))
	{
		if (WaterBurstVFX) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), WaterBurstVFX, PrimComp->GetComponentLocation());
		if (BurstSound) UGameplayStatics::PlaySoundAtLocation(this, BurstSound, PrimComp->GetComponentLocation());

		PrimComp->SetEnableGravity(true);
		PrimComp->SetLinearDamping(0.01f);
		PrimComp->SetAngularDamping(0.0f);
	}
}