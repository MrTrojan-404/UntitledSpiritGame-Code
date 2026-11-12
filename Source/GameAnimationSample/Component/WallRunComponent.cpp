#include "WallRunComponent.h"

#include "CombatComponent.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h" 
#include "DefaultMovementSet/LayeredMoves/LaunchMove.h"
#include "GameAnimationSample/AI/Character/GASPAICharacterBase.h"
#include "GameAnimationSample/GameVariables/CombatState.h"

UWallRunComponent::UWallRunComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UWallRunComponent::BeginPlay()
{
	Super::BeginPlay();

	CharacterOwner = Cast<AGASPBaseCharacter>(GetOwner());
	if (CharacterOwner)
	{
		MoverComp = CharacterOwner->FindComponentByClass<UMoverComponent>();
	}
}

void UWallRunComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CharacterOwner || !MoverComp) return;

	// 1. Reset fatigue when we touch the ground
	if (MoverComp->GetSyncState().MovementMode == FName("Walking"))
	{
		// Only re-enable wall run once the cooldown has cleared.
		// Previously this was unconditionally true, so a ground-touch
		// mid-wall-run would re-arm the system before the cooldown started.
		if (GetWorld()->GetTimeSeconds() - LastWallRunEndTime >= WallRunCooldown)
		{
			bCanWallRun = true;
		}
		
		if (bIsWallRunning) ExitWallRun();

		// --- MOMENTUM BLEED ---
		if (bDidWallRunThisJump)
		{
			// THE MOMENTUM SLIDE
			if (CharacterOwner->IsSlideBuffered())
			{
				CharacterOwner->OnBufferedSlideLanding();
				// The player wants to slide! 
				// We SKIP the brakes, letting them hit the ground with all 1200 velocity intact.
				// Ground friction will naturally slow them down during the slide animation.
				bDidWallRunThisJump = false; 
				return; 
			}
			
			FVector CurrentVel = MoverComp->GetVelocity();
			FVector VelocityXY(CurrentVel.X, CurrentVel.Y, 0.f);

			// If we are skating faster than our normal sprint speed, clamp it!
			if (VelocityXY.Length() > GroundLandingSpeed)
			{
				VelocityXY = VelocityXY.GetSafeNormal() * GroundLandingSpeed;

				TSharedPtr<FLayeredMove_LinearVelocity> LandingClamp = MakeShared<FLayeredMove_LinearVelocity>();
				LandingClamp->Velocity = FVector(VelocityXY.X, VelocityXY.Y, CurrentVel.Z);
             
				// 150ms gives the animation State Machine perfect time to blend into the run stop
				LandingClamp->DurationMs = 150.0f; 
				LandingClamp->MixMode = EMoveMixMode::OverrideVelocity;
               
				MoverComp->QueueLayeredMove(LandingClamp);
			}
           
			// Reset the flag so we don't accidentally clamp normal gameplay jumps
			bDidWallRunThisJump = false; 
		}
		return;
	}

	// 2. If we are currently wall running, check if we need to fall off
	if (bIsWallRunning)
	{
		// Did the player let go of 'W'? Drop them and fatigue them.
		if (!bIsHoldingForward)
		{
			bCanWallRun = false; 
			ExitWallRun();
			return;
		}

		// Don't check wall existence in the first MinWallRunCheckDelay seconds.
		// If we started right at the wall's edge, we'd immediately exit and get
		// the full wall-run boost with no clamp applied to counter it.
		if (GetWorld()->GetTimeSeconds() - WallRunStartTime < MinWallRunCheckDelay)
		{
			return;
		}

		// Is the player looking too far backwards? Drop them and fatigue them.
		FVector CameraForward = CharacterOwner->GetControlRotation().Vector();
		CameraForward.Z = 0.f;
		CameraForward.Normalize();

		if (FVector::DotProduct(CameraForward, CurrentWallTangent) < -0.1f)
		{
			bCanWallRun = false; 
			ExitWallRun();
			return;
		}

		// Did we hit a corner and lose physical speed? Drop them and fatigue them.
		FVector CurrentVel = MoverComp->GetVelocity();
		CurrentVel.Z = 0.f; 
		if (CurrentVel.Length() < 10.f)
		{
			bCanWallRun = false;
			ExitWallRun();
			return; 
		}

		// IS THE WALL STILL THERE?
		FVector StartLoc = CharacterOwner->GetActorLocation();
		FVector RightVec = CharacterOwner->GetActorRightVector();
		
		// Shoot a trace only to the side we are currently running on
		FVector EndLoc = StartLoc + (bIsRightWall ? RightVec : -RightVec) * TraceDistance;
		
		FHitResult HitResult;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(CharacterOwner);
		
		bool bHitWall = GetWorld()->LineTraceSingleByChannel(HitResult, StartLoc, EndLoc, ECC_Visibility, Params);
		
		// If the laser hit nothing (ran out of wall) OR hit a sloped floor (normal > 0.2)
		// OR hit something that isn't valid wall geometry (a pawn, a skeletal mesh)...
		if (!bHitWall || FMath::Abs(HitResult.ImpactNormal.Z) > 0.2f
			|| HitResult.GetActor()->IsA<APawn>()
			|| Cast<USkeletalMeshComponent>(HitResult.GetComponent()))
		{
			bCanWallRun = false; // Fatigue them so they don't glitch on the corner
			ExitWallRun();       // Drops physics and stops the montage!
			return;
		}
		// --- CUSTOM WALL RUN GRAVITY ---
		// Mover's Falling mode applies full world gravity automatically.
		// If we want a floatier wall run, we calculate the remaining gravity 
		// and cancel it out by injecting an upward velocity every frame.
		if (WallRunGravityScale != 1.0f)
		{
			float WorldGravityZ = GetWorld()->GetGravityZ(); // Usually around -980.f
			
			// Calculate the amount of gravity force we want to CANCEL OUT
			float GravityToCancel = WorldGravityZ * (1.0f - WallRunGravityScale);
			
			TSharedPtr<FLayeredMove_LinearVelocity> AntiGravityMove = MakeShared<FLayeredMove_LinearVelocity>();
			
			// Negative * Negative = Positive upward push. Multiply by DeltaTime to convert acceleration to velocity for this frame.
			AntiGravityMove->Velocity = FVector(0.f, 0.f, -GravityToCancel * DeltaTime);
			
			AntiGravityMove->DurationMs = 0.f; // Instant 1-frame application
			AntiGravityMove->MixMode = EMoveMixMode::AdditiveVelocity; // ADD to the falling speed, don't override it
			
			MoverComp->QueueLayeredMove(AntiGravityMove);
		}
	}
}

