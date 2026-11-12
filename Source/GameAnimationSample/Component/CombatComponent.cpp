// Fill out your copyright notice in the Description page of Project Settings.


#include "CombatComponent.h"

#include "MoverComponent.h"
#include "GameAnimationSample/GameAnimationSample.h"
#include "GameAnimationSample/Actor/ThrowableDistraction.h"
#include "GameAnimationSample/Actor/Weapon.h"
#include "GameAnimationSample/Actor/Weapon/ChargeLaserWeapon.h"
#include "GameAnimationSample/Actor/Weapon/Shotgun.h"
#include "GameAnimationSample/Actor/Weapon/Projectile/Projectile.h"
#include "GameAnimationSample/AI/Character/GASPEnemyBase.h"
#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "GameAnimationSample/Interface/CombatInterface.h"
#include "GameAnimationSample/GameVariables/CombatState.h"
#include "GameAnimationSample/Interface/InteractWithCrosshairsInterface.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "GameAnimationSample/SpellAbility/SpellComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AISense_Hearing.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerActor = GetOwner();
	OwnerPawn = GetOwnerPawn();

	if (OwnerPawn && OwnerPawn->HasAuthority())
	{
		InitializeCarriedAmmo();
	}

	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		if (AGASPCorePlayerController* PC = GetCachedController())
		{
			PC->SetHUDGrenades(Grenades);
		}
	}
}


void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (PrimaryEquippedWeapon && PrimaryEquippedWeapon->bIsChargeWeapon)
	{
		if (bFireButtonPressed)
		{
			// Constantly refresh the timer so the AnimBP never thinks we stopped fighting
			LastWeaponActionTime = GetWorld()->GetTimeSeconds();
		}
	}
	// Make sure the Charge Weapon bypasses the automatic fire loop
	else if (bFireButtonPressed && bCanFire && PrimaryEquippedWeapon && PrimaryEquippedWeapon->bAutomatic)
	{
		Fire();
	}
	if (OwnerPawn && IsOwnerLocallyControlled())
	{
		if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(OwnerPawn))
		{
			if (UMoverComponent* MoverComp = BaseChar->FindComponentByClass<UMoverComponent>())
			{
				FVector Velocity = MoverComp->GetVelocity();
				Velocity.Z = 0.f;

				FVector2D WalkSpeedRange(0.f, SpreadMaxWalkSpeed);
				FVector2D VelocityMultiplierRange(0.f, MaxVelocitySpread);
				CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(
					WalkSpeedRange, VelocityMultiplierRange, Velocity.Size());

				FName CurrentMode = MoverComp->GetMovementModeName();

				if (CurrentMode == FName("Falling") || CurrentMode == FName("Airborne"))
				{
					CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, InAirSpreadPenalty, DeltaTime, 2.25f);
				}
				else
				{
					CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 0.f, DeltaTime, 30.f);
				}
				if (bIsCrouching)
				{
					CrosshairCrouchFactor = FMath::FInterpTo(CrosshairCrouchFactor, CrouchSpreadBonus, DeltaTime, 30.f);
				}
				else
				{
					CrosshairCrouchFactor = FMath::FInterpTo(CrosshairCrouchFactor, 0.f, DeltaTime, 30.f);
				}
			}
		}
	}
	CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, bAiming ? AimSpreadBonus : 0.f, DeltaTime, AimRecoverySpeed);
	CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.f, DeltaTime, SpreadRecoverySpeed);
	
	float RawMultiplier = BaseSpread + CrosshairVelocityFactor + CrosshairInAirFactor - CrosshairAimFactor - CrosshairCrouchFactor + CrosshairShootingFactor;

	float MinSpreadClamp = MinimumSpread; 
	if (PrimaryEquippedWeapon)
	{
		MinSpreadClamp = PrimaryEquippedWeapon->WeaponMinimumSpread;
	}
	CurrentSpreadMultiplier = FMath::Max(RawMultiplier, MinSpreadClamp);
	
	//  Only run screen traces if WE are the ones looking through the camera AND we are a human player
	if (IsOwnerLocallyControlled() && OwnerPawn->IsPlayerControlled())
	{
		FHitResult HitResult;
		TraceUnderCrosshairs(HitResult);
		HitTarget = HitResult.ImpactPoint;
		SetHUDCrosshairs(DeltaTime);

		// Sync the aim target to the Server constantly
		if (!OwnerActor->HasAuthority())
		{
			ServerSyncHitTarget(HitTarget);
		}
	}

	float RecovSpeed = PrimaryEquippedWeapon ? PrimaryEquippedWeapon->RecoilRecoverySpeed : 15.f;
	VisualRecoilRotation = FMath::RInterpTo(VisualRecoilRotation, FRotator::ZeroRotator, DeltaTime, RecovSpeed);
	
}


void UCombatComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCombatComponent, PrimaryEquippedWeapon);
	DOREPLIFETIME(UCombatComponent, SecondaryEquippedWeapon);
	DOREPLIFETIME_CONDITION(UCombatComponent, CarriedAmmo, COND_OwnerOnly);
	DOREPLIFETIME(UCombatComponent, CombatState);
	DOREPLIFETIME(UCombatComponent, Grenades);
	DOREPLIFETIME(UCombatComponent, bAiming);
	DOREPLIFETIME(UCombatComponent, WeaponInventory);
	DOREPLIFETIME_CONDITION(UCombatComponent, HitTarget, COND_SkipOwner);
}

#pragma region RepNotify

void UCombatComponent::OnRep_CombatState()
{
	switch (CombatState)
	{
	case ECombatState::ECS_Reloading:
		if (OwnerPawn && !OwnerPawn->IsLocallyControlled()) HandleReload();
		break;
	case ECombatState::ECS_Unoccupied:
		if (bFireButtonPressed)
		{
			// Respect Charge Weapons and Non-Automatics
			if (PrimaryEquippedWeapon && PrimaryEquippedWeapon->bIsChargeWeapon)
			{
				// Restart the charge sequence properly
				HandleFire(true);
			}
			else if (PrimaryEquippedWeapon && PrimaryEquippedWeapon->bAutomatic)
			{
				Fire();
			}
		}
		break;
	case ECombatState::ECS_ThrowingGrenade:
		if (OwnerPawn && !OwnerPawn->IsLocallyControlled())
		{
			ICombatInterface::Execute_PlayThrowGrenadeMontage(OwnerActor);
			ShowAttachedGrenade(true);
		}
		break;
	case ECombatState::ECS_SwappingWeapons:
		if (OwnerPawn && !OwnerPawn->IsLocallyControlled())
		{
			bFinishedSwapping = false;
			ICombatInterface::Execute_PlayEquippedSwapMontage(OwnerActor);
		}
		break;
	}
}

void UCombatComponent::OnRep_Grenades()
{
	UpdateHUDGrenades();
}

