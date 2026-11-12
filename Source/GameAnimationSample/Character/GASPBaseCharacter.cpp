// Fill out your copyright notice in the Description page of Project Settings.


#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BodyInstance.h"
#include "MotionWarpingComponent.h"
#include "MoverComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "GameAnimationSample/GameAnimationSample.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"
#include "GameAnimationSample/Animation/BaseAnimInstance.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "GameAnimationSample/Component/DashComponent.h"
#include "GameAnimationSample/Component/DownedComponent.h"
#include "GameAnimationSample/Component/InteractionComponent.h"
#include "GameAnimationSample/Component/WallRunComponent.h"
#include "GameAnimationSample/GameMode/GASPArenaGameMode.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "GameAnimationSample/SpellAbility/SpellComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

static TAutoConsoleVariable<int32> CVarDrawMannyBodies(
	TEXT("gasp.DrawMannyBodies"), 0, TEXT("Draw Manny physics asset bodies"));

AGASPBaseCharacter::AGASPBaseCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	

	
	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	CombatComponent->SetIsReplicated(true);
	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
	WallRunComponent = CreateDefaultSubobject<UWallRunComponent>(TEXT("WallRunComponent"));
	DashComponent = CreateDefaultSubobject<UDashComponent>(TEXT("DashComponent"));
	SpellComponent = CreateDefaultSubobject<USpellComponent>(TEXT("SpellComponent"));
	
	DissolveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveTimelineComponent"));

	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	SetNetUpdateFrequency(66.f);
	SetMinNetUpdateFrequency(33.f);

}

void AGASPBaseCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGASPBaseCharacter, Health);
	DOREPLIFETIME(AGASPBaseCharacter, bDisableGameplay);
	DOREPLIFETIME(AGASPBaseCharacter, CharacterName);
	DOREPLIFETIME(AGASPBaseCharacter, MedkitCount);
}

void AGASPBaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (DissolveMaterialInstance01 && !DynamicDissolveMaterialInstance01)
	{
		DynamicDissolveMaterialInstance01 = UMaterialInstanceDynamic::Create(DissolveMaterialInstance01, this);
		DynamicDissolveMaterialInstance02 = UMaterialInstanceDynamic::Create(DissolveMaterialInstance02, this);
		
		if (CharacterMesh)
		{
			CharacterMesh->SetMaterial(0, DynamicDissolveMaterialInstance01);
			CharacterMesh->SetMaterial(1, DynamicDissolveMaterialInstance02);
		}
		
		DynamicDissolveMaterialInstance01->SetScalarParameterValue(TEXT("Dissolve"), -0.5f);
		DynamicDissolveMaterialInstance02->SetScalarParameterValue(TEXT("Dissolve"), -0.5f);
	}
	
	if (!CapsuleComponent)
		CapsuleComponent = FindComponentByClass<UCapsuleComponent>();

	if (!CharacterMesh)
		CharacterMesh = FindComponentByClass<USkeletalMeshComponent>();

	if (CharacterMesh)
	{
		CachedAnimInstance = Cast<UBaseAnimInstance>(CharacterMesh->GetAnimInstance());
	}
	
	// Only set up collision if both are valid
	if (CapsuleComponent)
		CapsuleComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// Setup collision
	if (CharacterMesh)
	{
		CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		CharacterMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		CharacterMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		CharacterMesh->SetCollisionObjectType(ECC_SkeletalMesh);
		CharacterMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	}

	MotionWarpingComponent = FindComponentByClass<UMotionWarpingComponent>();
	
	//Find Movera
	if (!MoverComponent)
	{
		MoverComponent = FindComponentByClass<UMoverComponent>();
		if (MoverComponent)
		{
			UE_LOG(LogTemp, Log, TEXT("%s: Found MoverComponent"), *GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: No MoverComponent! Wall-run disabled."), *GetName());
		}
	}

	// Find grenade
	if (!AttachedGrenade)
	{
		AttachedGrenade = FindComponentByTag<UStaticMeshComponent>(FName("Grenade"));
	}

	if (AttachedGrenade)
	{
		AttachedGrenade->SetVisibility(false);
	}

	if (HasAuthority())
	{
		// Ensure ALL characters (Players, AI, Enemies) initialize to their BP MaxHealth
		Health = MaxHealth;
		OnTakeAnyDamage.AddDynamic(this, &AGASPBaseCharacter::ReceiveDamage);
	}
	
	// Set our starting sensitivity
	ActiveMouseSensitivity = BaseMouseSensitivity;
	
}

void AGASPBaseCharacter::AddMedkit(int32 Amount)
{
	if (!HasAuthority()) return;

	MedkitCount = FMath::Clamp(MedkitCount + Amount, 0, MaxMedkitCount);
	OnRep_MedkitCount(); // server doesn't get its own RepNotify call, so trigger manually for local UI/HUD hook
}