void AGASPBaseCharacter::Server_RequestTakedown_Implementation(AGASPAICharacterBase* Victim)
{
	if (Victim)
	{
		// We are now safely on the Server. 
		// We tell the Victim (using the interface) that WE (this) are attacking them.
		// Since the Server owns the Victim, it will successfully trigger their Multicast!
		IInteractionInterface::Execute_ExecuteStealthTakedown(Victim, this);
	}
}
void UWallRunComponent::CheckForWall()
{
	if (GetWorld()->GetTimeSeconds() - LastWallRunEndTime < WallRunCooldown) return;

	// Only allow wall-running on actual world geometry.
	// Without this, the traces hit other players and even equipped guns
	// (which block ECC_Visibility via the character mesh collision setup).
	auto IsValidWallSurface = [](const FHitResult& Hit) -> bool
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor) return false;

		// Reject pawns (other players, AI, companions)
		if (HitActor->IsA<APawn>()) return false;

		// Reject any actor whose hit component is a SkeletalMesh
		// (covers equipped weapons attached to characters)
		if (Cast<USkeletalMeshComponent>(Hit.GetComponent())) return false;

		return true;
	};

	FVector StartLoc = CharacterOwner->GetActorLocation();
	FVector RightVec = CharacterOwner->GetActorRightVector();

	// Check speed requirement
	FVector CurrentVel = MoverComp->GetVelocity();
	CurrentVel.Z = 0.f;
	if (CurrentVel.Length() < MinWallSpeed) return;

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CharacterOwner);

	// ==========================================
	// --- TRACE RIGHT ---
	// ==========================================
	FVector RightEndLoc = StartLoc + (RightVec * TraceDistance);
	
	// Draw the Right Trace (Red)
	if (bShowDebugTraces)
	{
		DrawDebugLine(GetWorld(), StartLoc, RightEndLoc, FColor::Red, false, 0.1f, 0, 2.f);
	}

	bool bHitRight = GetWorld()->LineTraceSingleByChannel(HitResult, StartLoc, RightEndLoc, ECC_Visibility, Params);

	if (bHitRight)
	{
		// Draw a Green box where the trace actually struck a surface
		if (bShowDebugTraces)
		{
			DrawDebugBox(GetWorld(), HitResult.ImpactPoint, FVector(5.f), FColor::Green, false, 2.0f, 0, 2.f);
		}

		if (FMath::Abs(HitResult.ImpactNormal.Z) < 0.2f && IsValidWallSurface(HitResult))
		{
			StartWallRun(HitResult.ImpactNormal, HitResult.ImpactPoint, true);
			return; // Exit early if we attached to the right wall
		}
	}

	// ==========================================
	// --- TRACE LEFT ---
	// ==========================================
	FVector LeftEndLoc = StartLoc + (-RightVec * TraceDistance);
	
	// Draw the Left Trace (Blue)
	if (bShowDebugTraces)
	{
		DrawDebugLine(GetWorld(), StartLoc, LeftEndLoc, FColor::Blue, false, 0.1f, 0, 2.f);
	}

	bool bHitLeft = GetWorld()->LineTraceSingleByChannel(HitResult, StartLoc, LeftEndLoc, ECC_Visibility, Params);

	if (bHitLeft)
	{
		// Draw a Green box where the trace actually struck a surface
		if (bShowDebugTraces)
		{
			DrawDebugBox(GetWorld(), HitResult.ImpactPoint, FVector(5.f), FColor::Green, false, 2.0f, 0, 2.f);
		}

		if (FMath::Abs(HitResult.ImpactNormal.Z) < 0.2f && IsValidWallSurface(HitResult))
		{
			StartWallRun(HitResult.ImpactNormal, HitResult.ImpactPoint, false);
		}
	}
}

