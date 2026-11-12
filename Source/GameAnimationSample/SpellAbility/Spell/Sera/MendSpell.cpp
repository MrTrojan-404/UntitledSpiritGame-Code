#include "MendSpell.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "GameAnimationSample/SpellAbility/SpellComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"

UMendSpell::UMendSpell()
{
	EnergySegmentCost = 1; // Now costs 1 energy segment AND respects the cooldown
	SpellDisplayName = TEXT("Mend");
}

void UMendSpell::Execute_Implementation(AGASPBaseCharacter* Caster)
{
	if (!Caster || !Caster->HasAuthority()) return;

	USpellComponent* SpellComp = Caster->FindComponentByClass<USpellComponent>();

	// 1. Check Cooldown
	float CurrentTime = Caster->GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastCastTime < CooldownSeconds)
	{
		// Refund the energy that the SpellComponent just deducted
		if (SpellComp) SpellComp->RefundEnergySegments(EnergySegmentCost);
		return; 
	}

	// 2. Target Selection: Default to self, but check if we are aiming at an ally
	AGASPBaseCharacter* TargetChar = Caster;

	AController* Controller = Caster->GetController();
	if (Controller)
	{
		FVector ViewLoc;
		FRotator ViewRot;
		Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
		FVector TraceEnd = ViewLoc + (ViewRot.Vector() * TargetingRange);

		FCollisionQueryParams Params;
		Params.AddIgnoredActor(Caster);

		FHitResult Hit;
		float TargetingRadius = 50.f;
		FCollisionShape Sphere = FCollisionShape::MakeSphere(TargetingRadius);

		bool bHit = Caster->GetWorld()->SweepSingleByChannel(
			Hit,
			ViewLoc,
			TraceEnd,
			FQuat::Identity,
			ECC_Visibility,
			Sphere,
			Params
		);

		if (bHit)
		{
			AGASPBaseCharacter* HitChar = Cast<AGASPBaseCharacter>(Hit.GetActor());
			// If we are looking at a valid player ally or companion, target them instead
			if (HitChar && HitChar->IsPlayerAlly())
			{
				TargetChar = HitChar;
			}
		}
	}

	// 3. Prevent Wasting Energy on Full Health
	if (TargetChar->GetHealth() >= TargetChar->GetMaxHealth())
	{
		if (SpellComp) SpellComp->RefundEnergySegments(EnergySegmentCost);
		return;
	}

	// 4. Apply Healing
	LastCastTime = CurrentTime;
	TargetChar->SetHealth(FMath::Clamp(TargetChar->GetHealth() + HealAmount, 0.f, TargetChar->GetMaxHealth()));

	// 5. Push updated health to the target's HUD controller
	if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(TargetChar->GetController()))
	{
		PC->SetHUDHealth(TargetChar->GetHealth(), TargetChar->GetMaxHealth());
	}

	// 6. Push the Cooldown UI to the CASTER'S screen!
	// We check Caster->GetController() specifically so the UI only pops up for the person who cast it.
	if (AGASPCorePlayerController* CasterPC = Cast<AGASPCorePlayerController>(Caster->GetController()))
	{
		CasterPC->Client_StartMendCooldown(CooldownSeconds);
	}
	
	// ==========================================
	// 7. PLAY VFX & SFX
	// ==========================================
	if (HealNiagaraVFX)
	{
		// Spawn it attached to the target's mesh so it follows them if they run around
		UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			HealNiagaraVFX,
			TargetChar->CharacterMesh,
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true // bAutoDestroy when finished
		);

		// Because it's a looping particle, we use a Lambda function on a timer to force it to stop
		if (NiagaraComp)
		{
			FTimerHandle VFXTimerHandle;
			FTimerDelegate TimerDel = FTimerDelegate::CreateLambda([NiagaraComp]()
			{
				// Check if the component still exists (target didn't die/disconnect)
				if (IsValid(NiagaraComp))
				{
					NiagaraComp->Deactivate();
				}
			});

			TargetChar->GetWorldTimerManager().SetTimer(VFXTimerHandle, TimerDel, HealVFXDuration, false);
		}
	}

	if (HealSound)
	{
		// Spawn the sound attached so the audio pans correctly if they walk past you
		UGameplayStatics::SpawnSoundAttached(HealSound, TargetChar->CharacterMesh);
	}
}