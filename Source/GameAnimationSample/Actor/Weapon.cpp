// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon.h"

#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameAnimationSample/GameAnimationSample.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "GameAnimationSample/Interface/CombatInterface.h"
#include "GameAnimationSample/Component/StealthComponent.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "GameAnimationSample/UI/Widgets/WorldSpaceWidget/WorldAmmoWidget.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/Projectile/BulletCasing.h"


AWeapon::AWeapon()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create and attach the Suppressor Mesh
	SuppressorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SuppressorMesh"));
	SuppressorMesh->SetupAttachment(GetItemMesh(), SuppressorSocketName);
	
	// Turn off collision so it doesn't block bullets, traces, or the player
	SuppressorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SuppressorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	
	// Hide it by default
	SuppressorMesh->SetVisibility(false);

	AmmoWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("AmmoWidget"));
	AmmoWidgetComponent->SetupAttachment(RootComponent); 
	AmmoWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen); // Ensures it faces the camera
	AmmoWidgetComponent->SetDrawSize(FVector2D(150.f, 50.f));
	AmmoWidgetComponent->SetVisibility(false); // Hidden by default
	
}

void AWeapon::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWeapon, WeaponState);
	DOREPLIFETIME_CONDITION(AWeapon, bUseServerSideRewind, COND_OwnerOnly);
	DOREPLIFETIME(AWeapon, bSuppressed);
}

FVector AWeapon::TraceEndWithScatter(const FVector& HitTarget, float SpreadMultiplier)
{
	const USkeletalMeshSocket* MuzzleFlashSocket = GetItemMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket == nullptr) return FVector();

	const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetItemMesh());
	const FVector TraceStart = SocketTransform.GetLocation();

	const FVector ToTargetNormalized = (HitTarget - TraceStart).GetSafeNormal();
	const FVector SphereCenter = TraceStart + ToTargetNormalized * DistanceToSphere;
	
	const FVector RandVec = UKismetMathLibrary::RandomUnitVector() * FMath::FRandRange(0.f, SphereRadius * SpreadMultiplier);
	
	const FVector EndLoc = SphereCenter + RandVec;
	const FVector ToEndLoc = EndLoc - TraceStart;
	
	return FVector(TraceStart + ToEndLoc * TRACE_LENGTH / ToEndLoc.Size());
}

void AWeapon::Fire(const FVector& HitTarget)
{
	if (FireAnimation)
	{
		GetItemMesh()->PlayAnimation(FireAnimation, false);
	}
	if (CasingClass)
	{
		const USkeletalMeshSocket* AmmoEjectSocket = GetItemMesh()->GetSocketByName(FName("AmmoEject"));
		if (AmmoEjectSocket)
		{	
			FTransform SocketTransform = AmmoEjectSocket->GetSocketTransform(GetItemMesh());
			FActorSpawnParameters SpawnParameters;
			if (UWorld* World = GetWorld())
			{
				World->SpawnActor<ABulletCasing>(CasingClass, SocketTransform.GetLocation(), SocketTransform.GetRotation().Rotator(), SpawnParameters);
			}
		}
	}
	SpendRound();

	// Centralized Visual & Audio Effects
	const USkeletalMeshSocket* MuzzleFlashSocket = GetItemMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket && GetWorld())
	{
		FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetItemMesh());
		
		if (IsSuppressed())
		{
			// Only spawn if valid. If it's empty, it intentionally spawns nothing.
			if (SuppressedMuzzleFlash)
			{
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), SuppressedMuzzleFlash, SocketTransform);
			}
		}
		else
		{
			if (MuzzleFlash)
			{
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, SocketTransform);
			}
		}
	}

	// Pick the sound based purely on the Suppressed state
	USoundBase* SoundToPlay = IsSuppressed() ? SuppressedFireSound : FireSound;
	
	// If the chosen sound is empty (null), this fails and NO sound is played!
	if (SoundToPlay)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SoundToPlay, GetActorLocation());
	}
}

FText AWeapon::GetInteractionText_Implementation() const
{
	// This will send "Pick up [WeaponName]" to your new CharacterOverlay!
	FString FormattedText = FString::Printf(TEXT("Pick up %s"), *WeaponName);
	return FText::FromString(FormattedText);
}

float AWeapon::CalculateDamage(FName HitBoneName) const
{
	static const FName HeadBoneName(TEXT("head"));
	return (HitBoneName == HeadBoneName) ? HeadShotDamage : Damage;
}

