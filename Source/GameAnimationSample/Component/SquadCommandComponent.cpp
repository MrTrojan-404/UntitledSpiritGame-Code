#include "SquadCommandComponent.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"
#include "GameAnimationSample/AI/Character/Companion/GASPCompanionCharacter.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "GameAnimationSample/Component/DownedComponent.h"
#include "Components/DecalComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Engine/DecalActor.h"
#include "GameAnimationSample/AI/Character/GASPEnemyBase.h"
#include "GameAnimationSample/Subsystem/StealthSubsystem.h"
#include "Kismet/GameplayStatics.h"

USquadCommandComponent::USquadCommandComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void USquadCommandComponent::BeginPlay()
{
    Super::BeginPlay();

    if (TargetingDecalMaterial)
    {
        TargetingDecalActor = GetWorld()->SpawnActor<ADecalActor>(FVector::ZeroVector, FRotator::ZeroRotator);
        if (TargetingDecalActor)
        {
            TargetingDecalActor->SetDecalMaterial(TargetingDecalMaterial);
            TargetingDecalActor->GetDecal()->DecalSize = TargetingDecalSize; 
            TargetingDecalActor->SetActorHiddenInGame(true);
        }
    }
}

void USquadCommandComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bHasInitializedCompanionOrder)
    {
        AGASPBaseCharacter* Companion = GetCurrentCompanion();
        if (Companion)
        {
            APlayerController* PC = Cast<APlayerController>(GetOwner());
            APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;

            if (AGASPAIController* AICon = Cast<AGASPAIController>(Companion->GetController()))
            {
                AICon->IssueOrder(ECompanionOrder::Follow, FVector::ZeroVector, PlayerPawn);
                bCompanionCurrentlyFollowing = true;
                bHasInitializedCompanionOrder = true; // only do this once at startup
            }
        }
    }
    
    if (CurrentTargetingMode == ETargetingMode::ETM_None) return;

    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC) return;

    UCombatComponent* CombatComp = nullptr;
    if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(PC->GetPawn()))
    {
        CombatComp = BaseChar->GetCombatComponent();
    }

    if (TargetingDecalActor && CombatComp)
    {
        TargetingDecalActor->SetActorLocation(CombatComp->GetHitTarget());
        TargetingDecalActor->SetActorRotation(FRotator(-90.f, 0.f, 0.f));
        TargetingDecalActor->SetActorHiddenInGame(HoveredDownedAlly != nullptr);
    }

   // 1. Find the Hovered Downed Ally
    AGASPBaseCharacter* NewHoveredAlly = nullptr;
    float BestAimScore = 0.97f;

    TArray<AActor*> FoundCompanions;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPBaseCharacter::StaticClass(), FoundCompanions);

    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);
    FVector CamForward = CamRot.Vector();

    for (AActor* Actor : FoundCompanions)
    {
        AGASPBaseCharacter* Comp = Cast<AGASPBaseCharacter>(Actor);
        
        // Skip nulls and ANYONE who isn't our ally!
        if (Comp && Comp->IsPlayerAlly())
        {
            if (UDownedComponent* DownedComp = Comp->FindComponentByClass<UDownedComponent>())
            {
                if (DownedComp->IsDowned())
                {
                    FVector DirToAlly = (Comp->GetCombatMesh_Implementation()->GetComponentLocation() - CamLoc).GetSafeNormal();
                    float AimScore = FVector::DotProduct(CamForward, DirToAlly);

                    if (AimScore > BestAimScore)
                    {
                        BestAimScore = AimScore;
                        NewHoveredAlly = Comp;
                    }
                }
            }
        }
    }

    // 2. Now update the Outlines for everyone
    for (AActor* Actor : FoundCompanions)
    {
        AGASPBaseCharacter* Comp = Cast<AGASPBaseCharacter>(Actor);
        if (!Comp || Cast<AGASPEnemyBase>(Comp)) continue; // Skip nulls and enemies

        bool bIsDowned = false;
        if (UDownedComponent* DownedComp = Comp->FindComponentByClass<UDownedComponent>())
        {
            bIsDowned = DownedComp->IsDowned();
        }

        if (Comp == NewHoveredAlly)
        {
            Comp->EnableTargetingOutline(true, HoveredDownedAllyStencil);
        }
        else if (bIsDowned)
        {
            // It's a downed ally (Player or Companion)
            Comp->EnableTargetingOutline(true, DownedAllyStencil);
        }
        else if (Cast<AGASPAIController>(Comp->GetController())) 
        {
            // It's an ALIVE AI Companion (They are ready to receive orders)
            if (CurrentTargetingMode == ETargetingMode::ETM_CompanionMoveToSpecific && Comp != ActiveSelectedCompanion)
            {
                Comp->EnableTargetingOutline(false);
            }
            else
            {
                Comp->EnableTargetingOutline(true, NormalCompanionStencil);
            }
        }
        else 
        {
            // It's an ALIVE Human Player
            Comp->EnableTargetingOutline(false);
        }
    }

    HoveredDownedAlly = NewHoveredAlly;
    if (TargetingDecalActor) TargetingDecalActor->SetActorHiddenInGame(HoveredDownedAlly != nullptr);
    
    // Auto-clear mark if LOS is lost mid-tick
    if (MarkedTarget)
    {
        AGASPBaseCharacter* Companion = GetCurrentCompanion(); // was ActiveSelectedCompanion
        if (!Companion || !HasLineOfSight(Companion, MarkedTarget))
        {
            ClearMark();
        }
    }
}

