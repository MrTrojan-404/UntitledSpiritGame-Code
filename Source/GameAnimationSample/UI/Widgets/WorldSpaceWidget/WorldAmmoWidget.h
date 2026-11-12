#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WorldAmmoWidget.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UWorldAmmoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* AmmoText;

	void SetAmmoText(int32 CurrentAmmo, int32 CarriedAmmo);
};