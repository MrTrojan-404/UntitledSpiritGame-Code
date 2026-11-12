// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameAnimationSample/GameVariables/WeaponType.h"
#include "GameAnimationSample/Interface/AnimInterface.h"
#include "BaseAnimInstance.generated.h"

class IAnimInterface;
/**
 * 
 */
UCLASS()
class GAMEANIMATIONSAMPLE_API UBaseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	virtual void NativeInitializeAnimation() override;

	// The Character will call this when they take damage
	void TriggerHitReact(const FVector& HitWorldDirection, FName HitBone);

	FORCEINLINE bool GetIsCombatReady() const { return bIsCombatReady; }

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ResetCombatReady() { bIsCombatReady = false; bCombatReadyOverridden = true; }
	
protected:
	// What type of Weapon Does Player have?
	UPROPERTY(BlueprintReadOnly)
	EWeaponType EquippedWeaponType;

	// Holds the current weapon's idle animation to feed into the AnimGraph
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Animation")
	class UAnimSequence* CurrentWeaponIdlePose;

	// Is Player Aiming?
	UPROPERTY(BlueprintReadWrite)
	bool bAiming = false;

	// Is Player Firing?
	UPROPERTY(BlueprintReadOnly)
	bool bFiring = false;

	UPROPERTY(BlueprintReadOnly)
	bool bUseFABRIK;
	
	UPROPERTY(BlueprintReadOnly)
	bool bUseAimOffsets;

	UPROPERTY(BlueprintReadOnly)
	bool bTransformRightHand;

	UPROPERTY(BlueprintReadOnly)
	FTransform LeftHandTransform;
	
	UPROPERTY(BlueprintReadOnly)
	FRotator RightHandRotation;

	UPROPERTY(BlueprintReadOnly)
	bool bLocallyControlled;
	
	FRotator CharacterRotationLastFrame = FRotator::ZeroRotator;
	FRotator CharacterRotation = FRotator::ZeroRotator;
	FRotator DeltaRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly)
	float AO_Yaw;

	UPROPERTY(BlueprintReadOnly)
	float AO_Pitch;

	UPROPERTY(BlueprintReadOnly)
	float YawOffset;

	UPROPERTY(BlueprintReadOnly)
	bool  bAlignedToAim;

	UPROPERTY(BlueprintReadOnly)
	bool bOneHandedWeapon;
	
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
    FVector TraceHitTarget;
    
    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    bool bShouldLookAtTarget;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float LookAtAlpha = 0.f;

	// The speed at which the character lowers/raises their weapon when looking too far
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float LookAtAlphaInterpSpeed = 5.f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Recoil")
	FRotator VisualRecoil;

	// Is Player Downed?
	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsDowned = false;

	// --- HIT REACT VARIABLES ---
	UPROPERTY(BlueprintReadOnly, Category = "Combat|HitReact")
	FVector LocalHitDirection;

	// How fast the character recovers from the flinch
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|HitReact")
	float HitFlinchDecaySpeed = 6.f;
	
	// The final, mathematically calculated rotation to feed directly into the AnimGraph
	UPROPERTY(BlueprintReadOnly, Category = "Combat|HitReact")
	FRotator CalculatedHitRotation;

	//  Dedicated Rotator just for the arms
	UPROPERTY(BlueprintReadOnly, Category = "Combat|HitReact")
	FRotator ArmCalculatedHitRotation;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|HitReact")
	float HitIntensity = 0.f;

	//Zone-specific intensities
	UPROPERTY(BlueprintReadOnly, Category = "Combat|HitReact")
	float SpineHitIntensity = 0.001f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|HitReact")
	float HeadHitIntensity = 0.001f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|HitReact")
	float LeftArmHitIntensity = 0.001f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|HitReact")
	float RightArmHitIntensity = 0.001f;

	// Tracks if we are currently holding the gun up
	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsCombatReady = false;

	// Timer to track how long since we last shot or aimed
	float TimeSinceLastWeaponAction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	bool bIsWallRunning = false;

	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	bool bIsRightWall = false;

	// Tells the AnimGraph if Foot IK traces should be running
    	UPROPERTY(BlueprintReadOnly, Category = "IK")
    	bool bEnableFootIK = true;
    	
private:
	UPROPERTY()
	TScriptInterface<IAnimInterface> AnimOwner;

	FRotator TargetHitRotation = FRotator::ZeroRotator;
	FRotator ArmTargetHitRotation = FRotator::ZeroRotator;
	
	bool bCombatReadyOverridden = false;

};
