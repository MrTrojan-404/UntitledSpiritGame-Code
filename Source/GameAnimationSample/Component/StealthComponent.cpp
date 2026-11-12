#include "StealthComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/AISense_Hearing.h"
 
UStealthComponent::UStealthComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}
 
void UStealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UStealthComponent, ExposureLevel);
    DOREPLIFETIME(UStealthComponent, CurrentNoiseRadius);
}
 
void UStealthComponent::BeginPlay()
{
    Super::BeginPlay();
}
 
void UStealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!GetOwner()->HasAuthority()) return;
 
    UpdateExposureLevel(DeltaTime);
    UpdateMovementNoise(DeltaTime);
}
 
void UStealthComponent::UpdateExposureLevel(float DeltaTime)
{
    ACharacter* Owner = Cast<ACharacter>(GetOwner());
    if (!Owner) return;
 
    UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement();
    if (!MoveComp) return;
 
    float Speed = Owner->GetVelocity().Size();
    bool bCrouching = MoveComp->IsCrouching();
    bool bSprinting = Speed > 400.f;
 
    float TargetExposure;
    if (bCrouching)
        TargetExposure = CrouchExposureMultiplier * FMath::Clamp(Speed / 150.f, 0.1f, 1.f);
    else if (bSprinting)
        TargetExposure = FMath::Clamp(SprintExposureMultiplier, 0.f, 1.f);
    else
        TargetExposure = FMath::Clamp(0.3f + (Speed / 600.f) * 0.7f, 0.f, 1.f);
 
    ExposureLevel = FMath::FInterpTo(ExposureLevel, TargetExposure, DeltaTime, 3.f);
}
 
void UStealthComponent::UpdateMovementNoise(float DeltaTime)
{
    ACharacter* Owner = Cast<ACharacter>(GetOwner());
    if (!Owner) return;
 
    float Speed = Owner->GetVelocity().Size();
    if (Speed < 10.f)
    {
        CurrentNoiseRadius = FMath::Max(0.f, CurrentNoiseRadius - NoiseDecayRate * DeltaTime);
        return;
    }
 
    bool bSprinting = Speed > 400.f;
    float Interval    = bSprinting ? FootstepInterval_Sprint : FootstepInterval_Walk;
    float TargetRadius = bSprinting ? SprintNoiseRadius : WalkNoiseRadius;
 
    FootstepTimer += DeltaTime;
    if (FootstepTimer >= Interval)
    {
        FootstepTimer = 0.f;
        CurrentNoiseRadius = TargetRadius;
        UAISense_Hearing::ReportNoiseEvent(GetWorld(), Owner->GetActorLocation(),
            1.f, Owner, TargetRadius, FName("Footstep"));
    }
    else
    {
        CurrentNoiseRadius = FMath::Max(0.f, CurrentNoiseRadius - NoiseDecayRate * DeltaTime);
    }
}
 
void UStealthComponent::Server_EmitNoise_Implementation(ENoiseType NoiseType, FVector Location)
{
    ACharacter* Owner = Cast<ACharacter>(GetOwner());
    if (!Owner) return;
 
    float Radius = 0.f;
    FName Tag    = NAME_None;
 
    switch (NoiseType)
    {
    // ──────────────────────────────────────────────────────────────────────
    case ENoiseType::Gunshot:
        Radius = GunshotNoiseRadius;
        Tag    = FName("Gunshot");
        break;
 
    // suppressed shot — same tag so AI BT still handles it, but tiny radius
    case ENoiseType::SuppressedGunshot:
        Radius = SuppressedGunshotNoiseRadius;
        Tag    = FName("Gunshot");          // reuse tag so BT reacts; radius does the work
        break;
 
    case ENoiseType::Landing:
        Radius = LandingNoiseRadius;
        Tag    = FName("Landing");
        break;
 
    default: return;
    }
 
    CurrentNoiseRadius = Radius;
    UAISense_Hearing::ReportNoiseEvent(GetWorld(), Location, 1.f, Owner, Radius, Tag);
}