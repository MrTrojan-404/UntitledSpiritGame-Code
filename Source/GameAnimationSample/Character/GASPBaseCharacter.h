// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/TimelineComponent.h"
#include "GameAnimationSample/Interface/AnimInterface.h"
#include "GameAnimationSample/Interface/CombatInterface.h"
#include "GameAnimationSample/Interface/HitInterface.h"
#include "GameAnimationSample/Interface/InteractionInterface.h"
#include "GameAnimationSample/Interface/PickupInterface.h"
#include "GameFramework/Character.h"
#include "GASPBaseCharacter.generated.h"

class UMoverComponent;
class UStealthComponent;
class UBaseAnimInstance;
class AWeapon;
class UCombatComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBaseHealthChanged);

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPBaseCharacter :  public APawn,
public ICombatInterface, public IAnimInterface, public IHitInterface,
public IInteractionInterface, public IPickupInterface
{
	GENERATED_BODY()

public:
	AGASPBaseCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(BlueprintReadWrite, Category = "Components")
	UCapsuleComponent* CapsuleComponent;

	UPROPERTY(BlueprintReadWrite, Category = "Components")
	USkeletalMeshComponent* CharacterMesh;
	
	UPROPERTY(BlueprintReadOnly, Category = "Motion Warping")
	class UMotionWarpingComponent* MotionWarpingComponent;

	// Mover component (added in Blueprint)
	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UMoverComponent* MoverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal")
	class UWallRunComponent* WallRunComponent;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Stealth")
	float TakedownWarpOffset = 100.f;

	TWeakObjectPtr<AGASPBaseCharacter> CurrentTakedownTarget;

	// The function Attacker's Anim Notify will call
	void ExecuteTakedownFX();

	// The Victim remembers who is assassinating them!
	TWeakObjectPtr<AGASPBaseCharacter> TakedownInstigator;

	// The function the Victim's AnimNotify will call
	void ExecuteVictimDeath();
	
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Stealth")
	UAnimMontage* AttackerTakedownMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Stealth")
	UAnimMontage* VictimTakedownMontage;

	// Called by an AnimNotify when the attacker is ready to move again
	void EndTakedownLock();
	
	UPROPERTY()
	TMap<FName, class UBoxComponent*> HitCollisionBoxes;

	FVector LastHitDirection;

	void DissolveTimerFinished();

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnBaseHealthChanged OnBaseHealthChanged;

	// --- CAMERA SHOULDER SWAP ---
	// The smoothed value that you will read in your Blueprint to send to the Camera
	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	float CurrentShoulderSide = 1.0f;

	// The raw target (-1.0 for Left, 1.0 for Right)
	UPROPERTY(BlueprintReadWrite, Category = "Camera")
	float TargetShoulderSide = 1.0f;

	// How fast the camera glides across the back
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float ShoulderSwapSpeed = 10.0f;

	// Call this from the Player Controller input
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SwapCameraShoulder();

	// Call this to explicitly force a side (used by the Wall Run)
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetCameraShoulder(float NewShoulderSide);
	
#pragma region CombatInterface

	virtual USkeletalMeshComponent* GetCombatMesh_Implementation() const override;
	virtual AActor* GetCombatOwner_Implementation() const override;
	virtual APawn* GetCombatPawn_Implementation() const override;
	virtual void PlayFireMontage_Implementation(bool bIsAiming) override;
	virtual UAnimMontage* GetReloadMontage_Implementation() override;
	virtual void PlayReloadMontage_Implementation() override;
	virtual void PlayThrowGrenadeMontage_Implementation() override;
	virtual UStaticMeshComponent* GetAttachedGrenade_Implementation() override;
	virtual void OnThrowGrenadeFinished_Implementation() override;
	virtual void OnLaunchGrenade_Implementation() override;
	virtual void OnFinishReload_Implementation() override;
	virtual void OnShotgunShellReload_Implementation() override;
	virtual void PlayMeleeMontage_Implementation() override;
	virtual void OnMeleeHit_Implementation() override;
	virtual void OnMeleeFinished_Implementation() override;
	virtual void PlayEquippedSwapMontage_Implementation() override;
	virtual void OnSwapEquippedFinished_Implementation() override;
	virtual void ShowSniperScopeCall_Implementation(bool bIsAiming) override;
	
#pragma endregion CombatInterface

#pragma region AnimInterface

	virtual bool GetAiming_Implementation() const override;
	virtual bool GetIsFiring_Implementation() const override;
	virtual UCombatComponent* GetCombatComponent_Implementation() override;
	virtual AWeapon* GetPrimaryWeapon_Implementation() override;
	virtual float GetAOPitch_Implementation() override;
	virtual float GetAOYaw_Implementation() override;
	virtual bool GetIsGameplayDisbled_Implementation() override;
	virtual EWeaponType GetEquippedWeaponType_Implementation() const override;
	virtual bool GetIsWallRunning_Implementation() const override;
	virtual bool GetIsRightWall_Implementation() const override;
	
#pragma endregion AnimInterface


	// StencilValue determines the color in your Post Process Material!
	void EnableTargetingOutline(bool bEnable, int32 StencilValue = 250);

	// The server uses this to tell a SPECIFIC player's screen to outline an enemy
	UFUNCTION(Client, Reliable)
	void Client_SetTargetOutline(AGASPBaseCharacter* TargetChar, bool bEnable, int32 StencilValue = 0);
	
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "State")
	bool bDisableGameplay = false;

	// The Brain for the AI to use when the Player isn't driving this body!
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI")
	class UBehaviorTree* DefaultBehaviorTree;

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionText_Implementation() const override;

	virtual void OnPickupBegin_Implementation(AActor* PickupActor) override;
	virtual void OnPickupEnd_Implementation(AActor* PickupActor) override;
	virtual void OnEquipItem_Implementation(AActor* Item) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "UI")
	FString CharacterName = TEXT("Unknown");

	UFUNCTION(BlueprintPure, Category = "UI")
	FString GetDisplayName() const { return CharacterName; }

	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void ShowSniperScopeWidget(bool bShowScope);

	UFUNCTION(BlueprintPure, Category = "Combat|Animation")
	bool GetIsCombatReady() const;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ResetCombatReady();

	// === MOUSE SENSITIVITY SETTINGS ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse")
	float BaseMouseSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse")
	float DefaultAimSensitivity = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse")
	float SniperAimSens = 0.2f;

	UPROPERTY(BlueprintReadWrite, Category = "Mouse")
	float ActiveMouseSensitivity = 1.0f;

	// === FACTION IDENTIFIER ===
	UFUNCTION(BlueprintCallable, Category = "Faction")
	virtual bool IsPlayerAlly() const { return false; }
	
	UPROPERTY(EditAnywhere)
	TSubclassOf<AWeapon> DefaultWeaponClass;

	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<AWeapon> DefaultSecondaryWeaponClass;

	UFUNCTION(NetMulticast, Reliable)
	virtual void Multicast_ResetToAlive();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class UInteractionComponent* InteractionComponent;

	
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
	UAnimMontage* MeleeMontage;

	virtual void ClearFocusedItem();

	float PreAimShoulder = 1.0f;

	// --- SLIDE BUFFERING ---
	// Tracks if the button is physically held down
	UPROPERTY(BlueprintReadWrite, Category = "Movement|Slide")
	bool bIsHoldingCrouch = false;

	// Tracks the exact time the button was pressed
	UPROPERTY(BlueprintReadOnly, Category = "Movement|Slide")
	float LastCrouchInputTime = -100.f;

	// How many seconds before landing can the player press slide? (0.25s is standard AAA)
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Slide")
	float SlideBufferWindow = 0.25f;

	// Asks: "Did they hold it, or tap it recently?"
	UFUNCTION(BlueprintCallable, Category = "Movement|Slide")
	bool IsSlideBuffered() const;

	// Call this from your Enhanced Input in Blueprint!
	UFUNCTION(BlueprintCallable, Category = "Movement|Slide")
	void SetCrouchInput(bool bPressed);

	// C++ calls this the exact frame we hit the ground if a slide was buffered!
	UFUNCTION(BlueprintImplementableEvent, Category = "Movement|Slide")
	void OnBufferedSlideLanding();

	UFUNCTION(Server, Reliable)
	void Server_RequestTakedown(class AGASPAICharacterBase* Victim);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UCombatComponent* CombatComponent;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	class UDashComponent* DashComponent;

	// Each character subclass Blueprint implements this to spawn its own trail —
	// water for Ren, fire burst for Abel, jetpack thruster for Sera.
	UFUNCTION(BlueprintImplementableEvent, Category = "Movement|Dash")
	void OnDashPerformed(FVector DashDirection);

	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	FVector CachedMovementInputVector = FVector::ZeroVector;
	
	UFUNCTION(BlueprintCallable, Category = "Medkit")
	void AddMedkit(int32 Amount = 1);

	UFUNCTION(BlueprintCallable, Category = "Medkit")
	void ConsumeMedkit();

	UFUNCTION(Server, Reliable)
	void Server_ConsumeMedkit();

	UFUNCTION(BlueprintPure, Category = "Medkit")
	int32 GetMedkitCount() const { return MedkitCount; }

	UPROPERTY(VisibleAnywhere, Category = "Components")
	class USpellComponent* SpellComponent;

	// Default true. Boss 3 (phased spirit creature) will override this to return false
	// while in its phased state — matches the GMD rule "Hydro Cage works in solid state only."
	UFUNCTION(BlueprintNativeEvent, Category = "Spell")
	bool CanBeCrowdControlled() const;
	virtual bool CanBeCrowdControlled_Implementation() const { return true; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Spell")
	void OnCaged(bool bStart);

	UFUNCTION(BlueprintImplementableEvent, Category = "Spell")
	void OnFrostLocked(bool bLocked); // per-target lock-on marker while held

	UFUNCTION(BlueprintImplementableEvent, Category = "Spell")
	void OnFrozen(bool bStart); // ice encasement VFX, freeze → shatter

	UFUNCTION(BlueprintImplementableEvent, Category = "Spell")
	void OnStasisActive(bool bActive);

	UFUNCTION(BlueprintImplementableEvent, Category = "Spell")
	void OnInfernoWave(FVector Direction);

	UFUNCTION(BlueprintImplementableEvent, Category = "Spell")
	void OnStaggered(bool bWasHeavy);

	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void BeginRagdoll(FVector ImpulseDirection, float ImpulseStrength); // entry only

	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void RecoverFromRagdoll(); // exit only, callable anytime

	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void EnterTemporaryRagdoll(FVector ImpulseDirection, float ImpulseStrength, float RecoverAfterSeconds); // convenience: Begin + auto-timed Recover

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetFrozen(bool bFrozen, class UMaterialInterface* FreezeMaterial = nullptr, class UNiagaraSystem* FreezeVFX = nullptr);
	
	UPROPERTY(ReplicatedUsing = OnRep_Health, VisibleAnywhere, Category = "Player Stats")
	float Health = 100.f;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayTagTeamDissolve(bool bDissolveOut);

	UPROPERTY()
	bool bIsTagTeamDissolving = false;
	float TargetTagTeamDissolve = -0.5f;
	float CurrentTagTeamDissolve = -0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tag Team")
	float TagTeamDissolveSpeed = 2.0f;
	
protected:
	virtual void BeginPlay() override;

	virtual void Destroyed() override;

	UPROPERTY()
	UBaseAnimInstance* CachedAnimInstance;
	
	UFUNCTION()
	void ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
				   AController* InstigatorController,
				   AActor* DamageCauser);

	// To be overriden by child class (For HUD Updates or any other functionality)
	virtual void OnDamageReceived(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser) {}

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	// Tracks the specific bone hit for death ragdolls
	UPROPERTY()
	FName LastHitBone = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float RagdollDeathImpulse = 5000.f;

	void DrawMannyBodiesDebug();
	// The shared physics function
	void ApplyDeathPhysics();

	UPROPERTY(VisibleAnywhere)
	class ULagCompensationComponent* LagCompensation;
	
	UPROPERTY()
	class AGASPCoreGameMode* GASPGameMode;
	
	virtual void SpawnDefaultWeapon(bool bSetToDestroyWeapon, bool bElimmedCheck);

	
	/** 
* Hit boxes used for server-side rewind
*/
#pragma region SSR HitBoxes

	/*
	UPROPERTY(EditAnywhere)
	class UBoxComponent* head;

	UPROPERTY(EditAnywhere)
	UBoxComponent* pelvis;

	UPROPERTY(EditAnywhere)
	UBoxComponent* spine_02;

	UPROPERTY(EditAnywhere)
	UBoxComponent* spine_03;

	UPROPERTY(EditAnywhere)
	UBoxComponent* upperarm_l;

	UPROPERTY(EditAnywhere)
	UBoxComponent* upperarm_r;

	UPROPERTY(EditAnywhere)
	UBoxComponent* lowerarm_l;

	UPROPERTY(EditAnywhere)
	UBoxComponent* lowerarm_r;

	UPROPERTY(EditAnywhere)
	UBoxComponent* hand_l;

	UPROPERTY(EditAnywhere)
	UBoxComponent* hand_r;

	UPROPERTY(EditAnywhere)
	UBoxComponent* thigh_l;

	UPROPERTY(EditAnywhere)
	UBoxComponent* thigh_r;

	UPROPERTY(EditAnywhere)
	UBoxComponent* calf_l;

	UPROPERTY(EditAnywhere)
	UBoxComponent* calf_r;

	UPROPERTY(EditAnywhere)
	UBoxComponent* foot_l;

	UPROPERTY(EditAnywhere)
	UBoxComponent* foot_r;*/

