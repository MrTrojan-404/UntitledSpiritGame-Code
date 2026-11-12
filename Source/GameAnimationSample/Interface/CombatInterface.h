// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatInterface.generated.h"

class AItem;
// This class does not need to be modified.
UINTERFACE()
class UCombatInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class GAMEANIMATIONSAMPLE_API ICombatInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
#pragma region Owner
	
	// Where should weapons be attached?
	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	USkeletalMeshComponent* GetCombatMesh() const;

	// Who owns the weapon after equip?
	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	AActor* GetCombatOwner() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	APawn* GetCombatPawn() const;

#pragma endregion Owner

#pragma region Montage
	
	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	void PlayFireMontage(bool bIsAiming);

	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	void PlayEquippedSwapMontage();
	
	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	void PlayReloadMontage();

	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	void PlayThrowGrenadeMontage();
	
	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	UAnimMontage* GetReloadMontage();

#pragma endregion Montage

#pragma region AnimNotify Callbacks
	
	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	void OnSwapEquippedFinished();

	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	void OnFinishReload();

	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	void OnShotgunShellReload();

	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	void OnThrowGrenadeFinished();

	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	void OnLaunchGrenade();

	
#pragma endregion AnimNotify Callbacks
	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	void OnEquipItem(AActor* Item);

	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	UStaticMeshComponent* GetAttachedGrenade();

#pragma region PlayerCharacter

	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	void ShowSniperScopeCall(bool bIsAiming);
	
#pragma endregion PlayerCharacter

#pragma region Melee

	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	void PlayMeleeMontage();

	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	void OnMeleeHit(); // The AnimNotify will call this!

	UFUNCTION(BlueprintNativeEvent, Category="Combat")
	void OnMeleeFinished(); // To reset the state
	
#pragma endregion Melee
	
};
