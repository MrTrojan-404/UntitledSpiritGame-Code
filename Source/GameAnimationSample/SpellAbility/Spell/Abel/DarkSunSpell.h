#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/SpellAbility/SpellBase.h"
#include "DarkSunSpell.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UDarkSunSpell : public USpellBase
{
	GENERATED_BODY()

public:
	UDarkSunSpell()
	{
		// EnergySegmentCost is ignored by your SpellComponent for Ultimates, 
		// but we set it to 0 for clarity.
		EnergySegmentCost = 0; 
	}

	virtual void Execute_Implementation(AGASPBaseCharacter* Caster) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Dark Sun")
	TSubclassOf<class ADarkSunOrb> OrbClass;

	UPROPERTY(EditDefaultsOnly, Category = "Dark Sun")
	float SpawnDistance = 150.f; // Spawn slightly in front of Abel
};