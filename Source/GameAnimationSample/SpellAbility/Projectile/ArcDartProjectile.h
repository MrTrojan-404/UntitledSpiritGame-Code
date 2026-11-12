#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/Actor/Weapon/Projectile/Projectile.h"
#include "ArcDartProjectile.generated.h"

class AGASPBaseCharacter;

UCLASS()
class GAMEANIMATIONSAMPLE_API AArcDartProjectile : public AProjectile
{
	GENERATED_BODY()

public:
	AArcDartProjectile();

	void Initialize(AGASPBaseCharacter* InCaster);

protected:
	virtual void BeginPlay() override;
	virtual void Destroyed() override;
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Arc Dart")
	float StunDuration = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arc Dart|VFX")
	class UNiagaraSystem* ArcTrailSystem;

	UPROPERTY(EditDefaultsOnly, Category = "Arc Dart|Audio")
	class USoundBase* ZapSound;

	UPROPERTY(EditDefaultsOnly, Category = "Arc Dart|Audio")
	class USoundBase* LaunchSound;
	
	// The sparks that play ON the enemy's body
	UPROPERTY(EditDefaultsOnly, Category = "Arc Dart|VFX")
	class UNiagaraSystem* ElectrocuteVFX;

private:
	UPROPERTY()
	AGASPBaseCharacter* Caster;

	UPROPERTY()
	class UNiagaraComponent* ActiveTrail;
};