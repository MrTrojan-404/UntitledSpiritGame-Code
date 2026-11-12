// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/GameVariables/WeaponType.h"
#include "UObject/Interface.h"
#include "AnimInterface.generated.h"

class AWeapon;
class UCombatComponent;
enum class ECombatState : uint8;
// This class does not need to be modified.
UINTERFACE()
class UAnimInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class GAMEANIMATIONSAMPLE_API IAnimInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="AnimInterface")
	EWeaponType GetEquippedWeaponType() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="AnimInterface")
	bool GetAiming() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="AnimInterface")
	bool GetIsFiring() const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="AnimInterface")
	bool GetIsGameplayDisbled();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="AnimInterface")
	UCombatComponent* GetCombatComponent();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="AnimInterface")
	AWeapon* GetPrimaryWeapon();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="AnimInterface")
	float GetAOYaw();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="AnimInterface")
	float GetAOPitch();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AnimInterface|Traversal")
	bool GetIsWallRunning() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AnimInterface|Traversal")
	bool GetIsRightWall() const;
	
};
