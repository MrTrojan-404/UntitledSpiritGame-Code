// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GASPCorePlayerController.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHighPingDelegate, bool, bPingTooHigh);

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPCorePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AGASPCorePlayerController();
	
	// --- UNIVERSAL HUD ---
	void SetHUDHealth(float Health, float MaxHealth);
	void SetHUDWeaponAmmo(int32 Ammo);
	void SetHUDCarriedAmmo(int32 Ammo);
	void SetHUDGrenades(int32 Grenades);

	virtual void OnPossess(APawn* InPawn) override;
    
	// --- PING & SYSTEM ---
	FHighPingDelegate HighPingDelegate;
	virtual float GetServerTime();
	void ShowReturnToMainMenu();
    
	UFUNCTION(Client, Unreliable)
	void Client_ShowHitMarker(bool bHeadshot);

	UFUNCTION(Client, Reliable)
	void ClientNotifyMissionState(FName State);

	float SingleTripTime = 0.f;

	//PAUSE MENU COMMANDS
	UFUNCTION(Server, Reliable)
	void ServerRestartLevel();

	UFUNCTION(Server, Reliable)
	void ServerRestartCheckpoint();

	
	UFUNCTION(Server, Reliable)
	void ServerSwitchCharacter();

	void WeaponWheelStarted();
	void WeaponWheelReleased();

	// The RPC that tells the client's screen to move
	UFUNCTION(Client, Reliable)
	void Client_FinishCharacterSwap(APawn* NewPawn);

	UFUNCTION(Client, Reliable)
	void Client_RefreshSquadUI();

	// Pushes the Server's active objective to a late-joining client
	UFUNCTION(Client, Reliable)
	void Client_SyncObjective(class AObjectiveActor* ActiveObjective);

	// Tells the client they are dead and should show the spectator screen
	UFUNCTION(Client, Reliable)
	void Client_EnterSpectatorMode();

	/** Tells the client HUD what the highest enemy detection level currently is. */
	UFUNCTION(Client, Unreliable)
	void Client_UpdateDetectionHUD(AActor* DetectingEnemy, float DetectionLevel, uint8 AlertPhaseRaw);
	
	/** Routes the suppressor toggle to the server (player may be on a client). */
	UFUNCTION(Server, Reliable)
	void Server_ToggleSuppressor();

	UPROPERTY(EditDefaultsOnly, Category = "Squad|Mark")
	float MarkTraceRange = 3000.f;

	// --- ABILITY HUD ---
	void SetHUDUltimate(float CurrentCharge, float MaxCharge);
	void SetHUDEnergy(int32 CurrentSegments, int32 MaxSegments);

	void SetHUDMedkits(int32 Medkits);

	// Tells the local player's screen to start the Mend cooldown UI
	UFUNCTION(Client, Reliable)
	void Client_StartMendCooldown(float CooldownDuration);

	UPROPERTY(BlueprintReadWrite, Category = "Tag Team")
	bool bTagTeamMode = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tag Team")
	FVector BanishLocation;

	UPROPERTY(EditDefaultsOnly, Category = "Tag Team")
	class UNiagaraSystem* TagTeamSwitchVFX;

	UPROPERTY(EditDefaultsOnly, Category = "Tag Team")
	class USoundBase* TagTeamSwitchSFX;
	
	UFUNCTION(Server, Reliable)
	void ServerTagTeamSwitch();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tag Team")
	float TagTeamCooldown = 5.0f;

	bool bTagTeamOnCooldown = false;
	FTimerHandle TagTeamCooldownTimer;
	void ClearTagTeamCooldown();

	
protected:
	virtual void SetupInputComponent() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad")
	class USquadCommandComponent* SquadCommandComponent;