void AWeapon::OnRep_Owner()
{
	Super::OnRep_Owner();
	if (Owner == nullptr)
	{
		ShooterOwnerCharacter = nullptr;
		ShooterOwnerController = nullptr;
	}
	else
	{
		ShooterOwnerCharacter = ShooterOwnerCharacter == nullptr ? Cast<AGASPBaseCharacter>(Owner) : ShooterOwnerCharacter;
		if (ShooterOwnerCharacter && ShooterOwnerCharacter->GetCombatComponent()->GetPrimaryWeapon()
		   && ShooterOwnerCharacter->GetCombatComponent()->GetPrimaryWeapon() == this)
		{
			SetHUDAmmo();
		}
	}
}

void AWeapon::OnPingTooHigh(bool bPingTooHigh)
{
	bUseServerSideRewind = !bPingTooHigh;
}

void AWeapon::ToggleSuppressor()
{
	if (!bHasSuppressor) return;

	// Predict locally so the client can fire immediately with the correct sound/flash/mesh
	bSuppressed = !bSuppressed;

	if (HasAuthority())
	{
		// Server updates its own visual
		OnRep_bSuppressed();
	}
	else
	{
		// Client updates its local visual instantly, then asks the server to tell everyone else
		OnRep_bSuppressed();
		ServerToggleSuppressor();
	}
}

void AWeapon::ServerToggleSuppressor_Implementation()
{
	if (!bHasSuppressor) return;
	bSuppressed = !bSuppressed;

	// The server must call the RepNotify manually so the host can see the mesh update
	OnRep_bSuppressed(); 
}

void AWeapon::OnRep_bSuppressed()
{
	if (SuppressorMesh)
	{
		// Toggle the physical mesh visibility based on the boolean
		SuppressorMesh->SetVisibility(bSuppressed);
	}
}

void AWeapon::ClientUpdateAmmo_Implementation(int32 ServerAmmo)
{
	if (HasAuthority()) return;
	Ammo = ServerAmmo;
	--Sequence;
	Ammo -= Sequence;
	SetHUDAmmo();
}

void AWeapon::AddAmmo(int32 AmmoToAdd)
{
	Ammo = FMath::Clamp(Ammo + AmmoToAdd, 0, MagCapacity);
	SetHUDAmmo();
	ClientAddAmmo(AmmoToAdd);
}

void AWeapon::ClientAddAmmo_Implementation(int32 AmmoToAdd)
{
	if (HasAuthority()) return;
	Ammo = FMath::Clamp(Ammo + AmmoToAdd, 0, MagCapacity);
	ShooterOwnerCharacter = ShooterOwnerCharacter == nullptr ? Cast<AGASPBaseCharacter>(GetOwner()) : ShooterOwnerCharacter;
	if (ShooterOwnerCharacter && ShooterOwnerCharacter->GetCombatComponent() && IsFull())
	{
		ShooterOwnerCharacter->GetCombatComponent()->JumpToShotgunEnd();
	}
	SetHUDAmmo();
}

void AWeapon::SpendRound()
{
	Ammo = FMath::Clamp(Ammo - 1, 0, MagCapacity);
	SetHUDAmmo();

	if (HasAuthority())
	{
		ClientUpdateAmmo(Ammo);
	}
	else
	{
		++Sequence;
	}
}

void AWeapon::SetHUDAmmo()
{
	ShooterOwnerCharacter = ShooterOwnerCharacter == nullptr ? Cast<AGASPPlayerCharacter>(GetOwner()) : ShooterOwnerCharacter;
	if (ShooterOwnerCharacter)
	{
		ShooterOwnerController = ShooterOwnerController == nullptr ? Cast<AGASPCorePlayerController>(ShooterOwnerCharacter->Controller) : ShooterOwnerController;
		if (ShooterOwnerController)
		{
			ShooterOwnerController->SetHUDWeaponAmmo(Ammo);
		}
	}
}

void AWeapon::SetWeaponState(EWeaponState WeaponStatetoSet)
{
	WeaponState = WeaponStatetoSet;
	OnWeaponStateSet();
}

void AWeapon::Dropped()
{
	SetOwner(nullptr);
	ShooterOwnerCharacter = nullptr;
	ShooterOwnerController = nullptr;

	SetWeaponState(EWeaponState::EWS_Dropped);
}

void AWeapon::OnRep_WeaponState()
{
	OnWeaponStateSet();
}

void AWeapon::OnWeaponStateSet()
{
	switch (WeaponState)
	{
	case EWeaponState::EWS_Equipped:
		OnEquipped();
		break;
	case EWeaponState::EWS_EquippedSecondary:
		OnEquippedSecondary();
		break;
	case EWeaponState::EWS_Dropped:
		OnDropped();
		break;
	case EWeaponState::EWS_Stored:
		OnStored();
		break;
	}
}