void UCombatComponent::OnRep_CarriedAmmo()
{
	if (AGASPCorePlayerController* PC = GetCachedController())
	{
        PC->SetHUDCarriedAmmo(CarriedAmmo);
	}
	bool bJumpToShotgunEnd = 
		CombatState == ECombatState::ECS_Reloading &&
		GetPrimaryWeapon() != nullptr &&
		GetPrimaryWeapon()->bUsesShellByShellReload &&
		CarriedAmmo == 0;
	
	if (bJumpToShotgunEnd)
	{
		JumpToShotgunEnd();
	}
}

#pragma endregion RepNotify

void UCombatComponent::ThrowDistraction()
{
	if (!DistractionClass || !GetOwner()) return;

	APawn* ThrowingPawn = Cast<APawn>(GetOwner());
	if (!ThrowingPawn) return;

	FVector SpawnLocation = ThrowingPawn->GetActorLocation();
	FRotator SpawnRotation = ThrowingPawn->GetActorRotation();

	// Use the camera's looking direction!
	if (APlayerController* PC = Cast<APlayerController>(ThrowingPawn->GetController()))
	{
		SpawnRotation = PC->GetControlRotation();
		// Spawn it a little bit forward and up so it doesn't clip the player's chest
		SpawnLocation = ThrowingPawn->GetActorLocation() + FVector(0.f, 0.f, 50.f) + (SpawnRotation.Vector() * 100.f);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = ThrowingPawn;

	GetWorld()->SpawnActor<AThrowableDistraction>(DistractionClass, SpawnLocation, SpawnRotation, SpawnParams);
}

void UCombatComponent::ServerSyncHitTarget_Implementation(FVector_NetQuantize100 Target)
{
	HitTarget = Target;
}

void UCombatComponent::TraceUnderCrosshairs(FHitResult& TraceHitResults)
{
	FVector2D ViewportSize(FVector2D::ZeroVector);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	FVector2D CrosshairLocation(ViewportSize.X/2.f, ViewportSize.Y/2.f);
	FVector CrosshairWorldPosition;
	FVector CrosshairWorldDirection;

	// Use the exact controller driving this specific body.
	AGASPCorePlayerController* PC = GetCachedController();
	if (!PC) return;

	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
		PC, // <-- Replaced GetPlayerController(this, 0)
		CrosshairLocation,
		CrosshairWorldPosition,
		CrosshairWorldDirection
	);

	if (bScreenToWorld)
	{
		FVector Start = CrosshairWorldPosition;

		if (OwnerPawn)
		{
			float DistanceToCharacter = (OwnerPawn->GetActorLocation() - Start).Size();
			Start += CrosshairWorldDirection * (DistanceToCharacter + 100.f);
		}
		FVector End = Start + CrosshairWorldDirection * TRACE_LENGTH;

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(OwnerPawn); 

		// ==========================================
		// Ignore holstered weapons and grenades!
		// If the camera trace hits a gun on your back, it forces 
		// the muzzle to shoot backwards into your own chest.
		// ==========================================
		if (OwnerPawn)
		{
			TArray<AActor*> AttachedActors;
			OwnerPawn->GetAttachedActors(AttachedActors);
			QueryParams.AddIgnoredActors(AttachedActors);
		}

		if (PrimaryEquippedWeapon)
		{
			QueryParams.AddIgnoredActor(PrimaryEquippedWeapon);
		}
	
		GetWorld()->LineTraceSingleByChannel(
		  TraceHitResults,
		  Start,
		  End,
		  ECC_Visibility,
		  QueryParams 
	   );

		if (TraceHitResults.GetActor() && TraceHitResults.GetActor()->Implements<UInteractWithCrosshairsInterface>())
		{
			HUDPackage.CrosshairsColor = FLinearColor::Red;
			IInteractWithCrosshairsInterface* HitActor =
				Cast<IInteractWithCrosshairsInterface>(TraceHitResults.GetActor());
			if (HitActor) HitActor->OnCrosshairFocused();
			LastFocusedActor = Cast<AActor>(HitActor); 
		}
		else
		{
			HUDPackage.CrosshairsColor = FLinearColor::White;
			if (LastFocusedActor)
			{
				IInteractWithCrosshairsInterface* Last =
					Cast<IInteractWithCrosshairsInterface>(LastFocusedActor);
				if (Last) Last->OnCrosshairUnfocused();
				LastFocusedActor = nullptr;
			}
		}
		
		if (!TraceHitResults.bBlockingHit)
		{
			TraceHitResults.ImpactPoint = End;
		}
		
	}
}

void UCombatComponent::Melee()
{
    if (CombatState != ECombatState::ECS_Unoccupied) return;

    if (OwnerPawn && OwnerPawn->IsLocallyControlled())
    {
        ICombatInterface::Execute_PlayMeleeMontage(OwnerActor);
    }

    if (!OwnerActor->HasAuthority())
    {
        ServerMelee();
    }
    else
    {
        CombatState = ECombatState::ECS_Melee;
        MulticastMelee();
    }
}

void UCombatComponent::ServerMelee_Implementation()
{
    if (CombatState != ECombatState::ECS_Unoccupied) return;
    CombatState = ECombatState::ECS_Melee;
    MulticastMelee();
}

void UCombatComponent::MulticastMelee_Implementation()
{
    if (OwnerPawn && OwnerPawn->IsLocallyControlled() && !OwnerActor->HasAuthority()) return;

    ICombatInterface::Execute_PlayMeleeMontage(OwnerActor);
}

void UCombatComponent::MeleeHit()
{
	if (!OwnerActor->HasAuthority() || !OwnerPawn) return;

	FVector Start = OwnerPawn->GetPawnViewLocation();
	FVector Forward = OwnerPawn->GetViewRotation().Vector();
	FVector End = Start + (Forward * MeleeRange);

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwnerPawn);
	if (PrimaryEquippedWeapon) Params.AddIgnoredActor(PrimaryEquippedWeapon);

	bool bHit = GetWorld()->SweepSingleByChannel(
		HitResult, 
		Start, 
		End, 
		FQuat::Identity,
		ECC_Pawn, 
		FCollisionShape::MakeSphere(MeleeRadius),
		Params
	);
   
	DrawDebugLine(GetWorld(), Start, End, FColor::Blue, false, 2.f, 0, 2.f);
	DrawDebugSphere(GetWorld(), End, MeleeRadius, 12, FColor::Red, false, 2.f);

	if (bHit && HitResult.GetActor())
	{
		DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 15.f, 12, FColor::Green, false, 2.f);

		UGameplayStatics::ApplyDamage(
			HitResult.GetActor(),
			MeleeDamage,
			OwnerPawn->GetController(),
			OwnerActor,
			UDamageType::StaticClass()
		);

		if (AGASPBaseCharacter* AttackerChar = Cast<AGASPBaseCharacter>(OwnerActor))
		{
			if (USpellComponent* SpellComp = AttackerChar->FindComponentByClass<USpellComponent>())
			{
				SpellComp->OnDamageDealt(MeleeDamage);
			}
		}
	}
}

#pragma region UI