void AGASPBaseCharacter::ConsumeMedkit()
{
	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		Server_ConsumeMedkit();
	}
	else if (HasAuthority())
	{
		Server_ConsumeMedkit_Implementation();
	}
}

void AGASPBaseCharacter::Server_ConsumeMedkit_Implementation()
{
	if (MedkitCount <= 0) return;
	if (Health >= MaxHealth) return; // don't waste one at full HP

	MedkitCount--;
	SetHealth(FMath::Clamp(GetHealth() + MedkitHealAmount, 0.f, GetMaxHealth()));
	OnRep_MedkitCount();

	// Push fresh health to whoever's driving this body right now — same pattern used elsewhere for HUD sync
	if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(GetController()))
	{
		PC->SetHUDHealth(GetHealth(), GetMaxHealth());
	}
}

void AGASPBaseCharacter::OnRep_MedkitCount()
{
	// Push the updated medkit count to the local player controller's HUD
	if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(GetController()))
	{
		PC->SetHUDMedkits(MedkitCount);
	}
}

float AGASPBaseCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // 1. Point Damage (Bullets, Snipers, Shotguns)
    if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
    {
        const FPointDamageEvent* PointDamageEvent = static_cast<const FPointDamageEvent*>(&DamageEvent);
        LastHitBone = PointDamageEvent->HitInfo.BoneName;
        LastHitDirection = PointDamageEvent->ShotDirection; // <-- Exact bullet path!
    }
    // 2. Radial Damage (Grenades, Rockets)
    else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
    {
        const FRadialDamageEvent* RadialDamageEvent = static_cast<const FRadialDamageEvent*>(&DamageEvent);
        LastHitBone = NAME_None; // Blast applies to the whole body
        LastHitDirection = (GetActorLocation() - RadialDamageEvent->Origin).GetSafeNormal();
    }
    // 3. Generic Damage (Falling, Zone Damage, or old Lag Comp)
    else
    {
        LastHitBone = NAME_None;
        if (DamageCauser)
        {
            LastHitDirection = (GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal();
        }
    }

    return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AGASPBaseCharacter::Destroyed()
{
	Super::Destroyed();
	
}


void AGASPBaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsTagTeamDissolving)
	{
		// Interp speed of 2.0f means a snappy 0.5-second fade in!
		CurrentTagTeamDissolve = FMath::FInterpConstantTo(CurrentTagTeamDissolve, TargetTagTeamDissolve, DeltaTime, TagTeamDissolveSpeed);
		UpdateDissolveMaterial(CurrentTagTeamDissolve);
		
		if (CurrentTagTeamDissolve == TargetTagTeamDissolve)
		{
			bIsTagTeamDissolving = false;
		}
	}
	
	if (InteractionComponent)
	{
		UpdateInteractionFocus();
	}
	// Smoothly interpolate the shoulder side every frame
	if (CurrentShoulderSide != TargetShoulderSide)
	{
		CurrentShoulderSide = FMath::FInterpTo(CurrentShoulderSide, TargetShoulderSide, DeltaTime, ShoulderSwapSpeed);
	}
	DrawMannyBodiesDebug();
}

void AGASPBaseCharacter::ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser)
{
	GASPGameMode = GASPGameMode == nullptr ? GetWorld()->GetAuthGameMode<AGASPCoreGameMode>() : GASPGameMode;
	if (GASPGameMode == nullptr) return;
    
	Damage = GASPGameMode->CalculateDamage(InstigatorController, Controller, Damage);  
	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);

	OnDamageReceived(DamagedActor, Damage, DamageType, InstigatorController, DamageCauser); 

	// 3. ONLY FLINCH IF WE ARE STILL ALIVE
	if (Health > 0.f)
	{
		MulticastHitReact(LastHitDirection, LastHitBone);
	}
	OnBaseHealthChanged.Broadcast();
}

void AGASPBaseCharacter::ExecuteVictimDeath()
{
	// MULTIPLAYER CHECK: Only the Server is allowed to authorize death!
	if (HasAuthority())
	{
		// Try to get the attacker's controller. If the attacker disconnected, this safely falls back to nullptr.
		AController* InstigatorCtrl = TakedownInstigator.IsValid() ? TakedownInstigator->GetController() : nullptr;
		AActor* DamageCauser = TakedownInstigator.IsValid() ? TakedownInstigator.Get() : nullptr;

		// Apply lethal damage to OURSELVES
		UGameplayStatics::ApplyDamage(
			this, 
			9999.f, 
			InstigatorCtrl, 
			DamageCauser, 
			UDamageType::StaticClass()
		);
	}

	// Clear the memory
	TakedownInstigator.Reset();
}