void AWeapon::DisablePhysicsAndCollision()
{
	// 1. Disable mesh collision
	GetItemMesh()->SetSimulatePhysics(false);
	GetItemMesh()->SetEnableGravity(false);
	GetItemMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetItemMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);

	// 2. Disable TraceBox collision and Overlaps
	if (TraceBox)
	{
		TraceBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TraceBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	}
}

void AWeapon::OnDropped()
{
	// 1. Detach FIRST
	FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
	GetItemMesh()->DetachFromComponent(DetachRules);

	// 2. BULLETPROOF COLLISION SETUP
	GetItemMesh()->SetCollisionObjectType(ECC_PhysicsBody);
	GetItemMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetItemMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
	
	// EXPLICITLY block the ground and environment!
	GetItemMesh()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); 
	GetItemMesh()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	
	// Allow the Interact line trace to hit it
	GetItemMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); 

	// 3. Enable Physics
	GetItemMesh()->SetAllBodiesSimulatePhysics(true);
	GetItemMesh()->SetSimulatePhysics(true);
	GetItemMesh()->SetEnableGravity(true);
	GetItemMesh()->WakeAllRigidBodies();
	GetItemMesh()->SetUseCCD(true); 

	SetReplicateMovement(true);
	
	// 4. Setup the TraceBox for the Interact prompt
	TraceBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TraceBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TraceBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	
	// 5. Visuals
	GetItemMesh()->SetCustomDepthStencilValue(DroppedWeaponStencil);
	GetItemMesh()->MarkRenderStateDirty();
	EnableCustomDepth(true);
	SetActorHiddenInGame(false);
}

void AWeapon::OnEquipped()
{
	DisablePhysicsAndCollision();

	if (WeaponType == EWeaponType::EWT_SubmachineGun)
	{
		GetItemMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		GetItemMesh()->SetEnableGravity(true);
		GetItemMesh()->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
		GetItemMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Ignore);
	}
	
	AttachToSocket(PrimarySocketName);
	EnableCustomDepth(false);
	SetActorHiddenInGame(false);

	SetHUDAmmo();
	
	ShooterOwnerCharacter = ShooterOwnerCharacter == nullptr ? Cast<AGASPBaseCharacter>(GetOwner()) : ShooterOwnerCharacter;
	if (ShooterOwnerCharacter && bUseServerSideRewind)
	{
		ShooterOwnerController = ShooterOwnerController == nullptr ? Cast<AGASPCorePlayerController>(ShooterOwnerCharacter->Controller) : ShooterOwnerController;
		if (ShooterOwnerController && HasAuthority() && !ShooterOwnerController->HighPingDelegate.IsBound())
		{
			ShooterOwnerController->HighPingDelegate.AddDynamic(this, &AWeapon::OnPingTooHigh);
		}
	}
}

void AWeapon::OnStored()
{
	DisablePhysicsAndCollision();

	FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, false);
	GetItemMesh()->DetachFromComponent(DetachRules);
	SetActorHiddenInGame(true); 
}

void AWeapon::OnEquippedSecondary()
{
	DisablePhysicsAndCollision();

	if (WeaponType == EWeaponType::EWT_SubmachineGun)
	{
		GetItemMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		GetItemMesh()->SetEnableGravity(true);
		GetItemMesh()->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	}
	
	AttachToSocket(SecondarySocketName);
	SetActorHiddenInGame(false);
	EnableCustomDepth(true);
	
	GetItemMesh()->SetCustomDepthStencilValue(SecondaryWeaponStencil);
	GetItemMesh()->MarkRenderStateDirty();
}

void AWeapon::AttachToSocket(FName SocketName)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	ICombatInterface* CombatOwner = Cast<ICombatInterface>(OwnerActor);
	if (!CombatOwner) return;

	USkeletalMeshComponent* Mesh = ICombatInterface::Execute_GetCombatMesh(OwnerActor);
	if (!Mesh) return;

	const USkeletalMeshSocket* Socket = Mesh->GetSocketByName(SocketName);
	if (Socket)
	{
		Socket->AttachActor(this, Mesh);
	}
}

void AWeapon::Set3DAmmoVisibility(bool bVisible)
{
	if (AmmoWidgetComponent)
	{
		AmmoWidgetComponent->SetVisibility(bVisible);
	}
}

void AWeapon::Update3DAmmoUI(int32 CarriedAmmo)
{
	if (AmmoWidgetComponent && AmmoWidgetComponent->GetWidget())
	{
		if (UWorldAmmoWidget* AmmoWidget = Cast<UWorldAmmoWidget>(AmmoWidgetComponent->GetWidget()))
		{
			AmmoWidget->SetAmmoText(GetAmmo(), CarriedAmmo);
		}
	}
}

bool AWeapon::IsEmpty()
{
	return Ammo <= 0;
}

bool AWeapon::IsFull()
{
	return Ammo == MagCapacity;
}