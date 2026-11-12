#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameAnimationSample/Actor/Weapon.h"
#include "WeaponRadialMenu.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UWeaponRadialMenu : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	int32 GetSelectedSegmentIndex(int32 NumberOfSegments, float DeadzoneRadius = 50.f);

	// Re-evaluates what is currently hovered and updates the UI
	void UpdateMenuUI(int32 HoveredSlice);

	UFUNCTION(BlueprintImplementableEvent, Category = "Radial Menu")
	void OnUpdateSliceIcon(int32 SliceIndex, class UTexture2D* NewIcon, bool bIsActiveSlice, FLinearColor SliceColor);
	
public:
	// The Center Text
	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	class UTextBlock* CenterWeaponName;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadWrite)
	UTextBlock* CenterWeaponAmmo;

	//The icon to show when a category has no weapons
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Radial Menu Options")
	UTexture2D* EmptySlotIcon;
	
	// State Variables
	UPROPERTY()
	AGASPBaseCharacter* BaseCharacter;

	UPROPERTY()
	AWeapon* HoveredWeapon = nullptr;

	// Safety flag so releasing the wheel doesn't overwrite our click!
	bool bJustEquippedToSecondary = false;

protected:
	// The built-in Unreal function for detecting clicks on a UI
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	
private:
	int32 CurrentSliceIndex = -1;
	EWeaponCategory CurrentCategory = EWeaponCategory::EWC_Sidearm;
	int32 ScrollIndex = 0;
    
	// Clockwise layout
	const TArray<EWeaponCategory> SliceCategories = {
		EWeaponCategory::EWC_Sidearm,   // 0: Top
		EWeaponCategory::EWC_Assault,   // 1: Top Right
		EWeaponCategory::EWC_Shotgun,   // 2: Bottom Right
		EWeaponCategory::EWC_Sniper,    // 3: Bottom
		EWeaponCategory::EWC_Heavy,     // 4: Bottom Left
		EWeaponCategory::EWC_Special    // 5: Top Left
	};
	
};