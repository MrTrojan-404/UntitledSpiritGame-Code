#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GASPAirlockEntrance.generated.h"

class UBoxComponent;
class AGASPArenaManager;

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPAirlockEntrance : public AActor
{
	GENERATED_BODY()
	
public:	
	AGASPAirlockEntrance();

protected:
	virtual void BeginPlay() override;

	// The room between Door A and Door B
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* AirlockVolume;

	// The room past Door B (The actual arena)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* ArenaTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UArrowComponent* TeleportDropPoint;
	
	UPROPERTY(EditInstanceOnly, Category = "Boss Arena")
	AGASPArenaManager* ArenaManager;

	UFUNCTION()
	void OnAirlockOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnArenaOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// --- Visual Events for your Blueprints ---
	UFUNCTION(BlueprintImplementableEvent, Category = "Airlock")
	void BlueprintCloseDoorA();

	UFUNCTION(BlueprintImplementableEvent, Category = "Airlock")
	void BlueprintOpenDoorB();

	UFUNCTION(BlueprintImplementableEvent, Category = "Airlock")
	void BlueprintCloseDoorB();

	// --- Network Multicasts to sync visuals for all players ---
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Stage1_LockAirlock();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Stage2_StartFight();

	// Place a standard "Target Point" actor in your hidden room and assign it here!
	UPROPERTY(EditInstanceOnly, Category = "Tag Team")
	AActor* BanishPointActor;
	
private:
	// A helper to dynamically count if every active player is inside a specific box
	bool AreAllPlayersInVolume(UBoxComponent* Volume);

	bool bAirlockLocked = false;
	bool bFightStarted = false;
};