void UWallRunComponent::StartWallRun(const FVector& WallNormal, const FVector& WallHitPoint, bool bRightWall)
{
	bIsWallRunning = true;
	bIsRightWall = bRightWall;
	CurrentWallNormal = WallNormal;
	// Flag that we are wall running this jump
	bDidWallRunThisJump = true;
	WallRunStartTime = GetWorld()->GetTimeSeconds();

	// --- SYNC TO SERVER ---
	if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy)
	{
		ServerStartWallRun(WallNormal, WallHitPoint, bRightWall);
	}
	
	// CANCEL ANY ACTIVE RELOADS / COMBAT ACTIONS
	if (CharacterOwner)
	{
		if (UCombatComponent* CombatComp = CharacterOwner->GetCombatComponent())
		{
			// If we were reloading, throwing a grenade, or swapping weapons, cancel it so we don't break the character
			if (CombatComp->GetCombatState() != ECombatState::ECS_Unoccupied || CombatComp->bLocallyReloading)
			{
				CombatComp->ResetCombatState();
			}
		}
	}
	
	// --- PLAY ANIMATION MONTAGE ---
	if (CharacterOwner && CharacterOwner->CharacterMesh)
	{
		if (UAnimInstance* AnimInstance = CharacterOwner->CharacterMesh->GetAnimInstance())
		{
			UAnimMontage* MontageToPlay = bRightWall ? RightWallRunMontage : LeftWallRunMontage;
			if (MontageToPlay)
			{
				AnimInstance->Montage_Play(MontageToPlay);
			}
		}
	}

	// DYNAMIC CAMERA SHOULDER
	if (CharacterOwner)
	{
		PreWallRunShoulder = CharacterOwner->TargetShoulderSide;
		CharacterOwner->SetCameraShoulder(bRightWall ? -1.0f : 1.0f);
	}
    
	// 1. Calculate Tangent (Direction of the wall) mathematically perfectly
	// Mathematically guarantees the correct forward direction along the wall, perfectly synced on Client and Server.
	FVector Tangent = bRightWall ? FVector::CrossProduct(FVector::UpVector, CurrentWallNormal) : FVector::CrossProduct(CurrentWallNormal, FVector::UpVector);
	CurrentWallTangent = Tangent.GetSafeNormal();

	// 2. Calculate Titanfall Desired Velocity
	FVector DesiredVelocity = (CurrentWallTangent * WallRunSpeed) + 
						(CurrentWallNormal * -WallRunAdhesionSpeed) + 
						(FVector::UpVector * WallRunUpAssist);

	// 3. Inject into Mover Plugin using the updated BasicLayeredMoves struct
	TSharedPtr<FLayeredMove_LinearVelocity> LinearMove = MakeShared<FLayeredMove_LinearVelocity>();
	LinearMove->Velocity = DesiredVelocity;
	LinearMove->DurationMs = 0.f; // 0 means it lasts forever until we cancel it
	LinearMove->MixMode = EMoveMixMode::OverrideVelocity;
    
	MoverComp->QueueLayeredMove(LinearMove);
}

