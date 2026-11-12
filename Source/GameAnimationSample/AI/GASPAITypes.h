#pragma once

#include "CoreMinimal.h"
#include "GASPAITypes.generated.h"

UENUM(BlueprintType)
enum class EAIState : uint8
{
	Patrol      UMETA(DisplayName = "Patrol"),
	Alert       UMETA(DisplayName = "Alert"),
	Engage      UMETA(DisplayName = "Engage"),
	Dead        UMETA(DisplayName = "Dead")
};

UENUM(BlueprintType)
enum class ECompanionOrder : uint8
{
	Follow          UMETA(DisplayName = "Follow Leader"),
	StandBy         UMETA(DisplayName = "Stand By"),
	MoveToLocation  UMETA(DisplayName = "Move To Location"),
	WeaponsFree     UMETA(DisplayName = "Weapons Free"),
	HoldFire        UMETA(DisplayName = "Hold Fire"),
	ReviveAlly      UMETA(DisplayName = "Revive Ally"),
	AttackTarget    UMETA(DisplayName = "Attack Target"),
};

UENUM(BlueprintType)
enum class EAIMovementSpeed : uint8
{
	Walk    UMETA(DisplayName = "Walk"),
	Jog     UMETA(DisplayName = "Jog"),
	Sprint  UMETA(DisplayName = "Sprint")
};