// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseAnimInstance.h"

#include "MoverComponent.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "GameAnimationSample/Component/DashComponent.h"
#include "GameAnimationSample/Component/DownedComponent.h"
#include "GameAnimationSample/GameVariables/CombatState.h"
#include "GameAnimationSample/Interface/AnimInterface.h"
#include "GameAnimationSample/Interface/CombatInterface.h"
#include "Kismet/KismetMathLibrary.h"


void UBaseAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	APawn* PawnOwner = TryGetPawnOwner();
	if (PawnOwner && PawnOwner->GetClass()->ImplementsInterface(UAnimInterface::StaticClass()))
	{
		AnimOwner.SetObject(PawnOwner);
		AnimOwner.SetInterface(Cast<IAnimInterface>(PawnOwner));
	}

	bIsCombatReady = false;
}

void UBaseAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!AnimOwner) return;

    APawn* Pawn = Cast<APawn>(AnimOwner.GetObject());
    if (!Pawn) return;

    UCombatComponent* CombatComp = IAnimInterface::Execute_GetCombatComponent(Pawn);
    USkeletalMeshComponent* CombatMesh = ICombatInterface::Execute_GetCombatMesh(Pawn);
    
    if (!CombatComp || !CombatMesh) return;

	// Fetch the specific idle pose from the equipped weapon!
	if (Pawn->Implements<UAnimInterface>())
	{
		if (AWeapon* PrimaryWeapon = IAnimInterface::Execute_GetPrimaryWeapon(Pawn))
		{
			CurrentWeaponIdlePose = PrimaryWeapon->CustomIdlePose;
		}
		else
		{
			CurrentWeaponIdlePose = nullptr; // Clear it if we drop the weapon
		}
	}
	
    EquippedWeaponType = IAnimInterface::Execute_GetEquippedWeaponType(Pawn);
    bAiming = IAnimInterface::Execute_GetAiming(Pawn);
	
    bFiring = IAnimInterface::Execute_GetIsFiring(Pawn);
    bOneHandedWeapon = EquippedWeaponType == EWeaponType::EWT_SubmachineGun || EquippedWeaponType == EWeaponType::EWT_Pistol;
	bIsWallRunning = IAnimInterface::Execute_GetIsWallRunning(Pawn);
	bIsRightWall = IAnimInterface::Execute_GetIsRightWall(Pawn);

	// === FOOT IK LOGIC ===
	bool bIsInAir = false;
	bool bIsDashing = false;
	bool bIsTraversing = false;

	if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(Pawn))
	{
		// 1. Check In-Air (Using your existing MoverComponent modes)
		if (BaseChar->MoverComponent)
		{
			FName CurrentMode = BaseChar->MoverComponent->GetMovementModeName();
			bIsInAir = (CurrentMode == FName("Falling") || CurrentMode == FName("Airborne"));
		}

		// 2. Check Dashing
		if (BaseChar->DashComponent)
		{
			// Note: Ensure you have a public IsDashing() getter in UDashComponent!
			bIsDashing = BaseChar->DashComponent->IsDashing(); 
		}
	}

	// 3. Check Traversing
	// Your CombatComponent already tracks bIsTraversing, so we can pull it from there!
	// Note: You will need to add a quick `bool GetIsTraversing() const { return bIsTraversing; }` to CombatComponent.h
	if (CombatComp)
	{
		bIsTraversing = CombatComp->GetIsTraversing(); 
	}

	// 4. Final Evaluation
	// Turn off Foot IK if doing any extreme movement
	if (bIsInAir || bIsWallRunning || bIsDashing || bIsTraversing)
	{
		bEnableFootIK = false;
	}
	else
	{
		bEnableFootIK = true;
	}
	
	//  Calculate exactly how much time has passed since the Multicast registered a shot
	float TimeSinceAction = GetWorld()->GetTimeSeconds() - CombatComp->LastWeaponActionTime;
	
	// If sprint manually overrode us, only clear it once the player aims or fires fresh
	if (bCombatReadyOverridden)
	{
		// Aiming cancels the sprint override immediately
		if (bAiming)
		{
			bCombatReadyOverridden = false;
			bIsCombatReady = true;
		}
		// A new shot (not the old one) also cancels it — detect by checking if 
		// TimeSinceAction is very fresh (under one frame worth)
		else if (TimeSinceAction < 0.05f)
		{
			bCombatReadyOverridden = false;
			bIsCombatReady = true;
		}
		// Otherwise stay lowered
		else
		{
			bIsCombatReady = false;
		}
	}
	else
	{
		if (bAiming || TimeSinceAction < 1.5f)
		{
			bIsCombatReady = true;
		}
		else
		{
			bIsCombatReady = false;
		}
	}
	
	// LEFT HAND IK (FABRIK)

	if (EquippedWeaponType != EWeaponType::EWT_EmptyHand && IAnimInterface::Execute_GetPrimaryWeapon(Pawn)->GetItemMesh())
	{
		USkeletalMeshComponent* WeaponMesh = IAnimInterface::Execute_GetPrimaryWeapon(Pawn)->GetItemMesh();
    
		// 1. Get both transforms in World Space just ONCE to figure out the math
		FTransform SocketWorld = WeaponMesh->GetSocketTransform(FName("LeftHandSocket"), RTS_World);
		FTransform HandWorld = CombatMesh->GetSocketTransform(FName("hand_r"), RTS_World);
    
		// 2. Calculate the RELATIVE difference between them. 
		// This creates a rigid "Offset Transform" that says exactly how far the socket is from the right hand.
		LeftHandTransform = SocketWorld.GetRelativeTransform(HandWorld);
    
		// NOTE: Because the gun is rigid, this LeftHandTransform value will remain mathematically 
		// locked to the right hand, completely immune to frame-lag or camera movement!
	}
   

    // LOCOMOTION & AIM OFFSETS
    FRotator AimRotation = Pawn->GetBaseAimRotation();
    FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(Pawn->GetVelocity());
    FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation);
    DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaSeconds, 6.f);
    YawOffset = DeltaRotation.Yaw;
    
    CharacterRotationLastFrame = CharacterRotation;
    CharacterRotation = Pawn->GetActorRotation();
    AO_Yaw = IAnimInterface::Execute_GetAOYaw(Pawn);
    AO_Pitch = IAnimInterface::Execute_GetAOPitch(Pawn);

    float CameraYaw  = Pawn->GetBaseAimRotation().Yaw;
    float CharYaw    = Pawn->GetActorRotation().Yaw;
    float AimDelta   = FMath::Abs(FRotator::NormalizeAxis(CameraYaw - CharYaw));
	bAlignedToAim = AimDelta < CombatComp->AimAlignThreshold;
	
	// --- RIGHT HAND AIMING (PREPPING FOR 'LOOK AT' NODE) ---
	
	// 1. Grab the raw target from the Combat Component
	TraceHitTarget = CombatComp->GetHitTarget();
	
	// 2. Determine who owns this body
	bool bIsAI = !Pawn->IsPlayerControlled();
	bool bIsRemoteHuman = !Pawn->IsLocallyControlled() && !bIsAI;

	// 3. THE FALLBACK LOGIC
	// If the HitTarget is completely empty (0,0,0) OR this is a remote human player over the network:
	if (TraceHitTarget.IsNearlyZero() || bIsRemoteHuman)
	{
		FVector HeadLocation = Pawn->GetActorLocation() + FVector(0.f, 0.f, 70.f); 
		
		// AI point their gun straight ahead of their chest. Humans point where their camera is looking.
		FVector AimDir = bIsAI ? Pawn->GetActorForwardVector() : Pawn->GetBaseAimRotation().Vector();
		
		TraceHitTarget = HeadLocation + (AimDir * 5000.f);
	}
	
	if (bLocallyControlled)
	{
		// 1. Local Player uses their exact pixel-perfect crosshair trace
		TraceHitTarget = CombatComp->GetHitTarget(); 
	}
	else
	{
		// 2. Replicate the aiming for Server & Remote Clients!
		// Unreal automatically syncs 'BaseAimRotation' (where the camera is looking).
		// We cast a fake target 50 meters out so the gun perfectly tracks their pitch/yaw.
		FVector HeadLocation = Pawn->GetActorLocation() + FVector(0.f, 0.f, 70.f); 
		TraceHitTarget = HeadLocation + (Pawn->GetBaseAimRotation().Vector() * 5000.f);
	}
	// --- CHECK DOWNED STATE ---
    UDownedComponent* DownedComp = Pawn->FindComponentByClass<UDownedComponent>();
    bIsDowned = DownedComp && DownedComp->IsDowned();

    // --- OVERRIDE LOGIC IF DOWNED ---
    if (bIsDowned)
    {
        // Force off all aiming, IK, and offsets while crawling!
        bUseFABRIK = false;
        bShouldLookAtTarget = false;
        bUseAimOffsets = false;
        bTransformRightHand = false;
        LookAtAlpha = FMath::FInterpTo(LookAtAlpha, 0.f, DeltaSeconds, LookAtAlphaInterpSpeed);
    }
    else
    {
        //LEFT HAND LOGIC: Stays active 100% of the time as long as we are aiming/firing
    	bUseFABRIK = CombatComp->GetCombatState() == ECombatState::ECS_Unoccupied && bIsCombatReady;

        // Override logic for Left Hand
        bool bFABRIKOverride = Pawn->IsLocallyControlled() && CombatComp->GetCombatState() !=
	        ECombatState::ECS_ThrowingGrenade && CombatComp->bFinishedSwapping && bIsCombatReady;

    	if (bFABRIKOverride)
        {
            bUseFABRIK = !CombatComp->bLocallyReloading;
        }
        
        //RIGHT HAND LOGIC: Only active if we are aiming AND within the 80-degree cone
        bShouldLookAtTarget = bUseFABRIK && bAlignedToAim;    

        //Convert that boolean into a smoothly interpolating float
        float TargetAlpha = bShouldLookAtTarget ? 1.f : 0.f;

        // Interpolate towards the target.
        LookAtAlpha = FMath::FInterpTo(LookAtAlpha, TargetAlpha, DeltaSeconds, LookAtAlphaInterpSpeed);

        // Aim Offsets & Transform flags
    	bUseAimOffsets = CombatComp->GetCombatState() == ECombatState::ECS_Unoccupied && !IAnimInterface::Execute_GetIsGameplayDisbled(Pawn);
    	bTransformRightHand = CombatComp->GetCombatState() == ECombatState::ECS_Unoccupied && !IAnimInterface::Execute_GetIsGameplayDisbled(Pawn) && bIsCombatReady;
    }

	// Grab the visual recoil from the combat component
	if (Pawn->IsLocallyControlled())
	{
		bool bIsFlamethrower = false;
		if (Pawn->Implements<UAnimInterface>())
		{
			if (AWeapon* PrimaryWeapon = IAnimInterface::Execute_GetPrimaryWeapon(Pawn))
			{
				bIsFlamethrower = PrimaryWeapon->GetWeaponCategory() == EWeaponCategory::EWC_Special;
			}
		}

		// Zero out visual recoil for the flamethrower so the stream stays perfectly smooth
		if (bIsFlamethrower)
		{
			VisualRecoil = FRotator::ZeroRotator;
		}
		else
		{
			VisualRecoil = CombatComp->GetVisualRecoil();
		}
	}

	// --- HIT REACT DECAY ---
	// Decay Spine
	if (SpineHitIntensity > 0.001f)
	{
		SpineHitIntensity = FMath::FInterpTo(SpineHitIntensity, 0.001f, DeltaSeconds, HitFlinchDecaySpeed);
	}
	// Decay Head
	if (HeadHitIntensity > 0.001f)
	{
		HeadHitIntensity = FMath::FInterpTo(HeadHitIntensity, 0.001f, DeltaSeconds, HitFlinchDecaySpeed);
	}
	if (LeftArmHitIntensity > 0.001f)
	{
		LeftArmHitIntensity = FMath::FInterpTo(LeftArmHitIntensity, 0.001f, DeltaSeconds, HitFlinchDecaySpeed);
	}
	if (RightArmHitIntensity > 0.001f)
	{
		RightArmHitIntensity = FMath::FInterpTo(RightArmHitIntensity, 0.001f, DeltaSeconds, HitFlinchDecaySpeed);
	}
	
	// Note: We leave CalculatedHitRotation at full strength. 
	// The Alpha pins on the Spring Controllers will handle scaling it down!
	CalculatedHitRotation = TargetHitRotation;
	
	// Pass the inverted arm data
	ArmCalculatedHitRotation = ArmTargetHitRotation;
	
}