void UWallRunComponent::ExitWallRun(bool bFromJump)
{
    if (!bIsWallRunning) return;

    bIsWallRunning = false;
    LastWallRunEndTime = GetWorld()->GetTimeSeconds();

	// --- SYNC TO SERVER ---
	if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy)
	{
		ServerExitWallRun(bFromJump);
	}
	
	// Only force Falling if we're not already on the ground.
	// Calling this while in Walking mode was kicking the character
	// briefly airborne, causing an immediate wall re-attach.
	if (MoverComp->GetSyncState().MovementMode != FName("Walking"))
	{
		MoverComp->QueueNextMode(FName("Falling"));
	}
	
    // ---------------------------------------------------------------
    // AIR VELOCITY CLAMP
    // Wall jump manages its own velocity via FLayeredMove_Launch, so
    // skip this block for that path. For every other exit (ran out of
    // wall, let go of W, looked backward, etc.) we need to bleed the
    // wall-run speed immediately — otherwise the player carries 1200
    // units through the air until they land.
    //
    // IMPORTANT: We clamp unconditionally here (no length check).
    // GetVelocity() returns the current physical velocity, which hasn't
    // had the wall-run LinearVelocity move applied yet (it's still queued).
    // The old length check would therefore always be false on an immediate
    // exit, letting the queued 1200-unit move slip through uncancelled.
    // ---------------------------------------------------------------
    if (!bFromJump)
    {
        FVector CurrentVel = MoverComp->GetVelocity();
        FVector VelocityXY(CurrentVel.X, CurrentVel.Y, 0.f);

        // Clamp to AirExitMaxSpeed, but never accelerate — if the player
        // is somehow slower than the cap already, keep their actual speed.
        float ClampedSpeed = FMath::Min(VelocityXY.Length(), AirExitMaxSpeed);
        FVector ClampedXY = VelocityXY.IsNearlyZero() ? FVector::ZeroVector : VelocityXY.GetSafeNormal() * ClampedSpeed;

        TSharedPtr<FLayeredMove_LinearVelocity> AirClamp = MakeShared<FLayeredMove_LinearVelocity>();
        // Preserve Z so we don't kill upward momentum mid-air
        AirClamp->Velocity = FVector(ClampedXY.X, ClampedXY.Y, CurrentVel.Z);
        // Long enough to cover the whole fall; the ground-landing
        // bleed will take over once they touch down anyway.
        AirClamp->DurationMs = 600.0f;
        AirClamp->MixMode = EMoveMixMode::OverrideVelocity;

        MoverComp->QueueLayeredMove(AirClamp);
    }

	// --- RESTORE CAMERA SHOULDER ---
	if (CharacterOwner)
	{
		CharacterOwner->SetCameraShoulder(PreWallRunShoulder);
	}
	
    // --- STOP ANIMATION MONTAGE ---
    if (CharacterOwner && CharacterOwner->CharacterMesh)
    {
        if (UAnimInstance* AnimInstance = CharacterOwner->CharacterMesh->GetAnimInstance())
        {
            if (RightWallRunMontage && AnimInstance->Montage_IsPlaying(RightWallRunMontage))
            {
                AnimInstance->Montage_Stop(0.1f, RightWallRunMontage);
            }
            if (LeftWallRunMontage && AnimInstance->Montage_IsPlaying(LeftWallRunMontage))
            {
                AnimInstance->Montage_Stop(0.1f, LeftWallRunMontage);
            }
        }
    }
}