void UCombatComponent::SetHUDCrosshairs(float DeltaTime)
{
	if (!OwnerPawn) return;

	AGASPCorePlayerController* PC = GetCachedController();
	if (!PC) return;

	if (!HUD) HUD = Cast<AGASPHUD>(PC->GetHUD());
	if (!HUD) return;
	
	if (bShowCrosshairs && GetPrimaryWeapon())
	{
		HUDPackage.CrosshairsCenter = GetPrimaryWeapon()->CrosshairsCenter;
		HUDPackage.CrosshairsLeft   = GetPrimaryWeapon()->CrosshairsLeft;
		HUDPackage.CrosshairsRight  = GetPrimaryWeapon()->CrosshairsRight;
		HUDPackage.CrosshairsBottom = GetPrimaryWeapon()->CrosshairsBottom;
		HUDPackage.CrosshairsTop    = GetPrimaryWeapon()->CrosshairsTop;
	}
	else
	{
		HUDPackage.CrosshairsCenter = nullptr;
		HUDPackage.CrosshairsLeft   = nullptr;
		HUDPackage.CrosshairsRight  = nullptr;
		HUDPackage.CrosshairsBottom = nullptr;
		HUDPackage.CrosshairsTop    = nullptr;
	}

	HUDPackage.CrosshairSpread = CurrentSpreadMultiplier;
	HUD->SetHUDPackage(HUDPackage);
}

void UCombatComponent::UpdateHUDGrenades()
{
	if (AGASPCorePlayerController* PC = GetCachedController())
	{
        PC->SetHUDGrenades(Grenades);
	}
}

#pragma endregion UI

#pragma region Aiming

void UCombatComponent::SetAiming(bool bIsAiming)
{
	if (OwnerPawn == nullptr || GetPrimaryWeapon() == nullptr) return;

	if (bAiming == bIsAiming)
	{
		if (OwnerPawn->IsLocallyControlled()) bAimButtonPressed = bIsAiming;
		return;
	}

	if (bIsAiming && CombatState != ECombatState::ECS_Unoccupied)
	{
		if (OwnerPawn->IsLocallyControlled()) bAimButtonPressed = true;
		return;
	}

	bAiming = bIsAiming;
	ServerSetAiming(bIsAiming);

	if (OwnerPawn->IsLocallyControlled() && GetPrimaryWeapon()->GetWeaponType() == EWeaponType::EWT_SniperRifle)
	{
		ICombatInterface::Execute_ShowSniperScopeCall(OwnerActor, bIsAiming);
	}
   
	if (OwnerPawn->IsLocallyControlled()) bAimButtonPressed = bIsAiming;
}

void UCombatComponent::ServerSetAiming_Implementation(bool bIsAiming)
{
	bAiming = bIsAiming;
}

void UCombatComponent::OnRep_Aiming()
{
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		bAiming = bAimButtonPressed;
	}
}

#pragma endregion Aiming

#pragma region Fire

void UCombatComponent::HandleFire(bool bPressed)
{
	bFireButtonPressed = bPressed;

	if (bFireButtonPressed)
	{
		LastWeaponActionTime = GetWorld()->GetTimeSeconds(); 

		if (PrimaryEquippedWeapon && PrimaryEquippedWeapon->bIsChargeWeapon)
		{
			ChargeStartTime = GetWorld()->GetTimeSeconds();
			bIsCharging = true; // Mark as actively charging

			if (AChargeLaserWeapon* ChargeWeapon = Cast<AChargeLaserWeapon>(PrimaryEquippedWeapon))
			{
				ChargeWeapon->StartCharging();
			}
		}
		else
		{
			Fire();
		}
	}
	else
	{
		// Trigger Released
		if (PrimaryEquippedWeapon && PrimaryEquippedWeapon->bIsChargeWeapon && bIsCharging)
		{
			bIsCharging = false; // Clear the flag
			float ChargeDuration = GetWorld()->GetTimeSeconds() - ChargeStartTime;
			
			// Ensure the character is still being driven by a Human Player 
			// and is NOT in the middle of dissolving for a Tag Team switch! ===
			bool bIsActivePlayer = OwnerPawn && OwnerPawn->IsPlayerControlled();
			if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(OwnerPawn))
			{
				if (BaseChar->bIsTagTeamDissolving) bIsActivePlayer = false;
			}

			if (AChargeLaserWeapon* ChargeWeapon = Cast<AChargeLaserWeapon>(PrimaryEquippedWeapon))
			{
				// Must meet charge time, be an active player, AND be allowed to fire
				if (ChargeDuration >= ChargeWeapon->RequiredChargeTime && bIsActivePlayer && CanFire())
				{
					Fire();
				}
				else
				{
					// FIZZLE: Released too early, swapped characters, or blocked from firing!
					ChargeWeapon->CancelCharging();
				}
			}
		}
	}
}

void UCombatComponent::Fire()
{
	if (CombatState == ECombatState::ECS_Reloading &&
		PrimaryEquippedWeapon &&
		PrimaryEquippedWeapon->bUsesShellByShellReload &&
		!PrimaryEquippedWeapon->IsEmpty())
	{
		JumpToShotgunEnd(); 
		if (!OwnerActor->HasAuthority())
			ServerInterruptShotgunReload();
		return; 
	}
	
	if (CanFire())
	{
		bCanFire = false;
   
		if (PrimaryEquippedWeapon)
		{
			CrosshairShootingFactor = FMath::Min(CrosshairShootingFactor + ShootingSpreadPenalty, MaxShootingSpread);

			FVector OriginalStableTarget = HitTarget; 

			if (OwnerPawn && !OwnerPawn->IsPlayerControlled())
			{
				if (AGASPEnemyBase* Enemy = Cast<AGASPEnemyBase>(OwnerPawn))
				{
					HitTarget.X += FMath::RandRange(-Enemy->AimErrorRadius, Enemy->AimErrorRadius);
					HitTarget.Y += FMath::RandRange(-Enemy->AimErrorRadius, Enemy->AimErrorRadius);
					HitTarget.Z += FMath::RandRange(-Enemy->AimErrorRadius, Enemy->AimErrorRadius);
				}
			}

			switch (PrimaryEquippedWeapon->FireType)
			{
			case EFireType::EFT_Projectile:
				FireProjectileWeapon();
				break;
			case EFireType::EFT_HitScan:
				FireHitScanWeapon();
				break;
			case EFireType::EFT_Shotgun:
				FireShotgun();
				break;
			}

			HitTarget = OriginalStableTarget;
		}
		StartFireTimer();
	}
}

void UCombatComponent::FireProjectileWeapon()
{
	if (PrimaryEquippedWeapon && OwnerActor)
	{
		bool bUseFSA = bAiming && 
					   (CrosshairVelocityFactor < 0.05f) && 
					   (CrosshairInAirFactor < 0.05f) && 
					   (CrosshairShootingFactor <= (ShootingSpreadPenalty + 0.02f));

		float FinalSpread = bUseFSA ? 0.0f : CurrentSpreadMultiplier;
		
		HitTarget = PrimaryEquippedWeapon->bUseScatter ? PrimaryEquippedWeapon->TraceEndWithScatter(HitTarget, FinalSpread) : HitTarget;
		
		if (!OwnerActor->HasAuthority()) LocalFire(HitTarget);
		ServerFire(HitTarget, PrimaryEquippedWeapon->FireDelay);
	}
}