void AGASPBaseCharacter::EndTakedownLock()
{
	bDisableGameplay = false;

	// 2. Restore Collision & Gravity
	if (CapsuleComponent)
	{
		CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Block);
		CapsuleComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		CapsuleComponent->SetEnableGravity(true); 
	}
}

void AGASPBaseCharacter::ExecuteTakedownFX()
{

}

#pragma region Interaction

void AGASPBaseCharacter::UpdateInteractionFocus()
{
	// Intentionally left blank! 
	// Human Players override this to trace from the Camera/Crosshair.
	// AI Companions override this to trace from their Head, or ignore it entirely in favor of Behavior Trees.
}

void AGASPBaseCharacter::SetFocusedItem(AActor* NewItem)
{
    if (InteractionComponent->FocusedItem == NewItem) return;
    ClearFocusedItem();
    InteractionComponent->FocusedItem = NewItem;
    if (InteractionComponent->FocusedItem)
    {
        IInteractionInterface::Execute_ShowInteractionPrompt(InteractionComponent->FocusedItem, true);
    }
}

void AGASPBaseCharacter::ClearFocusedItem()
{
    if (InteractionComponent->FocusedItem)
    {
        IInteractionInterface::Execute_ShowInteractionPrompt(InteractionComponent->FocusedItem, false);

        if (HasAuthority())
        {
            UDownedComponent* Downed = InteractionComponent->FocusedItem->FindComponentByClass<UDownedComponent>();
            if (Downed && Downed->IsDowned() && Downed->GetCurrentReviver() == this)
            {
                Downed->CancelRevive();
            }
        }
        else
        {
            ServerCancelRevive(InteractionComponent->FocusedItem);
        }
    }
    InteractionComponent->FocusedItem = nullptr;
}

void AGASPBaseCharacter::ServerCancelRevive_Implementation(AActor* DownedTarget) 
{
    if (DownedTarget)
    {
        UDownedComponent* Downed = DownedTarget->FindComponentByClass<UDownedComponent>();
        if (Downed && Downed->IsDowned() && Downed->GetCurrentReviver() == this)
        {
            Downed->CancelRevive();
        }
    }
}

void AGASPBaseCharacter::Interact_Implementation(AActor* Interactor)
{
    UDownedComponent* Downed = FindComponentByClass<UDownedComponent>();
    if (Downed && Downed->IsDowned())
    {
        AGASPBaseCharacter* Reviver = Cast<AGASPBaseCharacter>(Interactor);
        if (Reviver)
        {
            Downed->StartRevive(Reviver);
        }
    }
}

FText AGASPBaseCharacter::GetInteractionText_Implementation() const
{
    UDownedComponent* Downed = FindComponentByClass<UDownedComponent>();
    if (Downed && Downed->IsDowned()) return Downed->GetInteractionText_Implementation();
    
    return FText::GetEmpty();
}

#pragma endregion Interaction

#pragma region PickupInterface

void AGASPBaseCharacter::OnPickupBegin_Implementation(AActor* PickupActor)
{
    if (!PickupActor || !CombatComponent) return;
    if (PickupActor->GetClass()->ImplementsInterface(UInteractionInterface::StaticClass()))
    {
        if (InteractionComponent) InteractionComponent->NearbyItem.AddUnique(PickupActor);
    }
}

void AGASPBaseCharacter::OnPickupEnd_Implementation(AActor* PickupActor)
{
    if (!PickupActor || !InteractionComponent) return;
    InteractionComponent->NearbyItem.Remove(PickupActor);
    if (InteractionComponent->FocusedItem == PickupActor) ClearFocusedItem();
}

void AGASPBaseCharacter::OnEquipItem_Implementation(AActor* Item)
{
    if (!CombatComponent || !Item) return;
    AItem* WeaponItem = Cast<AItem>(Item);
    if (WeaponItem) CombatComponent->ServerEquip(WeaponItem);
}

#pragma endregion PickupInterface

bool AGASPBaseCharacter::GetIsCombatReady() const
{
	if (CachedAnimInstance)
	{
		return CachedAnimInstance->GetIsCombatReady();
	}
	return false;
}

void AGASPBaseCharacter::ResetCombatReady()
{
	if (CachedAnimInstance)
	{
		CachedAnimInstance->ResetCombatReady();
	}
}

void AGASPBaseCharacter::MulticastHitReact_Implementation(FVector_NetQuantizeNormal HitDirection, FName HitBone)
{
	if (CharacterMesh && CharacterMesh->GetAnimInstance())
	{
		if (UBaseAnimInstance* BaseAnim = Cast<UBaseAnimInstance>(CharacterMesh->GetAnimInstance()))
		{
			// We are passing the Bone Name into the AnimBP now!
			BaseAnim->TriggerHitReact(HitDirection, HitBone);
		}
	}
}