void UWallRunComponent::WallJump()
{
	if (!bIsWallRunning) return;
	if (GetWorld()->GetTimeSeconds() - WallRunStartTime < 0.15f) return;

	// 1. Calculate the exact camera angle on the Client
	FVector CameraForward = CharacterOwner->GetControlRotation().Vector();
	CameraForward.Z = 0.f;
	CameraForward.Normalize();

	// 2. Execute locally for instant feedback
	ExecuteWallJump(CameraForward);

	// 3. Tell the Server to jump using OUR exact camera angle
	if (GetOwner() && GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
	{
		ServerWallJump(CameraForward);
	}
}

void UWallRunComponent::ExecuteWallJump(FVector CameraForward)
{
	FVector JumpDirection = (CurrentWallNormal * 0.3f + CameraForward * 0.7f).GetSafeNormal();
	FVector LaunchVelocity = (JumpDirection * JumpOutSpeed) + (FVector::UpVector * JumpUpSpeed);

	TSharedPtr<FLayeredMove_Launch> LaunchMove = MakeShared<FLayeredMove_Launch>();
	LaunchMove->LaunchVelocity = LaunchVelocity;
	MoverComp->QueueLayeredMove(LaunchMove);

	bCanWallRun = true;
	ExitWallRun(true); 
}

void UWallRunComponent::ServerWallJump_Implementation(FVector ClientCameraForward)
{
	// Optional security: Ensure the client didn't send us a corrupted vector
	ClientCameraForward.Normalize(); 
	
	// The Server now uses the exact same vector, so the Mover predictions align perfectly!
	ExecuteWallJump(ClientCameraForward);
}

void UWallRunComponent::SetIsHoldingForward(bool bHolding)
{
	bIsHoldingForward = bHolding;

	// --- MULTIPLAYER SYNC ---
	// The Server also needs to know if the player let go of 'W' so it can drop them off the wall
	if (GetOwner() && GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
	{
		ServerSetIsHoldingForward(bHolding);
	}
}

void UWallRunComponent::ServerSetIsHoldingForward_Implementation(bool bHolding)
{
	SetIsHoldingForward(bHolding);
}

void UWallRunComponent::OnJumpPressed()
{
	if (!CharacterOwner || !MoverComp) return;

	// 1. If we are ALREADY on a wall, jump off of it
	if (bIsWallRunning)
	{
		WallJump();
	}
	// 2. ONLY trace if we are strictly falling through the air
	// (This ignores Sliding, Traversing, Walking, etc.)
	else if (bCanWallRun && MoverComp->GetSyncState().MovementMode == FName("Falling"))
	{
		CheckForWall();
	}
}

void UWallRunComponent::ServerStartWallRun_Implementation(FVector WallNormal, FVector WallHitPoint, bool bRightWall)
{
	// The Server receives the exact impact normal and point the client found
	StartWallRun(WallNormal, WallHitPoint, bRightWall);
}

void UWallRunComponent::ServerExitWallRun_Implementation(bool bFromJump)
{
	ExitWallRun(bFromJump);
}