#include "SpellComponent.h"
#include "SpellBase.h"
#include "GameAnimationSample/Actor/Weapon/HitScanWeapon.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Spell/Ren/StasisSpell.h"

USpellComponent::USpellComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void USpellComponent::BeginPlay()
{
	Super::BeginPlay();
	CharacterOwner = Cast<AGASPBaseCharacter>(GetOwner());

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (bStartWithFullEnergy)
		{
			CurrentEnergySegments = MaxEnergySegments;
			OnRep_EnergySegments();
		}

		if (bStartWithFullUltimate)
		{
			UltimateBarCurrent = UltimateBarMax;
			OnRep_UltimateBar();
		}
	}
}

void USpellComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// DeltaTime here is already the DILATED delta — for a Stasis-tracking purpose we need
	// the real undilated value instead, since that's the whole point of Gotcha 1.
	if (Ultimate)
	{
		if (UStasisSpell* Stasis = Cast<UStasisSpell>(Ultimate))
		{
			if (Stasis->IsActive() && GetWorld())
			{
				/*float RealDelta = DeltaTime / FMath::Max(GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation(), KINDA_SMALL_NUMBER);
				Stasis->TickStasis(RealDelta);*/
				// FIX: DeltaTime already includes the Caster's 50x CustomTimeDilation. 
				// It is already running in "real time" for this specific component.
				Stasis->TickStasis(DeltaTime);
			}
		}
	}
}

void USpellComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USpellComponent, CurrentEnergySegments);
	DOREPLIFETIME(USpellComponent, UltimateBarCurrent);
}

// --- ENERGY GENERATION ---

void USpellComponent::OnDamageDealt(float DamageAmount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (DamagePerEnergySegment <= 0.f) return;

	AddEnergy(DamageAmount / DamagePerEnergySegment);
}

void USpellComponent::OnKillScored(bool bWasStealthKill)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	AddEnergy(bWasStealthKill ? EnergyPerStealthKill : EnergyPerCombatKill);

	UltimateBarCurrent = FMath::Clamp(
		UltimateBarCurrent + (bWasStealthKill ? UltimateGainPerStealthKill : UltimateGainPerCombatKill),
		0.f, UltimateBarMax);
	OnRep_UltimateBar(); // server-side doesn't get its own RepNotify — call manually for local UI hook
}

void USpellComponent::AddEnergy(float Amount)
{
	FractionalEnergyAccumulator += Amount;

	while (FractionalEnergyAccumulator >= 1.f && CurrentEnergySegments < MaxEnergySegments)
	{
		FractionalEnergyAccumulator -= 1.f;
		CurrentEnergySegments++;
	}

	// Don't let fractional buildup keep climbing once the bar is already full — avoids a
	// "banked" burst of segments the instant a spell is cast and space opens up.
	if (CurrentEnergySegments >= MaxEnergySegments)
	{
		FractionalEnergyAccumulator = 0.f;
	}

	OnRep_EnergySegments();
}

void USpellComponent::OnRep_EnergySegments()
{
	// Ensure the body exists and is currently being driven by a Human Player
	if (CharacterOwner)
	{
		if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(CharacterOwner->GetController()))
		{
			PC->SetHUDEnergy(CurrentEnergySegments, MaxEnergySegments);
		}
	}
}

void USpellComponent::OnRep_UltimateBar()
{
	// Ensure the body exists and is currently being driven by a Human Player
	if (CharacterOwner)
	{
		if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(CharacterOwner->GetController()))
		{
			PC->SetHUDUltimate(UltimateBarCurrent, UltimateBarMax);
		}
	}
}
// --- CASTING ---

void USpellComponent::TryCastSpell1()
{
	if (GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
	{
		Server_CastSpell1();
	}
	else if (GetOwner()->HasAuthority())
	{
		Server_CastSpell1_Implementation();
	}
}

void USpellComponent::Server_CastSpell1_Implementation()
{
	if (!Spell1) return;
	if (CurrentEnergySegments < Spell1->EnergySegmentCost) return;

	CurrentEnergySegments -= Spell1->EnergySegmentCost;
	OnRep_EnergySegments();
	CastSpell(Spell1, false);
}

void USpellComponent::TryCastSpell2()
{
	if (GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
	{
		Server_CastSpell2();
	}
	else if (GetOwner()->HasAuthority())
	{
		Server_CastSpell2_Implementation();
	}
}

void USpellComponent::Server_CastSpell2_Implementation()
{
	if (!Spell2) return;
	if (CurrentEnergySegments < Spell2->EnergySegmentCost) return;

	CurrentEnergySegments -= Spell2->EnergySegmentCost;
	OnRep_EnergySegments();
	CastSpell(Spell2, false);
}

void USpellComponent::ReleaseSpell2()
{
	if (GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
	{
		Server_ReleaseSpell2();
	}
	else if (GetOwner()->HasAuthority())
	{
		Server_ReleaseSpell2_Implementation();
	}
}

void USpellComponent::Server_ReleaseSpell2_Implementation()
{
	if (!Spell2 || !CharacterOwner) return;
	Spell2->OnReleased(CharacterOwner);
}

void USpellComponent::TryCastUltimate()
{
	if (GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
	{
		Server_CastUltimate();
	}
	else if (GetOwner()->HasAuthority())
	{
		Server_CastUltimate_Implementation();
	}
}

void USpellComponent::Server_CastUltimate_Implementation()
{
	if (!Ultimate) return;
	if (!IsUltimateReady()) return;

	UltimateBarCurrent = 0.f;
	OnRep_UltimateBar();
	CastSpell(Ultimate, true);
}

void USpellComponent::CastSpell(USpellBase* Spell, bool bIsUltimateSlot)
{
	if (!Spell || !CharacterOwner) return;
	Spell->Execute(CharacterOwner);
}

void USpellComponent::Multicast_ActivateStasis_Implementation(float Duration, float DilationFactor)
{
	if (UStasisSpell* Stasis = Cast < UStasisSpell > (Ultimate))
	{
		Stasis->LocalActivate(CharacterOwner, Duration, DilationFactor);
	}
}

void USpellComponent::Multicast_DeactivateStasis_Implementation()
{
	if (UStasisSpell* Stasis = Cast < UStasisSpell > (Ultimate))
	{
		Stasis->LocalDeactivate();
	}
}

void USpellComponent::RefundEnergySegments(int32 SegmentsToRefund)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	CurrentEnergySegments = FMath::Clamp(CurrentEnergySegments + SegmentsToRefund, 0, MaxEnergySegments);
	OnRep_EnergySegments();
}

void USpellComponent::NotifyWeaponAcquired(AWeapon* NewWeapon)
{
	if (UStasisSpell* Stasis = Cast<UStasisSpell>(Ultimate))
	{
		Stasis->RegisterWeaponForStasis(NewWeapon);
	}
}