bool AGASPBaseCharacter::IsSlideBuffered() const
{
	// Return true if they are holding the button OR if they tapped it within the last 0.25 seconds
	return bIsHoldingCrouch || (GetWorld()->GetTimeSeconds() - LastCrouchInputTime <= SlideBufferWindow);
}

void AGASPBaseCharacter::SetCrouchInput(bool bPressed)
{
	bIsHoldingCrouch = bPressed;
	
	// Record the exact millisecond they pressed it
	if (bPressed)
	{
		LastCrouchInputTime = GetWorld()->GetTimeSeconds();
	}
}

void AGASPBaseCharacter::SwapCameraShoulder()
{
	// If it's greater than 0 (Right), flip to -1 (Left). Otherwise, flip to 1 (Right).
	TargetShoulderSide = (TargetShoulderSide > 0.0f) ? -1.0f : 1.0f;
}

void AGASPBaseCharacter::SetCameraShoulder(float NewShoulderSide)
{
	TargetShoulderSide = FMath::Clamp(NewShoulderSide, -1.0f, 1.0f);
}

void AGASPBaseCharacter::EnableTargetingOutline(bool bEnable, int32 StencilValue)
{
	if (CharacterMesh)
	{
		CharacterMesh->SetRenderCustomDepth(bEnable);
		CharacterMesh->SetCustomDepthStencilValue(StencilValue);
		CharacterMesh->MarkRenderStateDirty();
	}
}

void AGASPBaseCharacter::Multicast_SetFrozen_Implementation(bool bFrozen, UMaterialInterface* FreezeMaterial, UNiagaraSystem* FreezeVFX)
{
	if (bFrozen)
	{
		if (CharacterMesh)
		{
			CharacterMesh->bPauseAnims = true;
			
			if (FreezeMaterial) 
			{
				CharacterMesh->SetOverlayMaterial(FreezeMaterial);
			}

			if (FreezeVFX)
			{
				ActiveFrozenVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
					FreezeVFX, CharacterMesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true
				);
			}
		}

		// Trigger the Blueprint Event for good measure!
		OnFrozen(true);
	}
	else
	{
		if (CharacterMesh)
		{
			CharacterMesh->bPauseAnims = false;
			CharacterMesh->SetOverlayMaterial(nullptr);
		}

		if (ActiveFrozenVFX)
		{
			ActiveFrozenVFX->Deactivate();
			ActiveFrozenVFX = nullptr;
		}

		OnFrozen(false);
	}
}

void AGASPBaseCharacter::Client_SetTargetOutline_Implementation(AGASPBaseCharacter* TargetChar, bool bEnable, int32 StencilValue)
{
	if (TargetChar)
	{
		// This now runs LOCALLY on the spell-caster's screen only!
		TargetChar->EnableTargetingOutline(bEnable, StencilValue);
	}
}

void AGASPBaseCharacter::Multicast_ResetToAlive_Implementation()
{
	bDisableGameplay = false;

	// Re-enable replication so the client stops freezing where they died!
	SetReplicateMovement(true);
	
	// 1. Kill death timers
	GetWorldTimerManager().ClearTimer(DissolveTImer);

	// 2. Wake up Capsule
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CapsuleComponent->SetEnableGravity(true);

	// 3. Stop Ragdoll
	CharacterMesh->SetAllBodiesSimulatePhysics(false);
	CharacterMesh->SetSimulatePhysics(false);
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CharacterMesh->SetCollisionProfileName(TEXT("CharacterMesh"));
	CharacterMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CharacterMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CharacterMesh->SetCollisionObjectType(ECC_SkeletalMesh);
	CharacterMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CharacterMesh->bPauseAnims = false;

	// Snap the mesh back inside the capsule so hitboxes align!
	CharacterMesh->AttachToComponent(CapsuleComponent, FAttachmentTransformRules::KeepRelativeTransform);
	CharacterMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	
	if (DissolveTimeline) DissolveTimeline->Stop();
	
	if (OriginalMaterial)
	{
		CharacterMesh->SetMaterial(0, OriginalMaterial);
		CharacterMesh->SetMaterial(1, OriginalMaterial);
	}
}


