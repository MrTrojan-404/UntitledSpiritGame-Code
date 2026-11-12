#include "SuspensionMatrixProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameAnimationSample/GameAnimationSample.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/SpellAbility/Effect/SuspensionMatrixEffect.h"

ASuspensionMatrixProjectile::ASuspensionMatrixProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent = CollisionSphere;
	CollisionSphere->SetSphereRadius(25.f);
	
	CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_SkeletalMesh, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	
	CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ASuspensionMatrixProjectile::OnOverlap);
	CollisionSphere->OnComponentHit.AddDynamic(this, &ASuspensionMatrixProjectile::OnHit);

	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(RootComponent);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->InitialSpeed = 4000.f;
	ProjectileMovement->MaxSpeed = 4000.f;
	ProjectileMovement->ProjectileGravityScale = 0.1f;
}

void ASuspensionMatrixProjectile::Initialize(AGASPBaseCharacter* InCaster)
{
	Caster = InCaster;
}

void ASuspensionMatrixProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (Caster)
	{
		CollisionSphere->IgnoreActorWhenMoving(Caster, true);
		TArray<AActor*> AttachedActors;
		Caster->GetAttachedActors(AttachedActors);
		for (AActor* AttachedActor : AttachedActors)
		{
			CollisionSphere->IgnoreActorWhenMoving(AttachedActor, true);
		}
	}

	SetLifeSpan(5.f); 
}

void ASuspensionMatrixProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	Explode(OtherActor);
}

void ASuspensionMatrixProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	Explode(OtherActor);
}

void ASuspensionMatrixProjectile::Explode(AActor* HitTargetActor)
{
	if (!HasAuthority()) return;
	if (!HitTargetActor || HitTargetActor == Caster || HitTargetActor == this) return;

	if (bHasExploded) return;
	bHasExploded = true;

	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (MatrixEffectClass)
	{
		FTransform SpawnTransform(FRotator::ZeroRotator, GetActorLocation());
		ASuspensionMatrixEffect* Effect = GetWorld()->SpawnActorDeferred<ASuspensionMatrixEffect>(MatrixEffectClass, SpawnTransform);

		if (Effect)
		{
			Effect->Initialize(Caster);
			Effect->FinishSpawning(SpawnTransform);
		}
	}

	Destroy();
}

float ASuspensionMatrixProjectile::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	Explode(DamageCauser);
	return DamageAmount;
}