#pragma region Inputs
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    class UInputAction* InteractAction;
 
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* FireAction;
 
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* ReloadAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* SwapWeaponAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* DropAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* AimAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* ThrowGrenadeAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* ThrowDistractionAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* PauseAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* ScoreboardAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* MeleeAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* CommandAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* SpecificCommandAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* WeaponWheelAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputAction* SwitchCharacterAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputAction* ToggleSuppressorAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputAction* SwapShoulderAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* MarkTargetAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* ToggleFollowAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* DashAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* ConsumeMedkitAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* Spell1Action;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* Spell2Action;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* UltimateAction;

	void Spell1ButtonPressed();
	void Spell2ButtonPressed();
	void Spell2ButtonReleased();
	void UltimateButtonPressed();
	
	void ConsumeMedkitButtonPressed();
	void DashButtonPressed();
	void ToggleFollowButtonPressed();
    void InteractButtonPressed();
    void InteractButtonReleased();
    void SwapButtonPressed();
    void DropButtonPressed();
    void ReloadButtonPressed();
    void ThrowGrenadeButtonPressed();
	void ThrowDistractionButtonPressed();
	void MeleeButtonPressed();
	void SwapShoulderButtonPressed();

	void AimButtonPressed();
    void AimButtonReleased();
    
    void FireButtonPressed();
    void FireButtonReleased();
	
	void MarkTargetButtonPressed();
	
	void SwitchCharacterButtonPressed();

	void ToggleSuppressorButtonPressed();
	
#pragma endregion Inputs
	
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void ReceivedPlayer() override;
	virtual void PollInit();
	virtual void SetupInitialHUD();
	
	// --- HUD & WIDGETS ---
	UPROPERTY()
	class AGASPHUD* GASPHUD;

	UPROPERTY()
	class UCharacterOverlay* CharacterOverlay;
	
	bool bInitializeCharacterOverlay = false;

	// --- CORE HUD VALUES ---
	float HUDHealth;
	float HUDMaxHealth;
	int32 HUDGrenades;
	
	float HUDCarriedAmmo;
	bool bInitializeCarriedAmmo = false;
	
	float HUDWeaponAmmo;
	bool bInitializeWeaponAmmo = false;

	// --- MAIN MENU ---
	UPROPERTY(EditAnywhere, Category = HUD)
	TSubclassOf<class UUserWidget> ReturnToMainMenuWidget;

	UPROPERTY()
	class UReturnToMainMenu* ReturnToMainMenu;

	// --- TIME SYNC ---
	float ClientServerDelta = 0.f;
	UPROPERTY(EditAnywhere, Category = Time) float TimeSyncFrequency = 5.f;
	float TimeSyncRunningTime = 0.f;
	void CheckTimeSync(float DeltaTime);

	UFUNCTION(Server, Reliable)
	void ServerRequestServerTime(float TimeOfClientRequest);

	UFUNCTION(Client, Reliable)
	void ClientReportServerTime(float TimeOfClientRequest, float TimeServerReceivedClientRequest);

	// --- PING ---
	float HighPingRunningTime = 0.f;
	UPROPERTY(EditAnywhere) float HighPingDuration = 5.f;
	float PingAnimationRunningTime = 0.f;
	UPROPERTY(EditAnywhere) float CheckPingFrequency = 20.f;
	UPROPERTY(EditAnywhere) float HighPingThreshold = 50.f;

	void HighPingWarning();
	void StopHighPingWarning();
	void CheckPing(float DeltaTime);

	UFUNCTION(Server, Reliable)
	void ServerReportPingStatus(bool bHighPing);

	// --- AUDIO ---
	UPROPERTY(EditAnywhere)
	USoundBase* HitMarkerSound;

	// The Event you can use in Blueprint to trigger your "Setup Camera" nodes!
	UFUNCTION(BlueprintImplementableEvent, Category = "Squad")
	void OnClientCharacterSwapped(APawn* NewPawn);

	// --- WEAPON WHEEL UI ---
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<class UWeaponRadialMenu> WeaponWheelClass;

	UPROPERTY()
	UWeaponRadialMenu* WeaponWheelInstance;

private:
	void PushHUDFromPawn(APawn* InPawn);

	float StealthHUDUpdateTimer = 0.f;
	static constexpr float StealthHUDUpdateInterval = 0.1f;   // 10 Hz
	
};