void UBaseAnimInstance::TriggerHitReact(const FVector& HitWorldDirection, FName HitBone)
{
	APawn* Pawn = TryGetPawnOwner();
	if (!Pawn) return;

	// ==========================================
	// THE FLAMETHROWER FILTER (EARLY EXIT)
	// ==========================================
	if (Pawn->Implements<UAnimInterface>())
	{
		if (AWeapon* PrimaryWeapon = IAnimInterface::Execute_GetPrimaryWeapon(Pawn))
		{
			if (PrimaryWeapon->GetWeaponCategory() == EWeaponCategory::EWC_Special)
			{
				return; // Kills the function here. No flinch while holding the heavy flamethrower!
			}
		}
	}
	
	// 1. Grab the bone name immediately
	FString BoneString = HitBone.ToString().ToLower();

	// ==========================================
	// THE LEG FILTER (EARLY EXIT)
	// ==========================================
	// If the bullet hit the lower body, abort the procedural flinch entirely!
	if (BoneString.Contains("thigh") || BoneString.Contains("calf") || 
		BoneString.Contains("foot")  || BoneString.Contains("pelvis") || 
		BoneString.Contains("leg"))
	{
		return; // Kills the function here. No intensities get spiked!
	}

	// 2. THE BASE MATH
	FVector LocalHitDir = Pawn->GetActorTransform().InverseTransformVectorNoScale(HitWorldDirection);
    
	float PitchAngle = LocalHitDir.Y * 60.f; 
	float RollAngle  = LocalHitDir.X * 60.f;  
	float YawAngle   = 0.f; // Default to 0

	// 3. THE SELECTIVE ZONE ROUTER
	if (BoneString.Contains("head") || BoneString.Contains("neck"))
	{
		HeadHitIntensity = 1.0f; 
	}
	// --- LEFT SIDE ---
	else if (BoneString.Contains("clavicle_l") || BoneString.Contains("hand_l"))
	{
		LeftArmHitIntensity = 1.0f; 
		SpineHitIntensity = 0.5f; // Wake up the spine!
		YawAngle = LocalHitDir.X * 45.f; // Apply the left twist!
	}
	else if (BoneString.Contains("arm_l")) 
	{
		LeftArmHitIntensity = 1.0f; // Pure arm flinch, NO twist!
	}
	// --- RIGHT SIDE ---
	else if (BoneString.Contains("clavicle_r") || BoneString.Contains("hand_r"))
	{
		RightArmHitIntensity = 1.0f; 
		SpineHitIntensity = 0.5f; // Wake up the spine!
		YawAngle = LocalHitDir.X * -45.f; // Apply the right twist!
	}
	else if (BoneString.Contains("arm_r"))
	{
		RightArmHitIntensity = 1.0f; // Pure arm flinch, NO twist!
	}
	else
	{
		SpineHitIntensity = 1.0f; // Pure bodyshot
	}

	// 4. SET THE TARGETS
	TargetHitRotation = FRotator(PitchAngle, YawAngle, RollAngle);
	ArmTargetHitRotation = FRotator(-PitchAngle, 0.f, -RollAngle);
}