void UCombatComponent::FireHitScanWeapon() 
{
	if (PrimaryEquippedWeapon && OwnerActor)
	{
		bool bUseFSA = bAiming && 
					   (CrosshairVelocityFactor < 0.05f) && 
					   (CrosshairInAirFactor < 0.05f) && 
					   (CrosshairShootingFactor <= (ShootingSpreadPenalty + 0.02f));

		float FinalSpread = bUseFSA ? 0.0f : CurrentSpreadMultiplier;

		HitTarget = PrimaryEquippedWeapon->bUseScatter ? PrimaryEquippedWeapon->TraceEndWithScatter(HitTarget, FinalSpread) : HitTarget;
       
		if (!OwnerActor->HasAuthority()) LocalFire(HitTarget);
		ServerFire(HitTarget, PrimaryEquippedWeapon->FireDelay);
	}
}

void UCombatComponent::FireShotgun()
{
	AShotgun* Shotgun = Cast<AShotgun>(PrimaryEquippedWeapon);
	if (Shotgun && OwnerActor)
	{
		TArray<FVector_NetQuantize100> HitTargets;
		Shotgun->ShotgunTraceEndWithScatter(HitTarget, HitTargets,CurrentSpreadMultiplier);
		if (!OwnerActor->HasAuthority()) ShotgunLocalFire(HitTargets);
		ServerShotgunFire(HitTargets, PrimaryEquippedWeapon->FireDelay);
	}
}

void UCombatComponent::ServerShotgunFire_Implementation(const TArray<FVector_NetQuantize100>& TraceHitTargets, float FireDelay)
{
	if (GetPrimaryWeapon())
	{
		if (bool bNearlyEqual = FMath::IsNearlyEqual(GetPrimaryWeapon()->FireDelay, FireDelay, 0.001f))
		{
			MulticastShotgunFire(TraceHitTargets);
		}		
	}
}
void UCombatComponent::MulticastShotgunFire_Implementation(const TArray<FVector_NetQuantize100>& TraceHitTargets)
{
	if (OwnerPawn && OwnerPawn->IsLocallyControlled() && !OwnerPawn->HasAuthority()) return;
	ShotgunLocalFire(TraceHitTargets);
}


void UCombatComponent::LocalFire(const FVector_NetQuantize& TraceHitTarget)
{
	if (PrimaryEquippedWeapon == nullptr) return;
	if (OwnerActor && CombatState == ECombatState::ECS_Unoccupied)
	{
		LastWeaponActionTime = GetWorld()->GetTimeSeconds();

		// Montage always fires immediately — starts the raise animation
		ICombatInterface::Execute_PlayFireMontage(OwnerActor, bAiming);

		// Only delay for the locally controlled pawn firing from an idle pose
		bool bCharCombatReady = true;
		if (OwnerPawn && OwnerPawn->IsLocallyControlled())
		{
			if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(OwnerPawn))
			{
				bCharCombatReady = BaseChar->GetIsCombatReady();
			}
		}

		// Remove the IsLocallyControlled() check from this line! 
		// This forces the Server and all remote clients to wait for the gun to physically raise 
		// before tracing the bullet path, stopping them from shooting into the floor!
		const bool bShouldDelay = !bAiming && !bCharCombatReady;

		if (bShouldDelay)
		{
			DelayedHitTarget = TraceHitTarget;

			float CurrentDilation = FMath::Max(OwnerActor->CustomTimeDilation, KINDA_SMALL_NUMBER);
			float DilatedDelay = HipFireRaiseDelay / CurrentDilation;

			OwnerActor->GetWorldTimerManager().SetTimer(
				HipFireDelayTimer,
				this,
				&UCombatComponent::ExecuteDelayedShot,
				DilatedDelay,
				false
			);
		}
		else
		{
			ExecuteShot(TraceHitTarget);
		}
	}
}

void UCombatComponent::ResetFireTimer()
{
	if (OwnerActor)
	{
		OwnerActor->GetWorldTimerManager().ClearTimer(FireTimer);
		OwnerActor->GetWorldTimerManager().ClearTimer(HipFireDelayTimer);
	}
	bCanFire = true;
}

void UCombatComponent::ExecuteShot(const FVector_NetQuantize& TraceHitTarget)
{
	if (!PrimaryEquippedWeapon || !OwnerActor) return;

	PrimaryEquippedWeapon->Fire(TraceHitTarget);

	if (OwnerActor->HasAuthority())
	{
		float NoiseRadius = PrimaryEquippedWeapon->IsSuppressed() ? 600.f : 5000.f;
		UAISense_Hearing::ReportNoiseEvent(
			OwnerActor->GetWorld(),
			OwnerPawn->GetActorLocation(),
			1.f, OwnerPawn,
			NoiseRadius,
			FName("Gunshot")
		);
	}

	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		if (PrimaryEquippedWeapon->FireCameraShakeClass)
		{
			if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
			{
				PC->ClientStartCameraShake(PrimaryEquippedWeapon->FireCameraShakeClass, 1.f);
			}
		}

		float RecoilPitch = FMath::RandRange(
			PrimaryEquippedWeapon->VisualRecoilPitchMin,
			PrimaryEquippedWeapon->VisualRecoilPitchMax);
		float RecoilYaw = FMath::RandRange(
			PrimaryEquippedWeapon->VisualRecoilYawMin,
			PrimaryEquippedWeapon->VisualRecoilYawMax);
		VisualRecoilRotation += FRotator(RecoilPitch, RecoilYaw, 0.f);
	}
}

void UCombatComponent::ExecuteDelayedShot()
{
	ExecuteShot(DelayedHitTarget);
}

void UCombatComponent::ShotgunLocalFire(const TArray<FVector_NetQuantize100>& TraceHitTargets)
{
	AShotgun* Shotgun = Cast<AShotgun>(PrimaryEquippedWeapon);
	if (Shotgun == nullptr || OwnerActor == nullptr) return;

	if (CombatState == ECombatState::ECS_Unoccupied)
	{
		LastWeaponActionTime = GetWorld()->GetTimeSeconds();
		bLocallyReloading = false;
		ICombatInterface::Execute_PlayFireMontage(OwnerActor, bAiming);
		CombatState = ECombatState::ECS_Unoccupied;

		bool bCharCombatReady = true;
		if (OwnerPawn && OwnerPawn->IsLocallyControlled())
		{
			if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(OwnerPawn))
			{
				bCharCombatReady = BaseChar->GetIsCombatReady();
			}
		}

		const bool bShouldDelay = !bAiming && !bCharCombatReady
			&& OwnerPawn && OwnerPawn->IsLocallyControlled();

		if (bShouldDelay)
		{
			DelayedShotgunTargets = TraceHitTargets;

			float CurrentDilation = FMath::Max(OwnerActor->CustomTimeDilation, KINDA_SMALL_NUMBER);
			float DilatedDelay = HipFireRaiseDelay / CurrentDilation;

			OwnerActor->GetWorldTimerManager().SetTimer(
				HipFireDelayTimer,
				this,
				&UCombatComponent::ExecuteDelayedShotgunShot,
				DilatedDelay,
				false
			);
		}
		else
		{
			ExecuteShotgunShot(TraceHitTargets);
		}
	}
}