#pragma endregion SSR HitBoxes
	
	/**
* Player health
*/

	UPROPERTY(EditAnywhere, Category = "Player Stats")
	float MaxHealth = 100.f;

	UFUNCTION()
	virtual  void OnRep_Health(float LastHealth);


	/**
* Dissolve effect
*/
	
#pragma region Dissolve

	FTimerHandle DissolveTImer;

	UPROPERTY(EditDefaultsOnly)
	float DissolveDelay = 3.f;
	
	UPROPERTY(VisibleAnywhere)
	UTimelineComponent* DissolveTimeline;
	FOnTimelineFloat DissolveTrack;

	UPROPERTY(EditAnywhere)
	UCurveFloat* DissolveCurve;

	UFUNCTION()
	void UpdateDissolveMaterial(float DissolveValue);
	void StartDissolve();

	// Dynamic instance that we can change at runtime
	UPROPERTY(VisibleAnywhere, Category = Elim)
	UMaterialInstanceDynamic* DynamicDissolveMaterialInstance01;

	UPROPERTY(VisibleAnywhere, Category = Elim)
	UMaterialInstanceDynamic* DynamicDissolveMaterialInstance02;
	
	// Material instance set on the Blueprint, used with the dynamic material instance
	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* DissolveMaterialInstance01;
	
	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* DissolveMaterialInstance02;

	/** 
	* Team colors
	*/

	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* RedDissolveMatInst;

	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* RedMaterial;

	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* BlueDissolveMatInst;

	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* BlueMaterial;

	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* OriginalMaterial;
	