bool USquadCommandComponent::HasLineOfSight(AGASPBaseCharacter* From, AActor* Target) const
{
	if (!From || !Target) return false;

	FHitResult Hit;
	FVector Start = From->GetPawnViewLocation();
	FVector End = Target->GetActorLocation();

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(From);
	Params.AddIgnoredActor(Target);

	bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	return !bBlocked;
}

AGASPBaseCharacter* USquadCommandComponent::GetCurrentCompanion() const
{
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC) return nullptr;

    APawn* PlayerPawn = PC->GetPawn();

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPBaseCharacter::StaticClass(), FoundActors);

    for (AActor* Actor : FoundActors)
    {
        AGASPBaseCharacter* Char = Cast<AGASPBaseCharacter>(Actor);
        if (!Char || Char == PlayerPawn) continue;
        if (!Char->IsPlayerAlly()) continue;

        // Must be AI-driven right now, not another player's body
        if (!Cast<AGASPAIController>(Char->GetController())) continue;

        if (UDownedComponent* Downed = Char->FindComponentByClass<UDownedComponent>())
        {
            if (Downed->IsDowned()) continue;
        }

        return Char; // Only one companion active at a time by design
    }

    return nullptr;
}

void USquadCommandComponent::TryMarkTarget(AActor* Target)
{
    if (!Target) return;

    AGASPBaseCharacter* Companion = GetCurrentCompanion();
    if (!Companion) return;

    // Range check: from the COMPANION to the target, using the companion's own engagement range —
    // not the player's trace distance. A companion with a pistol shouldn't "snipe" something
    // only the player's camera can see.
    if (AGASPCompanionCharacter* CompanionChar = Cast<AGASPCompanionCharacter>(Companion))
    {
        float DistToTarget = FVector::Dist(Companion->GetActorLocation(), Target->GetActorLocation());
        if (DistToTarget > CompanionChar->EngagementRange)
        {
            return; // Too far for this companion to realistically execute — silently fail, no mark
        }
    }

    if (!HasLineOfSight(Companion, Target))
    {
        MarkedTarget = nullptr;
        return;
    }

    MarkedTarget = Target;
    bMarkReadyToExecute = false;

    if (AGASPBaseCharacter* TargetChar = Cast<AGASPBaseCharacter>(Target))
    {
        TargetChar->EnableTargetingOutline(true, MarkedLiningUpStencil);
    }

    GetWorld()->GetTimerManager().SetTimer(MarkLineupTimer, this,
        &USquadCommandComponent::OnLineupComplete, MarkLineupDuration, false);
}

void USquadCommandComponent::OnLineupComplete()
{
    if (!MarkedTarget) return; // target was lost mid-lineup (LOS-loss already clears the mark)

    bMarkReadyToExecute = true;

    if (AGASPBaseCharacter* TargetChar = Cast<AGASPBaseCharacter>(MarkedTarget))
    {
        TargetChar->EnableTargetingOutline(true, MarkedReadyStencil); // swap color, still outlined
    }
}

