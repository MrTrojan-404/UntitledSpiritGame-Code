#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/GameVariables/WeaponType.h"
#include "GameAnimationSample/Actor/Weapon.h"
#include "GameAnimationSample/Objective/ObjectiveTypes.h"
#include "CheckpointTypes.generated.h"

class AGASPBaseCharacter;
class AGASPCorePlayerController;
class AGASPCompanionCharacter;
class AGASPEnemyBase;

UENUM(BlueprintType)
enum class ECheckpointType : uint8
{
    Snapshot    UMETA(DisplayName = "Snapshot (Rewinds Time)"),
    Persistent  UMETA(DisplayName = "Persistent (Respawn Only)")
};

UENUM(BlueprintType)
enum class ERespawnRule : uint8
{
    SquadWipeOnly UMETA(DisplayName = "Squad Wipe Triggers Restore"),
    IndividualRespawn UMETA(DisplayName = "Individual Respawn (New-U Station)") 
};

USTRUCT(BlueprintType)
struct FWeaponSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    TSoftClassPtr<AWeapon> WeaponClass = nullptr;

    UPROPERTY()
    int32 CurrentAmmo = 0;
};

USTRUCT(BlueprintType)
struct FWorldWeaponSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    TSoftClassPtr<AWeapon> WeaponClass = nullptr;

    UPROPERTY()
    FVector Location = FVector::ZeroVector;

    UPROPERTY()
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY()
    int32 CurrentAmmo = 0;

    UPROPERTY()
    EWeaponState SavedState = EWeaponState::EWS_Initial;
};

// ENEMY SAVE DATA
USTRUCT(BlueprintType)
struct FEnemySaveData
{
    GENERATED_BODY()

    UPROPERTY()
    TSoftClassPtr<AGASPEnemyBase> EnemyClass = nullptr;

    UPROPERTY()
    TSoftClassPtr<AWeapon> EquippedWeaponClass = nullptr;

    UPROPERTY()
    FVector Location = FVector::ZeroVector;

    UPROPERTY()
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY()
    FVector PatrolOrigin = FVector::ZeroVector;

    UPROPERTY()
    float Health = 100.f;

    UPROPERTY()
    bool bWasAlive = false;

    UPROPERTY()
    FName EventTag = NAME_None;

    UPROPERTY()
    class AGASPWaypointPath* WaypointPath = nullptr;
};

// COMPANION SAVE DATA 
USTRUCT(BlueprintType)
struct FCompanionSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    AGASPBaseCharacter* CompanionActor = nullptr;

    // To find the companion on reboot
    UPROPERTY()
    FString CompanionNameString = TEXT("");
    
    UPROPERTY()
    float Health = 100.f;

    UPROPERTY()
    bool bIsDowned = false;

    UPROPERTY()
    FWeaponSaveData PrimaryWeapon;

    UPROPERTY()
    FWeaponSaveData SecondaryWeapon;

    UPROPERTY()
    bool bHasPrimary = false;

    UPROPERTY()
    bool bHasSecondary = false;

    UPROPERTY()
    TArray<FWeaponSaveData> SavedInventory;

    UPROPERTY()
    TMap<EWeaponType, int32> CarriedAmmoMap;
};

USTRUCT(BlueprintType)
struct FPlayerSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    AGASPCorePlayerController* PlayerController = nullptr;

    //The exact body the human was driving when they saved
    UPROPERTY()
    AGASPBaseCharacter* PossessedBody = nullptr;

    // To find the player on reboot
    UPROPERTY()
    FString PlayerIDString = TEXT("");

    UPROPERTY()
    FString SavedCharacterName = TEXT("");
    
    //So the player's downed state is remembered too!
    UPROPERTY()
    bool bIsDowned = false;
    
    UPROPERTY()
    float Health = 100.f;

    UPROPERTY()
    FWeaponSaveData PrimaryWeapon;

    UPROPERTY()
    FWeaponSaveData SecondaryWeapon;

    UPROPERTY()
    bool bHasPrimary = false;

    UPROPERTY()
    bool bHasSecondary = false;

    UPROPERTY()
    int32 GrenadeCount = 0;

    // INVENTORY ARRAY
    UPROPERTY()
    TArray<FWeaponSaveData> SavedInventory;

    UPROPERTY() 
    TMap<EWeaponType, int32> CarriedAmmoMap;
};

USTRUCT(BlueprintType)
struct FCheckpointSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    int32 CheckpointIndex = 0;

    UPROPERTY()
    TArray<FPlayerSaveData> Players;

    UPROPERTY()
    TArray<FCompanionSaveData> Companions;

    UPROPERTY()
    TArray<FEnemySaveData> Enemies;

    UPROPERTY()
    TArray<FWorldWeaponSaveData> WorldWeapons;

    UPROPERTY()
    FVector RestoreLocation = FVector::ZeroVector;

    UPROPERTY()
    FRotator RestoreRotation = FRotator::ZeroRotator;

    UPROPERTY()
    FName ActiveObjectiveID = NAME_None;

    UPROPERTY() 
    TMap<FName, EObjectiveState> ObjectiveStates;

    UPROPERTY()
    TMap<FName, FObjectiveConditionData> ObjectiveProgress;

    bool IsValid() const
    {
        return Players.Num() > 0;
    }
};