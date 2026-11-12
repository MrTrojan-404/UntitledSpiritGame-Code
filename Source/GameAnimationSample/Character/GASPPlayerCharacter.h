// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GASPBaseCharacter.h"
#include "GameAnimationSample/GameVariables/Team.h"
#include "GameAnimationSample/Interface/InteractWithCrosshairsInterface.h"
#include "GASPPlayerCharacter.generated.h"

class UWidgetComponent;
class AWeapon;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class UCombatComponent;
class UInteractionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLeftGame);


UCLASS()
class GAMEANIMATIONSAMPLE_API
AGASPPlayerCharacter : public AGASPBaseCharacter,public IInteractWithCrosshairsInterface
{
	GENERATED_BODY()

public:
 	AGASPPlayerCharacter();
 	virtual void Tick(float DeltaTime) override;
	virtual void Destroyed() override;

	virtual void OnPickupBegin_Implementation(AActor* PickupActor) override;
	virtual void OnPickupEnd_Implementation(AActor* PickupActor) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UDownedComponent* DownedComponent;
	
	UFUNCTION(Server, Reliable)
	void ServerLeaveGame();

	FOnLeftGame OnLeftGame;

	UPROPERTY(VisibleAnywhere)
	UWidgetComponent* OverheadNameWidget;

	UPROPERTY(VisibleAnywhere)
	UWidgetComponent* OverheadHealthWidget;

	FTimerHandle HideNameTimer;
	FTimerHandle HideHealthTimer;

	void HideOverheadName();
	void HideOverheadHealth();
	
	void UpdateHUDHealth();

	void UpdateHUDAmmo();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastGainedTheLead();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastLostTheLead();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastElim(bool bPlayerLeftGame, FVector_NetQuantizeNormal HitDirection, FName HitBone = NAME_None, bool bDropWeapons = true);
	
	void SetTeamColor(ETeam Team);

	void Elim(bool bPlayerLeftGame);
	void ElimTimerFinished();

	bool bElimmed = false;

	FTimerHandle ElimTimer;

	UPROPERTY(EditDefaultsOnly)
	float RespawnDelay = 5.f;

	virtual void Interact_Implementation(AActor* Interactor) override;
	
	virtual FText GetInteractionText_Implementation() const override;
	
	virtual void Multicast_ResetToAlive_Implementation() override;
 
 #pragma region AnimInterface

	virtual bool GetIsGameplayDisbled_Implementation() override;

#pragma endregion AnimInterface
 
#pragma region InteractWithCrosshairsInterface
	
	virtual void OnCrosshairFocused() override;
	virtual void OnCrosshairUnfocused() override;
	
#pragma endregion InteractWithCrosshairsInterface

	void DropOrDestroyWeapon(AWeapon* Weapon);
	void DropOrDestroyWeapons();

	// Who is currently following us?
	UPROPERTY(BlueprintReadWrite, Category = "Squad")
	class AGASPCompanionCharacter* ActiveCompanion = nullptr;

	// --- SQUAD COMMANDS ---
	// The cached array of alive companions when the wheel opens
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	TArray<AGASPCompanionCharacter*> AvailableCompanions;

	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	AGASPCompanionCharacter* ActiveSelectedCompanion = nullptr;

	int32 SelectedCompanionIndex = 0;
	
	UFUNCTION(Client, Reliable)
	void RefreshHUDWithFullSquad();

	// === FACTION OVERRIDE ===
	virtual bool IsPlayerAlly() const override { return true; }
	
 protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UPawnNoiseEmitterComponent* NoiseEmitterComponent;

	UPROPERTY()
	UStealthComponent* StealthComponent = nullptr;
	
 	virtual void BeginPlay() override;
	
	virtual void OnDamageReceived(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser) override;

	// Poll for any relelvant classes and initialize our HUD
	void PollInit();

	virtual void OnRep_Health(float LastHealth) override;
	
	void SetSpawnPoint();
	void OnPlayerStateInitialized();

private:
	UPROPERTY()
	class AGASPCorePlayerState* GASPPlayerState;

	UPROPERTY()
	class AGASPCorePlayerController* GASPPlayerController;

#pragma region Multiplayer Variables
	
	bool bLeftGame = false;

	UPROPERTY(EditAnywhere)
	class UNiagaraSystem* CrownSystem;

	UPROPERTY()
	class UNiagaraComponent* CrownComponent;

#pragma endregion Multiplayer Variables 

#pragma region Interaction
	
	virtual void UpdateInteractionFocus() override;
	virtual void SetFocusedItem(AActor* NewItem) override;
	virtual void ClearFocusedItem() override;
	virtual void OnEquipItem_Implementation(AActor* Item) override;
	
#pragma endregion Interaction

	void ReturnToMainMenu();
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ShowOverheadHealth(float NewHealth);

public:
	FORCEINLINE bool IsElimmed() const { return bElimmed; }
 };