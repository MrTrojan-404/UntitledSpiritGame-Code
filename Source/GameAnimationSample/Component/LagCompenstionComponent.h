// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LagCompenstionComponent.generated.h"


class AGASPBaseCharacter;

USTRUCT(BlueprintType)
struct FBoxInformation
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location;

	UPROPERTY()
	FRotator Rotation;

	UPROPERTY()
	FVector BoxExtent;
};

USTRUCT(BlueprintType)
struct FFramePackage
{
	GENERATED_BODY()

	UPROPERTY()
	float Time;

	UPROPERTY()
	TMap<FName, FBoxInformation> HitBoxInfo;

	UPROPERTY()
	AGASPBaseCharacter* Character;
};


USTRUCT(BlueprintType)
struct FServerSideRewindResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bHitConfirmed;

	UPROPERTY()
	bool bHeadShot;
};


USTRUCT(BlueprintType)
struct FShotgunServerSideRewindResult
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<AGASPBaseCharacter*, uint32> HeadShots;

	UPROPERTY()
	TMap<AGASPBaseCharacter*, uint32> BodyShots;

};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class GAMEANIMATIONSAMPLE_API ULagCompensationComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	ULagCompensationComponent();
	friend class AGASPBaseCharacter;
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void ShowFramePackage(const FFramePackage& Package, const FColor& Color);

	
	/** 
	* Hitscan
	*/
	FServerSideRewindResult ServerSideRewind(
		  class AGASPBaseCharacter* HitCharacter, 
		  const FVector_NetQuantize100& TraceStart,
		  const FVector_NetQuantize100& HitLocation,
		  float HitTime);

	UFUNCTION(Server, Reliable)
	void ServerScoreRequest(
	   AGASPBaseCharacter* HitCharacter,
	   const FVector_NetQuantize100& TraceStart,
	   const FVector_NetQuantize100& HitLocation,
	   float HitTime);

	
	/** 
* Projectile
*/
	FServerSideRewindResult ProjectileServerSideRewind(
	   AGASPBaseCharacter* HitCharacter,
	   const FVector_NetQuantize100& TraceStart,
	   const FVector_NetQuantize100& InitialVelocity,
	   float HitTime
	);

	UFUNCTION(Server, Reliable)
	void ProjectileServerScoreRequest(
	   AGASPBaseCharacter* HitCharacter,
	   const FVector_NetQuantize100& TraceStart,
	   const FVector_NetQuantize100& InitialVelocity,
	   float HitTime
	);

	
	/** 
	* Shotgun
	*/
	FShotgunServerSideRewindResult ShotgunServerSideRewind(
		const TArray<AGASPBaseCharacter*>& HitCharacters, 
		const FVector_NetQuantize100& TraceStart, 
		const TArray<FVector_NetQuantize100>& HitLocations, 
		float HitTime);

	
	
	UFUNCTION(Server, Reliable)
	void ShotgunServerScoreRequest(
		const TArray<AGASPBaseCharacter*>& HitCharacters,
		const FVector_NetQuantize100& TraceStart,
		const TArray<FVector_NetQuantize100>& HitLocations,
		float HitTime
	);
protected:
	virtual void BeginPlay() override;
	void SaveFramePackage(FFramePackage& Package);
	FFramePackage InterpBetweenFrames(const FFramePackage& OlderFrame, const FFramePackage& YoungerFrame, float HitTime);
	void CacheBoxPositions(AGASPBaseCharacter* HitCharacter, FFramePackage& OutFramePackage);
	void MoveBoxes(AGASPBaseCharacter* HitCharacter, const FFramePackage& Package);
	void ResetHitBoxes(AGASPBaseCharacter* HitCharacter, const FFramePackage& Package);
	void EnableCharacterMeshCollision(const AGASPBaseCharacter* HitCharacter, ECollisionEnabled::Type CollisionEnabled);
	void SaveFramePackage();
	FFramePackage GetFrameToCheck(AGASPBaseCharacter* HitCharacter, float HitTime);

	/** 
	* Hitscan
	*/
	FServerSideRewindResult ConfirmHit(
		const FFramePackage& Package,
		AGASPBaseCharacter* HitCharacter,
		const FVector_NetQuantize100& TraceStart,
		const FVector_NetQuantize100& HitLocation);

	/** 
	* Projectile
	*/
	FServerSideRewindResult ProjectileConfirmHit(
		const FFramePackage& Package,
		AGASPBaseCharacter* HitCharacter,
		const FVector_NetQuantize100& TraceStart,
		const FVector_NetQuantize100& InitialVelocity,
		float HitTime
	);

	
	/** 
	* Shotgun
	*/

	FShotgunServerSideRewindResult ShotgunConfirmHit(
		const TArray<FFramePackage>& FramePackages,
		const FVector_NetQuantize100& TraceStart,
		const TArray<FVector_NetQuantize100>& HitLocations
	);
	
private:

	UPROPERTY()
	AGASPBaseCharacter* Character;

	UPROPERTY()
	class AGASPCorePlayerController* Controller;

	
	TDoubleLinkedList<FFramePackage> FrameHistory;

	UPROPERTY(EditAnywhere)
	float MaxRecordTime = 4.f;

		
};