void USquadCommandComponent::ToggleFollowStandBy()
{
    AGASPBaseCharacter* Companion = GetCurrentCompanion();
    if (!Companion) return;

    AGASPAIController* AICon = Cast<AGASPAIController>(Companion->GetController());
    if (!AICon) return;

    APlayerController* PC = Cast<APlayerController>(GetOwner());
    APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;

    bCompanionCurrentlyFollowing = !bCompanionCurrentlyFollowing;

    if (bCompanionCurrentlyFollowing)
    {
        AICon->IssueOrder(ECompanionOrder::Follow, FVector::ZeroVector, PlayerPawn);
    }
    else
    {
        AICon->IssueOrder(ECompanionOrder::StandBy, FVector::ZeroVector, nullptr);
    }
}

void USquadCommandComponent::ClearMark()
{
    GetWorld()->GetTimerManager().ClearTimer(MarkLineupTimer);
    bMarkReadyToExecute = false;

    if (AGASPBaseCharacter* TargetChar = Cast<AGASPBaseCharacter>(MarkedTarget))
    {
        TargetChar->EnableTargetingOutline(false);
    }
    MarkedTarget = nullptr;
}

void USquadCommandComponent::ExecuteMark()
{
    if (!MarkedTarget || !bMarkReadyToExecute) return; // physically cannot fire early

    GetWorld()->GetTimerManager().ClearTimer(MarkLineupTimer);

    AGASPBaseCharacter* Companion = GetCurrentCompanion();
    if (!Companion || !HasLineOfSight(Companion, MarkedTarget))
    {
        ClearMark();
        return;
    }

    if (AGASPAIController* AICon = Cast<AGASPAIController>(Companion->GetController()))
    {
        AICon->IssueOrder(ECompanionOrder::AttackTarget, FVector::ZeroVector, MarkedTarget);
    }

    ClearMark();
}

#pragma region Squad Commands UI

void USquadCommandComponent::HighlightAllCompanions(bool bEnable)
{
    TArray<AActor*> FoundCompanions;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPBaseCharacter::StaticClass(), FoundCompanions);
    for (AActor* Actor : FoundCompanions)
    {
        if (AGASPBaseCharacter* Comp = Cast<AGASPBaseCharacter>(Actor))
        {
            // Must be an Ally AND driven by AI
            if (Comp->IsPlayerAlly() && Cast<AGASPAIController>(Comp->GetController())) 
            {
                UDownedComponent* Downed = Comp->FindComponentByClass<UDownedComponent>();
                if (!Downed || !Downed->IsDowned())
                {
                    Comp->EnableTargetingOutline(bEnable, NormalCompanionStencil);
                }
            }
        }
    }
}

#pragma endregion Squad Commands UI

#pragma region Targeting System

void USquadCommandComponent::StartTargetingMode(ETargetingMode Mode)
{
    CurrentTargetingMode = Mode;

    if (TargetingDecalActor) TargetingDecalActor->SetActorHiddenInGame(false);

    if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
    {
        if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(PC->GetPawn()))
        {
            if (UCombatComponent* Combat = BaseChar->GetCombatComponent()) Combat->bShowCrosshairs = false;
        }
    }

    HoveredDownedAlly = nullptr;
    TArray<AActor*> FoundCompanions;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPBaseCharacter::StaticClass(), FoundCompanions);

    for (AActor* Actor : FoundCompanions)
    {
        if (AGASPBaseCharacter* Comp = Cast<AGASPBaseCharacter>(Actor))
        {
            if (Cast<AGASPEnemyBase>(Comp)) continue; // Skip Enemies
            
            bool bIsDowned = false;
            if (UDownedComponent* DownedComp = Comp->FindComponentByClass<UDownedComponent>())
            {
                bIsDowned = DownedComp->IsDowned();
            }

            // We want to outline AI Companions (to show who is receiving orders) 
            // AND we want to outline ANY Downed Ally (to show they can be revived)
            if (!bIsDowned && !Cast<AGASPAIController>(Comp->GetController())) continue;

            if (CurrentTargetingMode == ETargetingMode::ETM_CompanionMoveToSpecific && Comp != ActiveSelectedCompanion && !bIsDowned) continue;

            Comp->EnableTargetingOutline(true, bIsDowned ? DownedAllyStencil : NormalCompanionStencil);
        }
    }
}