void UCombatComponent::ExecuteShotgunShot(const TArray<FVector_NetQuantize100>& TraceHitTargets)
{
	AShotgun* Shotgun = Cast<AShotgun>(PrimaryEquippedWeapon);
	if (!Shotgun || !OwnerActor) return;

	Shotgun->FireShotgun(TraceHitTargets);

	if (OwnerActor->HasAuthority())
	{
		float NoiseRadius = Shotgun->IsSuppressed() ? 600.f : 5000.f;
		UAISense_Hearing::ReportNoiseEvent(
			OwnerActor->GetWorld(),
			OwnerPawn->GetActorLocation(),
			1.f, OwnerPawn,
			NoiseRadius,
			FName("Gunshot")
		);
	}

	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		if (PrimaryEquippedWeapon->FireCameraShakeClass)
		{
			if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
			{
				PC->ClientStartCameraShake(PrimaryEquippedWeapon->FireCameraShakeClass, 1.f);
			}
		}

		float RecoilPitch = FMath::RandRange(
			PrimaryEquippedWeapon->VisualRecoilPitchMin,
			PrimaryEquippedWeapon->VisualRecoilPitchMax);
		float RecoilYaw = FMath::RandRange(
			PrimaryEquippedWeapon->VisualRecoilYawMin,
			PrimaryEquippedWeapon->VisualRecoilYawMax);
		VisualRecoilRotation += FRotator(RecoilPitch, RecoilYaw, 0.f);
	}
}

void UCombatComponent::ExecuteDelayedShotgunShot()
{
	ExecuteShotgunShot(DelayedShotgunTargets);
}

void UCombatComponent::JumpToShotgunEnd()
{
	UAnimInstance* AnimInstance = ICombatInterface::Execute_GetCombatMesh(OwnerActor)->GetAnimInstance();
	if (ICombatInterface::Execute_GetReloadMontage(OwnerActor))
	{
		AnimInstance->Montage_JumpToSection(FName("ShotgunEnd"));
	}
}


void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	if (OwnerActor && IsOwnerLocallyControlled() && !OwnerActor->HasAuthority()) return;
	LocalFire(TraceHitTarget);
}

void UCombatComponent::ServerFire_Implementation(const FVector_NetQuantize100& TraceHitTarget, float FireDelay)
{
	MulticastFire(TraceHitTarget);
}


bool UCombatComponent::CanFire()
{
	if (PrimaryEquippedWeapon == nullptr) return false;

	if (OwnerPawn)
	{
		const float CamYaw  = OwnerPawn->GetBaseAimRotation().Yaw;
		const float CharYaw = OwnerPawn->GetActorRotation().Yaw;
		const float Delta   = FMath::Abs(FRotator::NormalizeAxis(CamYaw - CharYaw));
		if (Delta > AimAlignThreshold) return false;
	}

	if (bIsTraversing) return false; 

	if (bLocallyReloading) return false;

	return !PrimaryEquippedWeapon->IsEmpty()
		&& bCanFire
		&& CombatState == ECombatState::ECS_Unoccupied;
}

void UCombatComponent::StartFireTimer()
{
	if (PrimaryEquippedWeapon == nullptr || OwnerActor == nullptr) return;

	// Manually scale the delay based on the Owner's time dilation
	float CurrentDilation = FMath::Max(OwnerActor->CustomTimeDilation, KINDA_SMALL_NUMBER);
	float DilatedFireDelay = PrimaryEquippedWeapon->FireDelay / CurrentDilation;

	OwnerActor->GetWorldTimerManager().SetTimer(
		FireTimer,
		this,
		&UCombatComponent::FireTimerFinished,
		DilatedFireDelay
	);
}

void UCombatComponent::FireTimerFinished()
{
	if (PrimaryEquippedWeapon == nullptr) return;
	bCanFire = true;
	if (bFireButtonPressed && PrimaryEquippedWeapon->bAutomatic)
	{
		Fire();
	}
	ReloadEmptyWeapon();
}

#pragma endregion Fire

#pragma region Grenade

void UCombatComponent::OnThrowGrenadeFinished()
{
	if (IsOwnerLocallyControlled() && bAimButtonPressed) SetAiming(true);
	CombatState = ECombatState::ECS_Unoccupied;
}

void UCombatComponent::OnLaunchGrenade()
{
	ShowAttachedGrenade(false);
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		ServerLaunchGrenade(HitTarget);
	}
}


void UCombatComponent::ServerLaunchGrenade_Implementation(const FVector_NetQuantize& Target)
{
	if (OwnerPawn && GrenadeClass && ICombatInterface::Execute_GetAttachedGrenade(OwnerActor))
	{
		const FVector StartingLocation = ICombatInterface::Execute_GetAttachedGrenade(OwnerActor)->GetComponentLocation();
		FVector ToTarget = Target - StartingLocation;
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = OwnerPawn;
		SpawnParams.Instigator = OwnerPawn;
		UWorld* World = GetWorld();
		if (World)
		{
			World->SpawnActor<AProjectile>(
				GrenadeClass,
				StartingLocation,
				ToTarget.Rotation(),
				SpawnParams
				);
		}
	}
}

void UCombatComponent::ThrowGrenade()
{
	if (Grenades == 0) return;
	
	if (CombatState != ECombatState::ECS_Unoccupied) return; 

	bool bWasHoldingAim = bAimButtonPressed;
	SetAiming(false);
	if (IsOwnerLocallyControlled()) bAimButtonPressed = bWasHoldingAim;
	
	CombatState = ECombatState::ECS_ThrowingGrenade;

	if (OwnerPawn)
	{
		ICombatInterface::Execute_PlayThrowGrenadeMontage(OwnerActor);
		ShowAttachedGrenade(true);
	}
	if (OwnerPawn && !OwnerPawn->HasAuthority())
	{
		ServerThrowGrenade();
	}
	if (OwnerPawn && OwnerPawn->HasAuthority())
	{
		Grenades = FMath::Clamp(Grenades - 1, 0, MaxGrenades);
		UpdateHUDGrenades();
	}
}
void UCombatComponent::ServerThrowGrenade_Implementation()
{
	if (Grenades == 0) return;
	CombatState = ECombatState::ECS_ThrowingGrenade;
	if (OwnerPawn)
	{
		ICombatInterface::Execute_PlayThrowGrenadeMontage(OwnerActor);
		ShowAttachedGrenade(true);
	}
	Grenades = FMath::Clamp(Grenades - 1, 0, MaxGrenades);
	UpdateHUDGrenades();
}

void UCombatComponent::ShowAttachedGrenade(bool bShowGrenade)
{
	if (OwnerPawn && ICombatInterface::Execute_GetAttachedGrenade(OwnerActor))
	{
		ICombatInterface::Execute_GetAttachedGrenade(OwnerActor)->SetVisibility(bShowGrenade);
	}
}


#pragma endregion Grenade

#pragma region Ammo

