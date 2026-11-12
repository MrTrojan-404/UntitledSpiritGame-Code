#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/SpellAbility/SpellBase.h"
#include "ArcDartSpell.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UArcDartSpell : public USpellBase
{
	GENERATED_BODY()

public:
	UArcDartSpell()
	{
		EnergySegmentCost = 1; 
		SpellDisplayName = TEXT("Arc Dart");
	}

	virtual void Execute_Implementation(AGASPBaseCharacter* Caster) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Arc Dart")
	TSubclassOf<class AArcDartProjectile> DartProjectileClass;

};