void AGASPBaseCharacter::DrawMannyBodiesDebug()
{
	if (CVarDrawMannyBodies.GetValueOnGameThread() == 0 || !CharacterMesh) return;

	for (FBodyInstance* Body : CharacterMesh->Bodies)
	{
		if (!Body) continue;
		const UBodySetup* Setup = Body->GetBodySetup();
		if (!Setup) continue;

		const FTransform TM = Body->GetUnrealWorldTransform();

		for (const FKSphylElem& E : Setup->AggGeom.SphylElems)
		{
			DrawDebugCapsule(GetWorld(), TM.TransformPosition(E.Center),
				E.Length * 0.5f + E.Radius, E.Radius,
				TM.GetRotation() * E.Rotation.Quaternion(),
				FColor::Green, false, -1.f, 0, 1.f);
		}
		for (const FKSphereElem& E : Setup->AggGeom.SphereElems)
		{
			DrawDebugSphere(GetWorld(), TM.TransformPosition(E.Center),
				E.Radius, 12, FColor::Green, false, -1.f, 0, 1.f);
		}
		for (const FKBoxElem& E : Setup->AggGeom.BoxElems)
		{
			DrawDebugBox(GetWorld(), TM.TransformPosition(E.Center),
				FVector(E.X, E.Y, E.Z) * 0.5f,
				TM.GetRotation() * E.Rotation.Quaternion(),
				FColor::Green, false, -1.f, 0, 1.f);
		}
	}
}

void AGASPBaseCharacter::ApplyDeathPhysics()
{
	if (CharacterMesh == nullptr) return;

	CharacterMesh->bPauseAnims = true;
	
	// This wipes the collision slate clean...
	CharacterMesh->SetCollisionProfileName(TEXT("Ragdoll"));
	
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CharacterMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// so we MUST apply our custom block AFTER the profile is set!
	// This guarantees the ragdoll will block Projectiles (WorldDynamic).
	CharacterMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

	CharacterMesh->SetAllBodiesSimulatePhysics(true);
	CharacterMesh->SetSimulatePhysics(true);
	CharacterMesh->SetEnableGravity(true); 
	CharacterMesh->WakeAllRigidBodies();

	FName BoneToBlast = LastHitBone;
	if (BoneToBlast == NAME_None || CharacterMesh->GetBoneIndex(BoneToBlast) == INDEX_NONE)
	{
		BoneToBlast = FName("pelvis"); 
	}
    
	FVector DeathImpulse = LastHitDirection * RagdollDeathImpulse; 
	CharacterMesh->AddImpulse(DeathImpulse, BoneToBlast, true);
}

void AGASPBaseCharacter::OnRep_Health(float LastHealth)
{
	OnBaseHealthChanged.Broadcast();
}

void AGASPBaseCharacter::DissolveTimerFinished()
{
	// Start dissolve effect
	if (DissolveMaterialInstance01)
	{
		DynamicDissolveMaterialInstance01 = UMaterialInstanceDynamic::Create(DissolveMaterialInstance01, this);
		DynamicDissolveMaterialInstance02 = UMaterialInstanceDynamic::Create(DissolveMaterialInstance02, this);
		CharacterMesh->SetMaterial(0, DynamicDissolveMaterialInstance01);
		CharacterMesh->SetMaterial(1, DynamicDissolveMaterialInstance02);
		DynamicDissolveMaterialInstance01->SetScalarParameterValue(TEXT("Dissolve"), 0.55f);
		DynamicDissolveMaterialInstance01->SetScalarParameterValue(TEXT("Glow"), 200.f);

		DynamicDissolveMaterialInstance02->SetScalarParameterValue(TEXT("Dissolve"), 0.55f);
		DynamicDissolveMaterialInstance02->SetScalarParameterValue(TEXT("Glow"), 200.f);
	}
	StartDissolve();
}

void AGASPBaseCharacter::UpdateDissolveMaterial(float DissolveValue)
{
	// Check and update independently instead of requiring both!
	if (DynamicDissolveMaterialInstance01)
	{
		DynamicDissolveMaterialInstance01->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
	}
	
	if (DynamicDissolveMaterialInstance02)
	{
		DynamicDissolveMaterialInstance02->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
	}
}

void AGASPBaseCharacter::StartDissolve()
{
	DissolveTrack.BindDynamic(this, &AGASPBaseCharacter::UpdateDissolveMaterial);
	if (DissolveCurve && DissolveTimeline)
	{
		DissolveTimeline->AddInterpFloat(DissolveCurve, DissolveTrack);
		DissolveTimeline->Play();
	}
}

void AGASPBaseCharacter::BeginRagdoll(FVector ImpulseDirection, float ImpulseStrength)
{
	if (!HasAuthority()) return;

	Multicast_BeginRagdoll(ImpulseDirection, ImpulseStrength);

	if (AGASPAIController* AICon = Cast<AGASPAIController>(GetController()))
	{
		if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
		{
			BB->SetValueAsBool(AGASPAIController::BB_bIsInRagdoll, true); // renamed from bIsFrozen
		}
	}
}