void UCombatComponent::UpdateAmmoValues()
{
	if (OwnerPawn == nullptr || GetPrimaryWeapon() == nullptr) return;
	int32 ReloadAmount = AmountToReload();
	if (CarriedAmmoMap.Contains(GetPrimaryWeapon()->GetWeaponType()))
	{
		CarriedAmmoMap[GetPrimaryWeapon()->GetWeaponType()] -= ReloadAmount;
		CarriedAmmo = CarriedAmmoMap[GetPrimaryWeapon()->GetWeaponType()];
	}
	if (AGASPCorePlayerController* PC = GetCachedController())
	{
		PC->SetHUDCarriedAmmo(CarriedAmmo);
	}
	GetPrimaryWeapon()->AddAmmo(ReloadAmount);
}

void UCombatComponent::UpdateShotgunAmmoValues()
{
	if (OwnerPawn == nullptr || GetPrimaryWeapon() == nullptr) return;

	if (CarriedAmmoMap.Contains(GetPrimaryWeapon()->GetWeaponType()))
	{
		CarriedAmmoMap[GetPrimaryWeapon()->GetWeaponType()] -= 1;
		CarriedAmmo = CarriedAmmoMap[GetPrimaryWeapon()->GetWeaponType()];
	}
	if (AGASPCorePlayerController* PC = GetCachedController())
	{
		PC->SetHUDCarriedAmmo(CarriedAmmo);
	}
	GetPrimaryWeapon()->AddAmmo(1);
	bCanFire = true;
	if (GetPrimaryWeapon()->IsFull() || CarriedAmmo == 0)
	{
		JumpToShotgunEnd();
	}
}

void UCombatComponent::PickupAmmo(EWeaponType WeaponType, int32 AmmoAmount)
{
	if (CarriedAmmoMap.Contains(WeaponType))
	{
		CarriedAmmoMap[WeaponType] = FMath::Clamp(CarriedAmmoMap[WeaponType] + AmmoAmount, 0, MaxCarriedAmmo);
		UpdateCarriedAmmo();
	}
	if (GetPrimaryWeapon() && GetPrimaryWeapon()->IsEmpty() && GetPrimaryWeapon()->GetWeaponType() == WeaponType)
	{
		Reload();
	}
}

void UCombatComponent::UpdateCarriedAmmo()
{
	if (GetPrimaryWeapon() == nullptr) return;
	if (CarriedAmmoMap.Contains(GetPrimaryWeapon()->GetWeaponType()))
	{
		CarriedAmmo = CarriedAmmoMap[GetPrimaryWeapon()->GetWeaponType()];
	}

	if (AGASPCorePlayerController* PC = GetCachedController())
	{
		PC->SetHUDCarriedAmmo(CarriedAmmo);
	}
}

void UCombatComponent::InitializeCarriedAmmo()
{
	CarriedAmmoMap.Emplace(EWeaponType::EWT_AssaultRifle, StartingARAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_RocketLauncher, StartingRocketAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Pistol, StartingPistolAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SubmachineGun, StartingSMGAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Shotgun, StartingShotgunAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SniperRifle, StartingSniperAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_GrenadeLauncher, StartingGrenadeLauncherAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Special, 500);

	Grenades = StartingGrenades;
	
}
#pragma endregion Ammo

#pragma region Reload

void UCombatComponent::ReloadEmptyWeapon()
{
	if (PrimaryEquippedWeapon && PrimaryEquippedWeapon->IsEmpty())
	{
		Reload();
	}
}

void UCombatComponent::Reload()
{
	if (CarriedAmmo > 0 && CombatState == ECombatState::ECS_Unoccupied
	   && PrimaryEquippedWeapon && !PrimaryEquippedWeapon->IsFull() && !bLocallyReloading)
	{
		bool bWasHoldingAim = bAimButtonPressed;
       
		SetAiming(false);
       
		if (IsOwnerLocallyControlled())
		{
			bAimButtonPressed = bWasHoldingAim; 
		}

		ServerReload();
		HandleReload();
		bLocallyReloading = true;
	}
}

void UCombatComponent::ServerReload_Implementation()
{
    if (PrimaryEquippedWeapon == nullptr) return;

	WeaponBeingReloaded = PrimaryEquippedWeapon; // record before state changes
    CombatState = ECombatState::ECS_Reloading;
    
    if (!IsOwnerLocallyControlled()) 
    {
        HandleReload();
    }
}

void UCombatComponent::OnFinishReload()
{
	if (OwnerPawn == nullptr) return;

	bLocallyReloading = false;

	if (OwnerPawn->HasAuthority())
	{
		CombatState = ECombatState::ECS_Unoccupied;

		const bool bCorrectWeapon = WeaponBeingReloaded && WeaponBeingReloaded == GetPrimaryWeapon();
		WeaponBeingReloaded = nullptr;

		if (bCorrectWeapon && !GetPrimaryWeapon()->bUsesShellByShellReload)
		{
			UpdateAmmoValues();
		}
	}
	else
	{
		ServerFinishReload();
	}

	if (bFireButtonPressed)
	{
		// Respect Charge Weapons and Non-Automatics
		if (PrimaryEquippedWeapon && PrimaryEquippedWeapon->bIsChargeWeapon)
		{
			HandleFire(true);
		}
		else if (PrimaryEquippedWeapon && PrimaryEquippedWeapon->bAutomatic)
		{
			Fire();
		}
	}

	if (IsOwnerLocallyControlled() && bAimButtonPressed)
	{
		SetAiming(true);
	}
}

void UCombatComponent::ServerFinishReload_Implementation()
{
	OnFinishReload();
}

void UCombatComponent::OnShotgunShellReload()
{
	if (OwnerPawn && OwnerPawn->HasAuthority())
	{
		UpdateShotgunAmmoValues();
	}	
}


void UCombatComponent::HandleReload()
{
	ICombatInterface::Execute_PlayReloadMontage(OwnerActor);
}

int32 UCombatComponent::AmountToReload()
{
	if (PrimaryEquippedWeapon == nullptr) return 0;
	int32 RoomInMag = PrimaryEquippedWeapon->GetMagCapacity() - PrimaryEquippedWeapon->GetAmmo();

	if (CarriedAmmoMap.Contains(PrimaryEquippedWeapon->GetWeaponType()))
	{
		int32 AmountCarried = CarriedAmmoMap[PrimaryEquippedWeapon->GetWeaponType()];
		int32 Least = FMath::Min(RoomInMag, AmountCarried);
		return FMath::Clamp(RoomInMag, 0, Least);
	}
	return 0;
}

void UCombatComponent::ServerInterruptShotgunReload_Implementation()
{
	if (CombatState != ECombatState::ECS_Reloading) return;
	if (!PrimaryEquippedWeapon || PrimaryEquippedWeapon->IsEmpty()) return;

	if (!PrimaryEquippedWeapon->bUsesShellByShellReload) return;

	MulticastInterruptShotgunReload();
}

void UCombatComponent::MulticastInterruptShotgunReload_Implementation()
{
	if (OwnerPawn && OwnerPawn->IsLocallyControlled() && !OwnerActor->HasAuthority()) return;
	JumpToShotgunEnd();
}

#pragma endregion Reload

#pragma region Equip

