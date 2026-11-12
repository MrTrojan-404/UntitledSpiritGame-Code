#include "DashComponent.h"

#include "CombatComponent.h"
#include "MoverComponent.h"
#include "WallRunComponent.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "MoveLibrary/MovementUtils.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"

UDashComponent::UDashComponent()
{
	PrimaryComponentTick.bCanEverTick = true; // needed for the landing-reset check
	SetIsReplicatedByDefault(true);
}

void UDashComponent::BeginPlay()
{
	Super::BeginPlay();

	CharacterOwner = Cast<AGASPBaseCharacter>(GetOwner());
	if (CharacterOwner)
	{
		MoverComp = CharacterOwner->FindComponentByClass<UMoverComponent>();
	}
}

void UDashComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!MoverComp || !CharacterOwner) return;

	// Ground: unlimited refresh while touching, by design — cooldown is the only gate here.
	bool bGrounded = MoverComp->GetSyncState().MovementMode == FName("Walking");
	if (bGrounded)
	{
		bHasAirDashed = false;
	}

	// Wall-run: refresh ONCE at the moment of attachment, not every tick while attached —
	// otherwise the whole wall-run duration grants unlimited dashes.
	bool bWallRunning = false;
	if (UWallRunComponent* WallRun = CharacterOwner->FindComponentByClass<UWallRunComponent>())
	{
		bWallRunning = WallRun->IsWallRunning();
	}

	if (bWallRunning && !bWasWallRunningLastTick)
	{
		bHasAirDashed = false;
	}
	bWasWallRunningLastTick = bWallRunning;
}

void UDashComponent::TryDash()
{
	if (bDashOnCooldown || !CharacterOwner || !MoverComp) return;

	bool bIsGrounded = MoverComp->GetSyncState().MovementMode == FName("Walking");
	if (!bIsGrounded && bHasAirDashed) return; // already used the one air dash this jump

	
	//Clear the Combat State if we interrupt a reload
	if (UCombatComponent* CombatComp = CharacterOwner->GetCombatComponent())
	{
		if (CombatComp->GetCombatState() == ECombatState::ECS_Reloading || CombatComp->bLocallyReloading)
		{
			// Clear the state variables
			CombatComp->ResetCombatState(); 
			bInterruptedReload = true;
			
			// Stop the montage so it doesn't fight the dash animation
			if (UAnimInstance* Anim = CharacterOwner->CharacterMesh->GetAnimInstance())
			{
				if (UAnimMontage* ReloadMontage = CharacterOwner->GetReloadMontage_Implementation())
				{
					Anim->Montage_Stop(0.1f, ReloadMontage);
				}
			}
		}
	}
	
	FVector Direction = CharacterOwner->CachedMovementInputVector;

	if (Direction.IsNearlyZero())
	{
		Direction = -CharacterOwner->GetControlRotation().Vector(); // backward
	}

	Direction.Z = 0.f;
	Direction.Normalize();

	ExecuteDash(Direction);

	if (GetOwner() && GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
	{
		ServerDash(Direction);
	}
    else if (GetOwner() && GetOwner()->HasAuthority())
    {
        // If an AI or Host triggered this directly, tell the other clients!
        Multicast_DashFX(Direction);
    }

	if (!bIsGrounded)
	{
		bHasAirDashed = true;
	}

	bDashOnCooldown = true;
	GetWorld()->GetTimerManager().SetTimer(DashCooldownTimer, this, &UDashComponent::ClearDashCooldown, DashCooldown, false);
}

void UDashComponent::ExecuteDash(FVector Direction)
{
	bIsDashing = true;
	
	TSharedPtr<FLayeredMove_LinearVelocity> DashMove = MakeShared<FLayeredMove_LinearVelocity>();
	DashMove->Velocity = Direction * DashSpeed;
	DashMove->DurationMs = DashDurationMs;
	DashMove->MixMode = EMoveMixMode::OverrideVelocity;

	MoverComp->QueueLayeredMove(DashMove);

    // Play visuals locally and on the server immediately
    PlayDashFX(Direction);

	if (CharacterOwner)
	{
		CharacterOwner->OnDashPerformed(Direction);
	}

	// After the dash's own window ends, force velocity back down to normal
	// ground speed instead of relying on native braking to do it eventually.
	GetWorld()->GetTimerManager().SetTimer(DashClampTimer, this,
		&UDashComponent::ClampVelocityAfterDash, DashDurationMs / 1000.f, false);
}

void UDashComponent::ServerDash_Implementation(FVector ClientDirection)
{
	ClientDirection.Normalize();
	ExecuteDash(ClientDirection);
    Multicast_DashFX(ClientDirection);
}

void UDashComponent::ClampVelocityAfterDash()
{
	if (!MoverComp) return;

	bIsDashing = false;
	
	FVector CurrentVel = MoverComp->GetVelocity();
	FVector VelocityXY(CurrentVel.X, CurrentVel.Y, 0.f);

	if (VelocityXY.Length() > PostDashLandingSpeed)
	{
		VelocityXY = VelocityXY.GetSafeNormal() * PostDashLandingSpeed;

		TSharedPtr<FLayeredMove_LinearVelocity> Clamp = MakeShared<FLayeredMove_LinearVelocity>();
		Clamp->Velocity = FVector(VelocityXY.X, VelocityXY.Y, CurrentVel.Z);
		Clamp->DurationMs = 150.f;
		Clamp->MixMode = EMoveMixMode::OverrideVelocity;

		MoverComp->QueueLayeredMove(Clamp);
	}

    // Stop Trail and Trigger End FX
    EndDashFX();

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        Multicast_EndDashFX();
    }
	if (bInterruptedReload && CharacterOwner)
	{
		if (UCombatComponent* CombatComp = CharacterOwner->GetCombatComponent())
		{
			CombatComp->Reload(); 
		}
		bInterruptedReload = false;
	}
}

