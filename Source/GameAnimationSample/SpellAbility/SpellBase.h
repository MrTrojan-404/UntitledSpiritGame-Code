#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SpellBase.generated.h"

class AGASPBaseCharacter;

UCLASS(Abstract, Blueprintable, EditInlineNew)
class GAMEANIMATIONSAMPLE_API USpellBase : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Spell")
	void Execute(AGASPBaseCharacter* Caster);
	virtual void Execute_Implementation(AGASPBaseCharacter* Caster) {}

	// Cost in Energy segments. Ultimate slot ignores this — it's gated by the separate kill-only bar instead.
	UPROPERTY(EditDefaultsOnly, Category = "Spell")
	int32 EnergySegmentCost = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Spell")
	FName SpellDisplayName;

	UFUNCTION(BlueprintNativeEvent, Category = "Spell")
	void OnReleased(AGASPBaseCharacter* Caster);
	virtual void OnReleased_Implementation(AGASPBaseCharacter* Caster) {}
	
};