void UCombatComponent::EquipItem(AItem* ItemToEquip)
{
	if (ItemToEquip == nullptr) return;

	if (AWeapon* Weapon = Cast<AWeapon>(ItemToEquip))
	{
		EquipWeapon(Weapon);
	}
}


void UCombatComponent::ServerEquip_Implementation(AItem* ItemToEquip)
{
	if (!ItemToEquip) return;

	AActor* ActorOwner = GetOwner();
	if (!ActorOwner || !ActorOwner->HasAuthority()) return;

	AWeapon* Weapon = Cast<AWeapon>(ItemToEquip);
	if (!Weapon) return;

	const float Distance = FVector::Dist(
		ActorOwner->GetActorLocation(),
		Weapon->GetActorLocation()
	);

	if (Distance > INTERACTION_DISTANCE) return; 

	EquipWeapon(Weapon);
}

void UCombatComponent::EquipWeapon(AWeapon* WeaponToEquip)
{
    if (!OwnerActor || !WeaponToEquip) return;
    if (!OwnerActor->HasAuthority()) return;

    WeaponToEquip->SetOwner(ICombatInterface::Execute_GetCombatOwner(OwnerActor));

    AddWeaponToInventory(WeaponToEquip);

    if (PrimaryEquippedWeapon == nullptr)
    {
       PrimaryEquippedWeapon = WeaponToEquip;
       WeaponToEquip->SetWeaponState(EWeaponState::EWS_Equipped);
       UpdateCarriedAmmo();
       GetPrimaryWeapon()->SetHUDAmmo();
       ReloadEmptyWeapon();
       return;
    }

    if (SecondaryEquippedWeapon == nullptr)
    {
       SecondaryEquippedWeapon = WeaponToEquip;
       WeaponToEquip->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
       return;
    }

    if (PrimaryEquippedWeapon && SecondaryEquippedWeapon)
    {
       StoreWeapon(PrimaryEquippedWeapon); 
       
       PrimaryEquippedWeapon = WeaponToEquip;
       WeaponToEquip->SetWeaponState(EWeaponState::EWS_Equipped);
       UpdateCarriedAmmo();
       GetPrimaryWeapon()->SetHUDAmmo();
       ReloadEmptyWeapon();
    }
}

void UCombatComponent::StoreWeapon(AWeapon* WeaponToStore)
{
	if (!WeaponToStore) return;
	
	// Stop VFX if we put the weapon on our back while holding the trigger
	if (WeaponToStore->bIsChargeWeapon)
	{
		if (AChargeLaserWeapon* ChargeWeapon = Cast<AChargeLaserWeapon>(WeaponToStore))
		{
			ChargeWeapon->CancelCharging();
		}
	}
	
	WeaponToStore->SetWeaponState(EWeaponState::EWS_Stored);
}

#pragma endregion Equip

#pragma region Radial Menu Actions

void UCombatComponent::AddWeaponToInventory(AWeapon* WeaponToAdd)
{
	if (!WeaponToAdd || !OwnerActor || !OwnerActor->HasAuthority()) return;

	if (!WeaponInventory.Contains(WeaponToAdd))
	{
		WeaponInventory.Add(WeaponToAdd);
		WeaponToAdd->SetOwner(ICombatInterface::Execute_GetCombatOwner(OwnerActor));
		NotifySpellOfNewWeapon(WeaponToAdd); // server's own local UStasisSpell
	}
}

// UE auto-populates OldInventory with the pre-replication value for a ReplicatedUsing TArray
void UCombatComponent::OnRep_WeaponInventory(TArray<AWeapon*> OldInventory)
{
	for (AWeapon* Weapon : WeaponInventory)
	{
		if (Weapon && !OldInventory.Contains(Weapon))
		{
			NotifySpellOfNewWeapon(Weapon); // runs locally on every client once THEIR copy updates
		}
	}
}

void UCombatComponent::NotifySpellOfNewWeapon(AWeapon* NewWeapon)
{
	if (!OwnerActor) return;
	if (USpellComponent* SpellComp = OwnerActor->FindComponentByClass<USpellComponent>())
	{
		SpellComp->NotifyWeaponAcquired(NewWeapon);
	}
}

void UCombatComponent::ServerEquipFromMenuToPrimary_Implementation(AWeapon* SelectedWeapon)
{
	if (!SelectedWeapon || !WeaponInventory.Contains(SelectedWeapon)) return;
	if (PrimaryEquippedWeapon == SelectedWeapon) return;

	if (bLocallyReloading || CombatState == ECombatState::ECS_Reloading)
	{
		if (USkeletalMeshComponent* Mesh = ICombatInterface::Execute_GetCombatMesh(OwnerActor))
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				UAnimMontage* ReloadMontage = ICombatInterface::Execute_GetReloadMontage(OwnerActor);
				if (ReloadMontage && AnimInstance->Montage_IsPlaying(ReloadMontage))
				{
					AnimInstance->Montage_Stop(0.1f, ReloadMontage);
				}
			}
		}
		ResetCombatState(); 
	}

	if (SecondaryEquippedWeapon == SelectedWeapon)
	{
		HandleEquippedSwap();
		return;
	}

	if (PrimaryEquippedWeapon)
	{
		StoreWeapon(PrimaryEquippedWeapon);
	}

	PrimaryEquippedWeapon = SelectedWeapon;
	PrimaryEquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);

	UpdateCarriedAmmo();
	GetPrimaryWeapon()->SetHUDAmmo();
	ReloadEmptyWeapon(); 
}

void UCombatComponent::ServerEquipFromMenuToSecondary_Implementation(AWeapon* SelectedWeapon)
{
    if (!SelectedWeapon || !WeaponInventory.Contains(SelectedWeapon)) return;

    if (SecondaryEquippedWeapon == SelectedWeapon) return;

    if (PrimaryEquippedWeapon == SelectedWeapon)
    {
        HandleEquippedSwap();
        return;
    }

    if (SecondaryEquippedWeapon)
    {
        StoreWeapon(SecondaryEquippedWeapon);
    }

    SecondaryEquippedWeapon = SelectedWeapon;
    SecondaryEquippedWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
}

TArray<AWeapon*> UCombatComponent::GetWeaponsInCategory(EWeaponCategory Category)
{
	TArray<AWeapon*> FilteredWeapons;

	for (AWeapon* Weapon : WeaponInventory)
	{
		if (Weapon && Weapon->GetWeaponCategory() == Category)
		{
			FilteredWeapons.Add(Weapon);
		}
	}

	return FilteredWeapons;
}

#pragma endregion Radial Menu Actions

#pragma region Inventory

void UCombatComponent::EquippedSwap()
{
	if (GetPlayerWeaponState() != EPlayerWeaponState::EPWS_HasTwoWeapons) return;

	if (bLocallyReloading || CombatState == ECombatState::ECS_Reloading)
	{
		if (USkeletalMeshComponent* Mesh = ICombatInterface::Execute_GetCombatMesh(OwnerActor))
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				UAnimMontage* ReloadMontage = ICombatInterface::Execute_GetReloadMontage(OwnerActor);
				if (ReloadMontage && AnimInstance->Montage_IsPlaying(ReloadMontage))
				{
					AnimInstance->Montage_Stop(0.1f, ReloadMontage);
				}
			}
		}
		ResetCombatState();
	}

	if (!GetOwner()->HasAuthority())
	{
		bPredictedSwap = true;
		PlayLocalEquippedSwap();
		ServerEquippedSwapWeapon();
	}
	else
	{
		HandleEquippedSwap();
	}
}

