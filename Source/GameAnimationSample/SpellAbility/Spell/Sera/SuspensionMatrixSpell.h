#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/SpellAbility/SpellBase.h"
#include "SuspensionMatrixSpell.generated.h"

class ASuspensionMatrixProjectile;

UCLASS()
class GAMEANIMATIONSAMPLE_API USuspensionMatrixSpell : public USpellBase
{
	GENERATED_BODY()

public:
	USuspensionMatrixSpell()
	{
		EnergySegmentCost = 0; // Ultimate
	}

	virtual void Execute_Implementation(AGASPBaseCharacter* Caster) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Suspension Matrix")
	TSubclassOf<ASuspensionMatrixProjectile> MatrixProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Suspension Matrix|Audio")
	USoundBase* CastSound;
	
};