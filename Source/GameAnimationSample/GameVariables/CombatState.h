#pragma once

UENUM(BlueprintType)
enum class ECombatState : uint8
{
	ECS_Unoccupied UMETA(DisplayName = "Unoccupied"),
	ECS_Reloading UMETA(DisplayName = "Reloading"),
	ECS_ThrowingGrenade UMETA(DisplayName = "Throwing Grenade"),
	ECS_SwappingWeapons UMETA(DisplayName = "Swapping Weapons"),
	ECS_Melee UMETA(DisplayName = "Melee"),


	ECS_MAX UMETA(DisplayName = "DefaultMAX")
};

UENUM(BlueprintType)
enum class EPlayerWeaponState : uint8
{
	EPWS_HasNoWeapon UMETA(DisplayName = "HasNoWeapon"),
	EPWS_HasOneWeapon UMETA(DisplayName = "HasOneWeapon"),
	EPWS_HasTwoWeapons UMETA(DisplayName = "HasTwoWeapons"),
	
	EPWS_MAX UMETA(DisplayName = "DefaultMAX")

};