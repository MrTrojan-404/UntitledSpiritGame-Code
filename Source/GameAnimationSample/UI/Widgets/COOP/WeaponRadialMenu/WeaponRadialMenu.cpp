#include "WeaponRadialMenu.h"
#include "Components/TextBlock.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "Kismet/GameplayStatics.h"

void UWeaponRadialMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// Grab the current body directly from the Player Controller before doing anything else!
	if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(GetOwningPlayer()))
	{
		BaseCharacter = Cast<AGASPBaseCharacter>(PC->GetPawn());
	}

	// Now it is perfectly safe to build the UI!
	if (BaseCharacter && BaseCharacter->GetCombatComponent())
	{
		for (int32 i = 0; i < 6; i++)
		{
			TArray<AWeapon*> CategoryWeapons = BaseCharacter->GetCombatComponent()->GetWeaponsInCategory(SliceCategories[i]);
            
			if (CategoryWeapons.Num() > 0 && CategoryWeapons[0])
			{
				OnUpdateSliceIcon(i, CategoryWeapons[0]->WeaponIcon, false, CategoryWeapons[0]->WeaponUIColor); 
			}
			else
			{
				OnUpdateSliceIcon(i, EmptySlotIcon, false, FLinearColor::White); 
			}
		}
	}
}

void UWeaponRadialMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    int32 HoveredSlice = GetSelectedSegmentIndex(6, 60.f);

    if (HoveredSlice != -1 && HoveredSlice != CurrentSliceIndex)
    {
        CurrentSliceIndex = HoveredSlice;
        CurrentCategory = SliceCategories[CurrentSliceIndex];
        ScrollIndex = 0; 
        UpdateMenuUI(CurrentSliceIndex);
    }
}

FReply UWeaponRadialMenu::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!BaseCharacter || !BaseCharacter->GetCombatComponent() || CurrentSliceIndex == -1) return FReply::Unhandled();

    TArray<AWeapon*> CategoryWeapons = BaseCharacter->GetCombatComponent()->GetWeaponsInCategory(CurrentCategory);

    if (CategoryWeapons.Num() > 1)
    {
        float ScrollDelta = InMouseEvent.GetWheelDelta();
        if (ScrollDelta > 0) ScrollIndex++;
        else if (ScrollDelta < 0) ScrollIndex--;

        ScrollIndex = (ScrollIndex % CategoryWeapons.Num() + CategoryWeapons.Num()) % CategoryWeapons.Num();
        
        UpdateMenuUI(CurrentSliceIndex);
    }

    return FReply::Handled();
}

FReply UWeaponRadialMenu::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (BaseCharacter && BaseCharacter->GetCombatComponent() && HoveredWeapon)
    {
       if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
       {
          BaseCharacter->GetCombatComponent()->ServerEquipFromMenuToPrimary(HoveredWeapon);
            
          if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(GetOwningPlayer()))
          {
             PC->WeaponWheelReleased();
          }
          return FReply::Handled();
       }
        
       if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
       {
          BaseCharacter->GetCombatComponent()->ServerEquipFromMenuToSecondary(HoveredWeapon);
          bJustEquippedToSecondary = true; 
          return FReply::Handled();
       }
    }

    return FReply::Handled(); 
}

void UWeaponRadialMenu::UpdateMenuUI(int32 HoveredSlice)
{
    if (!BaseCharacter || !BaseCharacter->GetCombatComponent() || !CenterWeaponName) return;

    TArray<AWeapon*> CategoryWeapons = BaseCharacter->GetCombatComponent()->GetWeaponsInCategory(CurrentCategory);

    if (CategoryWeapons.Num() == 0)
    {
       HoveredWeapon = nullptr;
       CenterWeaponName->SetText(FText::FromString(TEXT("Unarmed")));
       if (CenterWeaponAmmo) CenterWeaponAmmo->SetText(FText::GetEmpty());

       OnUpdateSliceIcon(HoveredSlice, EmptySlotIcon, true, FLinearColor::White);
    }
    else if (CategoryWeapons.IsValidIndex(ScrollIndex))
    {
       HoveredWeapon = CategoryWeapons[ScrollIndex];
        
       CenterWeaponName->SetText(FText::FromString(HoveredWeapon->WeaponName));
        
       if (CenterWeaponAmmo)
       {
          FString AmmoText = FString::Printf(TEXT("%d / %d"), HoveredWeapon->GetAmmo(), BaseCharacter->GetCombatComponent()->CarriedAmmoMap.Contains(HoveredWeapon->GetWeaponType()) ? BaseCharacter->GetCombatComponent()->CarriedAmmoMap[HoveredWeapon->GetWeaponType()] : 0);
          CenterWeaponAmmo->SetText(FText::FromString(AmmoText));
       }

       OnUpdateSliceIcon(HoveredSlice, HoveredWeapon->WeaponIcon, true, HoveredWeapon->WeaponUIColor);
    }
}

int32 UWeaponRadialMenu::GetSelectedSegmentIndex(int32 NumberOfSegments, float DeadzoneRadius)
{
    if (NumberOfSegments <= 0) return -1;
    APlayerController* PC = GetOwningPlayer();    if (!PC) return -1;

    float MouseX, MouseY;
    if (!PC->GetMousePosition(MouseX, MouseY)) return -1;

    int32 ViewportSizeX, ViewportSizeY;
    PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
    FVector2D Center(ViewportSizeX / 2.0f, ViewportSizeY / 2.0f);
    FVector2D MousePos(MouseX, MouseY);

    if (FVector2D::Distance(Center, MousePos) < DeadzoneRadius) return -1;

    FVector2D Delta = MousePos - Center;
    float Angle = FMath::RadiansToDegrees(FMath::Atan2(Delta.X, -Delta.Y));

    if (Angle < 0.0f) Angle += 360.0f;

    float SegmentSize = 360.0f / NumberOfSegments;
    Angle += (SegmentSize / 2.0f);
    if (Angle >= 360.0f) Angle -= 360.0f;

    return FMath::FloorToInt(Angle / SegmentSize);
}