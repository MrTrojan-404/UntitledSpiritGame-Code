#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WallRunComponent.generated.h"

class UMoverComponent;
class AGASPBaseCharacter;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class GAMEANIMATIONSAMPLE_API UWallRunComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UWallRunComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Called by Player Input or AI Behavior Tree to eject from the wall
	UFUNCTION(BlueprintCallable, Category = "Traversal|WallRun")
	void WallJump();

	void ExecuteWallJump(FVector CameraForward);

	UFUNCTION(Server, Reliable)
	void ServerWallJump(FVector ClientCameraForward);
	
	UFUNCTION(BlueprintPure, Category = "Traversal|WallRun")
	bool IsWallRunning() const { return bIsWallRunning; }

	UFUNCTION(BlueprintPure, Category = "Traversal|WallRun")
	bool IsRightWall() const { return bIsRightWall; }

	// Called from Blueprint to tell the component if 'W' is pressed
	UFUNCTION(BlueprintCallable, Category = "Traversal|WallRun")
	void SetIsHoldingForward(bool bHolding);

	// Tells the Server that 'W' was pressed/released
	UFUNCTION(Server, Reliable)
	void ServerSetIsHoldingForward(bool bHolding);
	
	// --- ANIMATIONS ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Traversal|WallRun|Animation")
	UAnimMontage* RightWallRunMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Traversal|WallRun|Animation")
	UAnimMontage* LeftWallRunMontage;

	// Speed cap applied the moment you fall off a wall (air, non-jump exit).
	// Set slightly above your sprint speed so the transition feels natural.
	UPROPERTY(EditDefaultsOnly, Category = "Traversal|WallRun|Ground")
	float AirExitMaxSpeed = 650.f;

	// Multiplier for gravity while on a wall (1.0 = normal gravity, 0.5 = half gravity, 0.0 = horizontal)
	UPROPERTY(EditDefaultsOnly, Category = "Traversal|WallRun")
	float WallRunGravityScale = 0.4f;

	UFUNCTION(BlueprintCallable, Category = "Traversal|WallRun")
	void OnJumpPressed();

	UFUNCTION(Server, Reliable)
	void ServerStartWallRun(FVector WallNormal, FVector WallHitPoint, bool bRightWall);

	UFUNCTION(Server, Reliable)
	void ServerExitWallRun(bool bFromJump);
	
protected:
	virtual void BeginPlay() override;

	void CheckForWall();
	void StartWallRun(const FVector& WallNormal, const FVector& WallHitPoint, bool bRightWall);
	void ExitWallRun(bool bFromJump = false);

public:
	// --- CONFIGURATION ---
	UPROPERTY(EditDefaultsOnly, Category = "Traversal|WallRun")
	float TraceDistance = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Traversal|WallRun")
	float MinWallSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Traversal|WallRun")
	float WallRunSpeed = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Traversal|WallRun")
	float WallRunAdhesionSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Traversal|WallRun")
	float WallRunUpAssist = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Traversal|WallRun")
	float WallRunCooldown = 0.2f;

	// --- JUMP SETTINGS ---
	UPROPERTY(EditDefaultsOnly, Category = "Traversal|WallRun|Jump")
	float JumpOutSpeed = 700.f;

	UPROPERTY(EditDefaultsOnly, Category = "Traversal|WallRun|Jump")
	float JumpUpSpeed = 500.f;

	// --- DEBUG ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|WallRun|Debug")
	bool bShowDebugTraces = true;

	// The speed you want to snap down to when landing (Set this to match your character's Max Sprint Speed)
	UPROPERTY(EditDefaultsOnly, Category = "Traversal|WallRun|Ground")
	float GroundLandingSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Traversal|WallRun")
	float MinWallRunCheckDelay = 0.12f; // seconds before we test if the wall is still there
	
private:
	UPROPERTY()
	AGASPBaseCharacter* CharacterOwner;

	UPROPERTY()
	UMoverComponent* MoverComp;

	bool bIsWallRunning = false;
	bool bIsRightWall = false;
	FVector CurrentWallNormal;
	FVector CurrentWallTangent;
	float LastWallRunEndTime = 0.f;
	bool bIsHoldingForward = false;
	float WallRunStartTime = 0.f;
	bool bCanWallRun = true;
	// Tracks if we need to bleed off momentum when we touch the ground
	bool bDidWallRunThisJump = false;

	// Remembers the player's preferred camera shoulder before the wall run started
	float PreWallRunShoulder = 1.0f;
};