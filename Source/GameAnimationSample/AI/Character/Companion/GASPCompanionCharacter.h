#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/AI/GASPAITypes.h"
#include "GameAnimationSample/AI/Character/GASPAICharacterBase.h"
#include "GASPCompanionCharacter.generated.h"

class UDownedComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCompanionHealthChanged);

USTRUCT(BlueprintType)
struct FCompanionUIState
{
	GENERATED_BODY()

	UPROPERTY() ECompanionOrder CurrentOrder = ECompanionOrder::Follow;
	UPROPERTY() EAIState CurrentAIState = EAIState::Patrol;
	UPROPERTY() bool bWeaponsFree = true;
	UPROPERTY() FString LeaderName = TEXT("");
};

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPCompanionCharacter : public AGASPAICharacterBase
{
	GENERATED_BODY()

public:
	AGASPCompanionCharacter();

	UPROPERTY(BlueprintAssignable, Category = "Squad UI")
	FOnCompanionHealthChanged OnHealthChanged;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UDownedComponent* DownedComponent;

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionText_Implementation() const override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad UI")
	FString CompanionName = TEXT("Unknown Soldier");

	// === FACTION OVERRIDE ===
	virtual bool IsPlayerAlly() const override { return true; }
	
	UPROPERTY(Replicated)
	FCompanionUIState UIState;
	
protected:
	virtual void BeginPlay() override;
	virtual void HandleDeath(AController* InstigatorController, AActor* DamageCauser) override;
	virtual void OnDamageReceived(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser) override;
	virtual void OnRep_Health(float LastHealth) override;

	UFUNCTION()
	void OnRevived(AGASPBaseCharacter* RevivedCharacter);

	virtual void UpdateInteractionFocus() override;
	virtual void ClearFocusedItem() override;
	virtual void SetFocusedItem(AActor* NewItem) override;
};