void USquadCommandComponent::CancelTargetingMode()
{
    CurrentTargetingMode = ETargetingMode::ETM_None;

    if (TargetingDecalActor) TargetingDecalActor->SetActorHiddenInGame(true);

    if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
    {
        if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(PC->GetPawn()))
        {
            if (UCombatComponent* Combat = BaseChar->GetCombatComponent()) Combat->bShowCrosshairs = true;
        }
    }

    TArray<AActor*> FoundCompanions;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPBaseCharacter::StaticClass(), FoundCompanions);
    for (AActor* Actor : FoundCompanions)
    {
        if (AGASPBaseCharacter* Comp = Cast<AGASPBaseCharacter>(Actor))
        {
            // Aggressively turn off the outline for EVERYONE (Players, Companions, Enemies)
            Comp->EnableTargetingOutline(false);
        }
    }

    HoveredDownedAlly = nullptr;
}

void USquadCommandComponent::ConfirmTargetLocation()
{
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    FVector TargetLocation = FVector::ZeroVector;

    if (PC && PC->GetPawn())
    {
        if (UCombatComponent* Combat = PC->GetPawn()->FindComponentByClass<UCombatComponent>())
        {
            TargetLocation = Combat->GetHitTarget();
        }
    }

    if (HoveredDownedAlly)
    {
        if (CurrentTargetingMode == ETargetingMode::ETM_CompanionMoveToAll)
            ServerDispatchMedicToAlly(HoveredDownedAlly, nullptr);
        else if (CurrentTargetingMode == ETargetingMode::ETM_CompanionMoveToSpecific)
            ServerDispatchMedicToAlly(HoveredDownedAlly, ActiveSelectedCompanion);
    }
    else
    {
        switch (CurrentTargetingMode)
        {
        case ETargetingMode::ETM_CompanionMoveToAll:
            ServerIssueSquadOrder(0, TargetLocation);
            break;
        case ETargetingMode::ETM_CompanionMoveToSpecific:
            ServerIssueSpecificOrder(0, ActiveSelectedCompanion, TargetLocation);
            break;
        case ETargetingMode::ETM_Airstrike:
            break;
        }
    }
    CancelTargetingMode();
}

#pragma endregion Targeting System

#pragma region Server RPCs

void USquadCommandComponent::ServerIssueSquadOrder_Implementation(int32 SelectedSegment, FVector OrderLocation)
{
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    AActor* CommandSource = PC ? PC->GetPawn() : nullptr; 

    TArray<AActor*> AllCompanions;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPBaseCharacter::StaticClass(), AllCompanions);

    for (AActor* Actor : AllCompanions)
    {
        if (AGASPBaseCharacter* Companion = Cast<AGASPBaseCharacter>(Actor))
        {
            if (Companion->IsPlayerAlly())
            {
                if (AGASPAIController* AICon = Cast<AGASPAIController>(Companion->GetController())) // ONLY ORDER AI!
                {
                    switch (SelectedSegment)
                    {
                    case 0: AICon->IssueOrder(ECompanionOrder::MoveToLocation, OrderLocation, CommandSource); break;
                    case 1: AICon->IssueOrder(ECompanionOrder::WeaponsFree, FVector::ZeroVector, CommandSource); break;
                    case 2: AICon->IssueOrder(ECompanionOrder::Follow, FVector::ZeroVector, CommandSource); break;
                    case 3: AICon->IssueOrder(ECompanionOrder::StandBy, FVector::ZeroVector, CommandSource); break;
                    case 4: AICon->IssueOrder(ECompanionOrder::HoldFire, FVector::ZeroVector, CommandSource); break;
                    }
                }
            }
        }
    }
}

