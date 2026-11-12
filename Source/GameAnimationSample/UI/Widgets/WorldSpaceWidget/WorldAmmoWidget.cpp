#include "WorldAmmoWidget.h"
#include "Components/TextBlock.h"

void UWorldAmmoWidget::SetAmmoText(int32 CurrentAmmo, int32 CarriedAmmo)
{
	if (AmmoText)
	{
		FString Text = FString::Printf(TEXT("%d / %d"), CurrentAmmo, CarriedAmmo);
		AmmoText->SetText(FText::FromString(Text));
	}
}