void UCombatComponent::PlayLocalEquippedSwap()
{
	if (OwnerActor)
	{
		ICombatInterface::Execute_PlayEquippedSwapMontage(OwnerActor);
	}
}

void UCombatComponent::ServerEquippedSwapWeapon_Implementation()
{
	// Stop any in-progress reload before swapping — mirrors EquippedSwap()'s client-side guard
	if (CombatState == ECombatState::ECS_Reloading)
	{
		if (USkeletalMeshComponent* Mesh = ICombatInterface::Execute_GetCombatMesh(OwnerActor))
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				UAnimMontage* ReloadMontage = ICombatInterface::Execute_GetReloadMontage(OwnerActor);
				if (ReloadMontage && AnimInstance->Montage_IsPlaying(ReloadMontage))
				{
					AnimInstance->Montage_Stop(0.1f, ReloadMontage);
				}
			}
		}
		ResetCombatState(); // clears WeaponBeingReloaded too
	}
	HandleEquippedSwap();
}

void UCombatComponent::HandleEquippedSwap()
{
	CombatState = ECombatState::ECS_SwappingWeapons;
	MulticastEquippedSwapWeapon();
	bFinishedSwapping = false;
}

void UCombatComponent::MulticastEquippedSwapWeapon_Implementation()
{
	if (bPredictedSwap)
	{
		bPredictedSwap = false;
		return;
	}
	
	PlayLocalEquippedSwap();	
}


void UCombatComponent::OnEquippedSwapFinished()
{
	if (!GetOwner()->HasAuthority()) return;

	if (!PrimaryEquippedWeapon || !SecondaryEquippedWeapon)
	{
		CombatState = ECombatState::ECS_Unoccupied;
		bFinishedSwapping = true;
		return;
	}

	CombatState = ECombatState::ECS_Unoccupied;

	AWeapon* Temp = PrimaryEquippedWeapon;
	PrimaryEquippedWeapon = SecondaryEquippedWeapon;
	SecondaryEquippedWeapon = Temp;

	UpdateCarriedAmmo();
	GetPrimaryWeapon()->SetHUDAmmo();

	PrimaryEquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
	SecondaryEquippedWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
	bFinishedSwapping = true;

	ReloadEmptyWeapon(); 
}


void UCombatComponent::ServerDropWeapon_Implementation()
{
	DropEquippedWeapon();
}

void UCombatComponent::DropEquippedWeapon()
{
	if (!GetOwner()->HasAuthority()) return;
	if (!PrimaryEquippedWeapon) return;

	// Stop VFX if we drop the weapon while charging
	if (PrimaryEquippedWeapon->bIsChargeWeapon)
	{
		if (AChargeLaserWeapon* ChargeWeapon = Cast<AChargeLaserWeapon>(PrimaryEquippedWeapon))
		{
			ChargeWeapon->CancelCharging();
		}
	}
	
	if (bLocallyReloading || CombatState == ECombatState::ECS_Reloading)
	{
		if (USkeletalMeshComponent* Mesh = ICombatInterface::Execute_GetCombatMesh(OwnerActor))
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				UAnimMontage* ReloadMontage = ICombatInterface::Execute_GetReloadMontage(OwnerActor);
				if (ReloadMontage && AnimInstance->Montage_IsPlaying(ReloadMontage))
				{
					AnimInstance->Montage_Stop(0.1f, ReloadMontage);
				}
			}
		}
		ResetCombatState(); 
	}

	WeaponInventory.Remove(PrimaryEquippedWeapon);
	PrimaryEquippedWeapon->Dropped();
	PrimaryEquippedWeapon = nullptr;

	if (SecondaryEquippedWeapon)
	{
		PrimaryEquippedWeapon = SecondaryEquippedWeapon;
		SecondaryEquippedWeapon = nullptr;
		PrimaryEquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
       
		UpdateCarriedAmmo();
		PrimaryEquippedWeapon->SetHUDAmmo();
	}

	if (!SecondaryEquippedWeapon)
	{
		for (AWeapon* StoredWeapon : WeaponInventory)
		{
			if (StoredWeapon != PrimaryEquippedWeapon)
			{
				SecondaryEquippedWeapon = StoredWeapon;
				SecondaryEquippedWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
				break; 
			}
		}
	}

	if (!PrimaryEquippedWeapon && !SecondaryEquippedWeapon)
	{
		if (AGASPCorePlayerController* PC = GetCachedController())
		{
			PC->SetHUDWeaponAmmo(0);
			PC->SetHUDCarriedAmmo(0);
		}
	}

	bLocallyReloading = false;
	CombatState = ECombatState::ECS_Unoccupied;
}

void UCombatComponent::ClearWeaponPointers()
{
	PrimaryEquippedWeapon = nullptr;
	SecondaryEquippedWeapon = nullptr;
	CombatState = ECombatState::ECS_Unoccupied;
}
#pragma endregion Inventory

#pragma region Helper Functions

APawn* UCombatComponent::GetOwnerPawn()
{
	if (OwnerPawn) return OwnerPawn;
    
	if (OwnerActor && OwnerActor->Implements<UCombatInterface>())
	{
		OwnerPawn = ICombatInterface::Execute_GetCombatPawn(OwnerActor);
	}
    
	return OwnerPawn;
}

bool UCombatComponent::IsOwnerLocallyControlled() const
{
	return OwnerPawn && OwnerPawn->IsLocallyControlled();
}

EPlayerWeaponState UCombatComponent::GetPlayerWeaponState() const
{
	if (PrimaryEquippedWeapon && SecondaryEquippedWeapon)
		return EPlayerWeaponState::EPWS_HasTwoWeapons;
	if (PrimaryEquippedWeapon)
		return EPlayerWeaponState::EPWS_HasOneWeapon;
	return EPlayerWeaponState::EPWS_HasNoWeapon;
}

AGASPCorePlayerController* UCombatComponent::GetCachedController()
{
	if (Controller) return Controller;
	if (!OwnerPawn) OwnerPawn = GetOwnerPawn(); 
	if (!OwnerPawn) return nullptr;
	Controller = Cast<AGASPCorePlayerController>(OwnerPawn->Controller);
	return Controller;
}

EWeaponType UCombatComponent::GetEquippedWeaponType() const
{
	if (PrimaryEquippedWeapon)
	{
		return PrimaryEquippedWeapon->GetWeaponType();
	}
	return EWeaponType::EWT_EmptyHand;
}

#pragma endregion Helper Functions

void UCombatComponent::ResetCombatState()
{
	CombatState = ECombatState::ECS_Unoccupied;
	bLocallyReloading = false;
	WeaponBeingReloaded = nullptr;
	ShowAttachedGrenade(false);
}