#pragma endregion Dissolve


	/** 
* Animation montages
*/
#pragma region Montage
	
	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* FireWeaponMontage;
	
	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* ThrowGrenadeMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Montages")
	UAnimMontage* SwapMontage;

#pragma endregion Montage

	/** 
* Grenade
*/

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* AttachedGrenade;

	/** 
* Default weapon
*/

	void CalculateAO_Pitch();
	float AO_Pitch;
	float AO_Yaw;
	FRotator StartingAimRotation;

	// The RPC to broadcast the hit reaction to all clients
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHitReact(FVector_NetQuantizeNormal HitDirection, FName HitBone);

	virtual void UpdateInteractionFocus();
	virtual void SetFocusedItem(AActor* NewItem);

	UFUNCTION(Server, Reliable)
	void ServerCancelRevive(AActor* DownedTarget);

	UPROPERTY(ReplicatedUsing = OnRep_MedkitCount, BlueprintReadOnly, Category = "Medkit")
	int32 MedkitCount = 0;

	UFUNCTION()
	void OnRep_MedkitCount();

	UPROPERTY(EditDefaultsOnly, Category = "Medkit")
	int32 MaxMedkitCount = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Medkit")
	float MedkitHealAmount = 50.f;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BeginRagdoll(FVector ImpulseDirection, float ImpulseStrength);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_RecoverFromRagdoll();

	UPROPERTY()
	class UNiagaraComponent* ActiveFrozenVFX;
	
private:
	FTimerHandle TemporaryRagdollTimer;
	
public:
	FORCEINLINE UCombatComponent* GetCombatComponent() const { return CombatComponent; }
	FORCEINLINE ULagCompensationComponent* GetLagCompensation() const { return LagCompensation; }
	FORCEINLINE float GetHealth() const { return Health; }
	FORCEINLINE void SetHealth(float Amount) { Health = Amount; }
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }
	
};