void USquadCommandComponent::ServerIssueSpecificOrder_Implementation(int32 SelectedSegment, AGASPBaseCharacter* TargetCompanion, FVector OrderLocation)
{
    if (!TargetCompanion) return;
    
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    AActor* CommandSource = PC ? PC->GetPawn() : nullptr;

    if (AGASPAIController* AICon = Cast<AGASPAIController>(TargetCompanion->GetController()))
    {
        switch (SelectedSegment)
        {
        case 0: AICon->IssueOrder(ECompanionOrder::MoveToLocation, OrderLocation, CommandSource); break;
        case 1: AICon->IssueOrder(ECompanionOrder::WeaponsFree, FVector::ZeroVector, CommandSource); break;
        case 2: AICon->IssueOrder(ECompanionOrder::Follow, FVector::ZeroVector, CommandSource); break;
        case 3: AICon->IssueOrder(ECompanionOrder::StandBy, FVector::ZeroVector, CommandSource); break;
        case 4: AICon->IssueOrder(ECompanionOrder::HoldFire, FVector::ZeroVector, CommandSource); break;
        }
    }
}

void USquadCommandComponent::ServerDispatchMedicToAlly_Implementation(AGASPBaseCharacter* DownedAlly, AGASPBaseCharacter* SpecificMedic)
{
    if (!DownedAlly) return;

    AGASPBaseCharacter* BestMedic = SpecificMedic;

    if (!BestMedic)
    {
        TArray<AActor*> FoundCompanions;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPBaseCharacter::StaticClass(), FoundCompanions);

        float MinDist = MAX_flt;

        for (AActor* Actor : FoundCompanions)
        {
            AGASPBaseCharacter* Comp = Cast<AGASPBaseCharacter>(Actor);
            if (!Comp || Comp == DownedAlly) continue;
            
            if (!Cast<AGASPAIController>(Comp->GetController())) continue; // MEDICS MUST BE AI!

            if (UDownedComponent* Downed = Comp->FindComponentByClass<UDownedComponent>())
            {
                if (Downed->IsDowned()) continue;
            }

            float ActualWalkingDistance = MAX_flt;
            if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
            {
                UNavigationPath* NavPath = NavSys->FindPathToActorSynchronously(GetWorld(), Comp->GetActorLocation(), DownedAlly);
                if (NavPath && NavPath->IsValid())
                {
                    ActualWalkingDistance = NavPath->GetPathLength();
                }
            }

            if (ActualWalkingDistance < MinDist)
            {
                MinDist = ActualWalkingDistance;
                BestMedic = Comp;
            }
        }
    }

    if (BestMedic)
    {
        if (AGASPAIController* AICon = Cast<AGASPAIController>(BestMedic->GetController()))
        {
            AICon->IssueOrder(ECompanionOrder::ReviveAlly, FVector::ZeroVector, DownedAlly);
        }
    }
}

void USquadCommandComponent::ServerCallForMedic_Implementation(AGASPBaseCharacter* DownedAlly)
{
    if (!DownedAlly) return;

    TArray<AActor*> FoundCompanions;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPBaseCharacter::StaticClass(), FoundCompanions);

    AGASPBaseCharacter* BestMedic = nullptr;
    float MinDist = MAX_flt;

    for (AActor* Actor : FoundCompanions)
    {
        AGASPBaseCharacter* Comp = Cast<AGASPBaseCharacter>(Actor);
        if (!Comp) continue;

        if (!Cast<AGASPAIController>(Comp->GetController())) continue; // MEDICS MUST BE AI!

        if (UDownedComponent* Downed = Comp->FindComponentByClass<UDownedComponent>())
        {
            if (Downed->IsDowned()) continue;
        }

        float Dist = FVector::Dist(DownedAlly->GetActorLocation(), Comp->GetActorLocation());
        if (Dist < MinDist)
        {
            MinDist = Dist;
            BestMedic = Comp;
        }
    }

    if (BestMedic)
    {
        if (AGASPAIController* AICon = Cast<AGASPAIController>(BestMedic->GetController()))
        {
            AICon->IssueOrder(ECompanionOrder::ReviveAlly, FVector::ZeroVector, DownedAlly);
        }
    }
}

#pragma endregion Server RPCs

FVector USquadCommandComponent::GetCrosshairHitTarget() const
{
    if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
    {
        if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(PC->GetPawn()))
        {
            if (UCombatComponent* Combat = BaseChar->GetCombatComponent()) return Combat->GetHitTarget();
        }
    }
    return FVector::ZeroVector;
}