void AGASPBaseCharacter::RecoverFromRagdoll()
{
	if (!HasAuthority()) return;

	// Do not attempt to stand up a dead body! 
	// This prevents the Matrix from turning off their physics and dropping them through the floor.
	if (Health <= 0.f || bDisableGameplay) return;

	GetWorldTimerManager().ClearTimer(TemporaryRagdollTimer);

	Multicast_RecoverFromRagdoll();

	if (AGASPAIController* AICon = Cast<AGASPAIController>(GetController()))
	{
		if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
		{
			BB->SetValueAsBool(AGASPAIController::BB_bIsInRagdoll, false);
		}
	}
}

void AGASPBaseCharacter::EnterTemporaryRagdoll(FVector ImpulseDirection, float ImpulseStrength, float RecoverAfterSeconds)
{
	if (!HasAuthority()) return;

	BeginRagdoll(ImpulseDirection, ImpulseStrength);

	GetWorldTimerManager().SetTimer(TemporaryRagdollTimer, this,
    		&AGASPBaseCharacter::RecoverFromRagdoll, RecoverAfterSeconds, false);
}

void AGASPBaseCharacter::Multicast_BeginRagdoll_Implementation(FVector ImpulseDirection, float ImpulseStrength)
{
	if (!CharacterMesh) return;

	if (CapsuleComponent)
	{
		CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CapsuleComponent->SetEnableGravity(false);
	}
	
	CharacterMesh->bPauseAnims = true;
	CharacterMesh->SetCollisionProfileName(TEXT("Ragdoll"));
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CharacterMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	
	// Ensure floating Zero-G ragdolls also block projectiles!
	CharacterMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

	CharacterMesh->SetAllBodiesSimulatePhysics(true);
	CharacterMesh->SetSimulatePhysics(true);
	CharacterMesh->WakeAllRigidBodies();
	CharacterMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

	FName ImpulseBone = FName("pelvis");
	if (CharacterMesh->GetBoneIndex(ImpulseBone) == INDEX_NONE)
	{
		ImpulseBone = NAME_None;
	}

	FVector FinalImpulse = ImpulseDirection.GetSafeNormal() * ImpulseStrength;
	TWeakObjectPtr<AGASPBaseCharacter> WeakSelf(this);
	FName BoneCopy = ImpulseBone;

	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakSelf, FinalImpulse, BoneCopy]()
	{
		if (AGASPBaseCharacter* Self = WeakSelf.Get())
		{
			if (Self->CharacterMesh)
			{
				Self->CharacterMesh->AddImpulse(FinalImpulse, BoneCopy, true);
			}
		}
	}));
}