void UDashComponent::ClearDashCooldown()
{
	bDashOnCooldown = false;
}

// ==========================================
// FX AND ANIMATION LOGIC
// ==========================================

void UDashComponent::PlayDashFX(FVector Direction)
{
    if (!CharacterOwner) return;

    // 1. Determine the correct directional animation
    UAnimMontage* MontageToPlay = DashForwardAnim; 
    FVector Forward = CharacterOwner->GetActorForwardVector();
    FVector Right = CharacterOwner->GetActorRightVector();

    float ForwardDot = FVector::DotProduct(Forward, Direction);
    float RightDot = FVector::DotProduct(Right, Direction);

    if (FMath::Abs(ForwardDot) > FMath::Abs(RightDot))
    {
        MontageToPlay = (ForwardDot > 0.f) ? DashForwardAnim : DashBackwardAnim;
    }
    else
    {
        MontageToPlay = (RightDot > 0.f) ? DashRightAnim : DashLeftAnim;
    }

    if (MontageToPlay && CharacterOwner->CharacterMesh)
    {
        if (UAnimInstance* AnimInst = CharacterOwner->CharacterMesh->GetAnimInstance())
        {
            AnimInst->Montage_Play(MontageToPlay);
        }
    }

	// 2. Start VFX (Niagara)
    if (DashStartVFX)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DashStartVFX, CharacterOwner->GetActorLocation(), CharacterOwner->GetActorRotation());
    }
	// 2.5 Start VFX (Cascade)
	if (DashStartCascadeVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DashStartCascadeVFX, CharacterOwner->GetActorLocation(), CharacterOwner->GetActorRotation());
	}

    // 3. Start Audio
    if (DashStartSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), DashStartSound, CharacterOwner->GetActorLocation());
    }

    // 4. Looping Trail VFX
    if (DashTrailVFX)
    {
        ActiveTrailVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
            DashTrailVFX, 
            CharacterOwner->CharacterMesh, 
            NAME_None, 
            FVector::ZeroVector, 
            FRotator::ZeroRotator, 
            EAttachLocation::SnapToTarget, 
            true
        );
    }

    // 5. Looping Trail Audio
    if (DashTrailSound)
    {
        ActiveTrailSound = UGameplayStatics::SpawnSoundAttached(DashTrailSound, CharacterOwner->CharacterMesh);
    }

	// 6. Start Afterimage Timer
	if (AfterimageVFX)
	{
		// Spawns the first one immediately
		SpawnAfterimage(); 

		// Starts the loop for the rest of the dash
		GetWorld()->GetTimerManager().SetTimer(
			AfterimageTimer, 
			this, 
			&UDashComponent::SpawnAfterimage, 
			AfterimageInterval, 
			true
		);
	}
}

void UDashComponent::EndDashFX()
{
	if (!CharacterOwner) return;

	// 1. Kill the trails & loops
	if (ActiveTrailVFX)
	{
		ActiveTrailVFX->Deactivate();
		ActiveTrailVFX = nullptr;
	}
	if (ActiveTrailSound)
	{
		ActiveTrailSound->Stop();
		ActiveTrailSound = nullptr;
	}
    
	// Stop the afterimages!
	GetWorld()->GetTimerManager().ClearTimer(AfterimageTimer);

	// 2. Play End VFX (Niagara)
	if (DashEndVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DashEndVFX, CharacterOwner->GetActorLocation(), CharacterOwner->GetActorRotation());
	}

	// 2.5 Play End VFX (Cascade)
	if (DashEndCascadeVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DashEndCascadeVFX, CharacterOwner->GetActorLocation(), CharacterOwner->GetActorRotation());
	}

	// 3. Play End Audio
	if (DashEndSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), DashEndSound, CharacterOwner->GetActorLocation());
	}
}

void UDashComponent::Multicast_DashFX_Implementation(FVector Direction)
{
    // Filter out the Host and Autonomous Proxy so they don't play the FX twice
    if (GetOwner() && GetOwner()->GetLocalRole() == ROLE_SimulatedProxy)
    {
        PlayDashFX(Direction);
    }
}

void UDashComponent::Multicast_EndDashFX_Implementation()
{
    // Filter out the Host and Autonomous Proxy so they don't play the End FX twice
    if (GetOwner() && GetOwner()->GetLocalRole() == ROLE_SimulatedProxy)
    {
        EndDashFX();
    }
}

void UDashComponent::SpawnAfterimage()
{
	if (!CharacterOwner || !AfterimageVFX) return;
    
	// We MUST spawn it attached so the Niagara Skeletal Mesh Location module 
	// can successfully find the mesh and sample the current animation pose!
	UNiagaraFunctionLibrary::SpawnSystemAttached(
		AfterimageVFX, 
		CharacterOwner->CharacterMesh, 
		NAME_None, 
		FVector::ZeroVector, 
		FRotator::ZeroRotator, 
		EAttachLocation::SnapToTarget, 
		true // AutoDestroy when the one-shot particle burst finishes
	);
}