void AGASPBaseCharacter::Multicast_RecoverFromRagdoll_Implementation()
{
	//Ensure clients also abort the recovery for dead bodies
	if (Health <= 0.f || bDisableGameplay) return;

	if (!CharacterMesh || !CapsuleComponent) return;

	FVector RestingLocation = CharacterMesh->GetSocketLocation(FName("pelvis"));
	if (RestingLocation.IsZero())
	{
		RestingLocation = CharacterMesh->GetComponentLocation();
	}

	CharacterMesh->SetAllBodiesSimulatePhysics(false);
	CharacterMesh->SetSimulatePhysics(false);
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CharacterMesh->SetCollisionProfileName(TEXT("CharacterMesh"));
	CharacterMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CharacterMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CharacterMesh->SetCollisionObjectType(ECC_SkeletalMesh);
	CharacterMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CharacterMesh->bPauseAnims = false;

	FVector CapsuleTargetLocation = RestingLocation + FVector(0.f, 0.f, 90.f); 
	CapsuleComponent->SetWorldLocation(CapsuleTargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CapsuleComponent->SetEnableGravity(true);

	CharacterMesh->AttachToComponent(CapsuleComponent, FAttachmentTransformRules::KeepRelativeTransform);
	CharacterMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
}

void AGASPBaseCharacter::Multicast_PlayTagTeamDissolve_Implementation(bool bDissolveOut)
{
	bIsTagTeamDissolving = true;

	if (CharacterMesh)
	{
		// 1. Handle Material Slot 0
		if (DissolveMaterialInstance01)
		{
			if (!DynamicDissolveMaterialInstance01)
			{
				DynamicDissolveMaterialInstance01 = UMaterialInstanceDynamic::Create(DissolveMaterialInstance01, this);
			}
			CharacterMesh->SetMaterial(0, DynamicDissolveMaterialInstance01);
			DynamicDissolveMaterialInstance01->SetScalarParameterValue(TEXT("Glow"), 200.f);
		}

		// 2. Handle Material Slot 1 (Only if it actually exists in the Blueprint!)
		if (DissolveMaterialInstance02)
		{
			if (!DynamicDissolveMaterialInstance02)
			{
				DynamicDissolveMaterialInstance02 = UMaterialInstanceDynamic::Create(DissolveMaterialInstance02, this);
			}
			CharacterMesh->SetMaterial(1, DynamicDissolveMaterialInstance02);
			DynamicDissolveMaterialInstance02->SetScalarParameterValue(TEXT("Glow"), 200.f);
		}
	}

	// 3. Set the fade targets
	if (bDissolveOut)
	{
		CurrentTagTeamDissolve = -0.55f;
		TargetTagTeamDissolve = 0.55f; // Fade to invisible
	}
	else
	{
		CurrentTagTeamDissolve = 0.55f; // Start fully invisible
		TargetTagTeamDissolve = -0.55f; // Fade to solid
		
		// Apply immediately so they spawn fully invisible and smoothly fade IN
		UpdateDissolveMaterial(CurrentTagTeamDissolve); 
	}
}

void AGASPBaseCharacter::CalculateAO_Pitch()
{
	AO_Pitch = GetBaseAimRotation().Pitch;
	if (AO_Pitch > 90.f && !IsLocallyControlled())
	{
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
	}
}

void AGASPBaseCharacter::SpawnDefaultWeapon(bool bSetToDestroyWeapon, bool bElimmedCheck)
{
	// AI Player Should not destroy their dropped Weapon
	// Also Check for PvP Condition (bElimmed), For NPC - false
	// For AI Class - Both Conditions false
	
	GASPGameMode = GASPGameMode == nullptr ? GetWorld()->GetAuthGameMode<AGASPCoreGameMode>() : GASPGameMode;
	UWorld* World = GetWorld();
	
	if (GASPGameMode && World && !bElimmedCheck)
	{
		// 1. Spawn Primary Weapon
		if (DefaultWeaponClass)
		{
			AWeapon* StartingWeapon = World->SpawnActor<AWeapon>(DefaultWeaponClass);
			StartingWeapon->bDestroyWeapon = bSetToDestroyWeapon;
			if (CombatComponent)
			{
				CombatComponent->EquipWeapon(StartingWeapon);
			}
		}

		// 2. Spawn Secondary Weapon
		if (DefaultSecondaryWeaponClass)
		{
			AWeapon* StartingSecondaryWeapon = World->SpawnActor<AWeapon>(DefaultSecondaryWeaponClass);
			StartingSecondaryWeapon->bDestroyWeapon = bSetToDestroyWeapon;
			if (CombatComponent)
			{
				// EquipWeapon automatically slots it as Secondary because Primary is full
				CombatComponent->EquipWeapon(StartingSecondaryWeapon);
			}
		}
	}
}

#pragma region CombatInterface

USkeletalMeshComponent* AGASPBaseCharacter::GetCombatMesh_Implementation() const
{
	// Try cached pointer first
	if (CharacterMesh)
	{
		return CharacterMesh;
	}

	// Fall back to finding it
	return FindComponentByClass<USkeletalMeshComponent>();
}

AActor* AGASPBaseCharacter::GetCombatOwner_Implementation() const
{
	return const_cast<AGASPBaseCharacter*>(this);
}

APawn* AGASPBaseCharacter::GetCombatPawn_Implementation() const
{
	return const_cast<AGASPBaseCharacter*>(this);
}

void AGASPBaseCharacter::PlayFireMontage_Implementation(bool bIsAiming)
{
	if (CombatComponent == nullptr || CombatComponent->GetPrimaryWeapon() == nullptr) return;

	UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
	if (AnimInstance && FireWeaponMontage)
	{
		AnimInstance->Montage_Play(FireWeaponMontage);
		FName SectionName;
		SectionName = bIsAiming ? FName("RifleAimFire") : FName("RifleHipFire");
		AnimInstance->Montage_JumpToSection(SectionName);
	}	
}

UAnimMontage* AGASPBaseCharacter::GetReloadMontage_Implementation()
{
	// Fetch the montage directly from the equipped weapon!
	if (CombatComponent && CombatComponent->GetPrimaryWeapon())
	{
		return CombatComponent->GetPrimaryWeapon()->GetReloadMontage();
	}
	return nullptr;	
}

void AGASPBaseCharacter::PlayReloadMontage_Implementation()
{
	if (CombatComponent == nullptr || CombatComponent->GetPrimaryWeapon() == nullptr) return;

	UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
	UAnimMontage* WeaponReloadMontage = CombatComponent->GetPrimaryWeapon()->GetReloadMontage();

	if (AnimInstance && WeaponReloadMontage)
	{
		// Play the weapon's specific montage from the beginning
		AnimInstance->Montage_Play(WeaponReloadMontage);
	}	
}

void AGASPBaseCharacter::PlayThrowGrenadeMontage_Implementation()
{
	UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
	if (AnimInstance && ThrowGrenadeMontage)
	{
		AnimInstance->Montage_Play(ThrowGrenadeMontage);
	}
}

void AGASPBaseCharacter::PlayMeleeMontage_Implementation()
{
	if (!MeleeMontage || !CharacterMesh) return;
    
	UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
	if (AnimInstance && MeleeMontage)
	{
		AnimInstance->Montage_Play(MeleeMontage);
        
		// 1. Fallback section for unarmed players or basic AI
		FName SectionName = FName("Default"); 

		// 2. If we have a weapon, pick the specific bash animation!
		if (CombatComponent && CombatComponent->GetPrimaryWeapon())
		{
			switch (CombatComponent->GetPrimaryWeapon()->GetWeaponType())
			{
			case EWeaponType::EWT_AssaultRifle:
			case EWeaponType::EWT_SniperRifle:
			case EWeaponType::EWT_RocketLauncher:
			case EWeaponType::EWT_GrenadeLauncher:
				SectionName = FName("Rifle");
				break;
                
			case EWeaponType::EWT_Pistol:
			case EWeaponType::EWT_SubmachineGun:
				SectionName = FName("Pistol");
				break;
                
			case EWeaponType::EWT_Shotgun:
				SectionName = FName("Shotgun");
				break;
			}
		}

		// 3. Jump to the correct bash!
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void AGASPBaseCharacter::OnMeleeHit_Implementation()
{
	if (CombatComponent)
	{
		CombatComponent->MeleeHit();
	}
}

void AGASPBaseCharacter::OnMeleeFinished_Implementation()
{
	if (CombatComponent)
	{
		CombatComponent->SetCombatState(ECombatState::ECS_Unoccupied);
	}
}

void AGASPBaseCharacter::PlayEquippedSwapMontage_Implementation()
{
	if (CombatComponent == nullptr || CombatComponent->GetPrimaryWeapon() == nullptr) return;

	UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
	if (AnimInstance && SwapMontage)
	{
		AnimInstance->Montage_Play(SwapMontage);
	}
}

void AGASPBaseCharacter::OnSwapEquippedFinished_Implementation()
{
	// You don't need the Super call here since this is now the base implementation!
	if (CombatComponent)
	{
		CombatComponent->OnEquippedSwapFinished();
	}
}

void AGASPBaseCharacter::ShowSniperScopeCall_Implementation(bool bIsAiming)
{
	// Ensure that ONLY the human player actually draws the scope on their screen!
	// We don't want AI trying to spawn UI widgets on the server.
	if (IsLocallyControlled())
	{
		ShowSniperScopeWidget(bIsAiming);
	}
}

UStaticMeshComponent* AGASPBaseCharacter::GetAttachedGrenade_Implementation()
{
	return AttachedGrenade;	
}

void AGASPBaseCharacter::OnThrowGrenadeFinished_Implementation()
{
	GetCombatComponent()->OnThrowGrenadeFinished();
}

void AGASPBaseCharacter::OnLaunchGrenade_Implementation()
{
	GetCombatComponent()->OnLaunchGrenade();
}

void AGASPBaseCharacter::OnFinishReload_Implementation()
{
	GetCombatComponent()->OnFinishReload();
}

void AGASPBaseCharacter::OnShotgunShellReload_Implementation()
{
	GetCombatComponent()->OnShotgunShellReload();
}

bool AGASPBaseCharacter::GetAiming_Implementation() const
{
	if (CombatComponent)
	{
		return CombatComponent->GetIsAiming();
	}
	return false;
}

bool AGASPBaseCharacter::GetIsFiring_Implementation() const
{
	if (CombatComponent)
	{
		return CombatComponent->GetFireButtonPressed();
	}
	return false;	
}

UCombatComponent* AGASPBaseCharacter::GetCombatComponent_Implementation()
{
	return CombatComponent;	
}

AWeapon* AGASPBaseCharacter::GetPrimaryWeapon_Implementation()
{
	return GetCombatComponent()->GetPrimaryWeapon();
}

float AGASPBaseCharacter::GetAOPitch_Implementation()
{
	return AO_Pitch;
}

float AGASPBaseCharacter::GetAOYaw_Implementation()
{
	return AO_Yaw;
}

bool AGASPBaseCharacter::GetIsGameplayDisbled_Implementation() 
{ 
	return false; 
}

EWeaponType AGASPBaseCharacter::GetEquippedWeaponType_Implementation() const
{
	return CombatComponent->GetEquippedWeaponType();
}

bool AGASPBaseCharacter::GetIsWallRunning_Implementation() const
{
	if (WallRunComponent)
	{
		return WallRunComponent->IsWallRunning();
	}
	return false;
}

bool AGASPBaseCharacter::GetIsRightWall_Implementation() const
{
	if (WallRunComponent)
	{
		return WallRunComponent->IsRightWall();
	}
	return false;
}

#pragma endregion CombatInterface
