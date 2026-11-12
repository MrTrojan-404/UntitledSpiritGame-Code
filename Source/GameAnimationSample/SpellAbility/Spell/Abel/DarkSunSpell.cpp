#include "DarkSunSpell.h"
#include "DarkSunOrb.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"

void UDarkSunSpell::Execute_Implementation(AGASPBaseCharacter* Caster)
{
	if (!Caster || !Caster->HasAuthority()) return;

	UClass* ClassToSpawn = OrbClass ? OrbClass.Get() : ADarkSunOrb::StaticClass();
	
	// Spawn slightly in front of the caster to prevent immediate self-collision
	FVector SpawnLoc = Caster->GetActorLocation() + (Caster->GetActorForwardVector() * SpawnDistance);
	FTransform SpawnTransform(Caster->GetActorRotation(), SpawnLoc);

	ADarkSunOrb* Orb = Caster->GetWorld()->SpawnActorDeferred<ADarkSunOrb>(ClassToSpawn, SpawnTransform);
	
	if (Orb)
	{
		Orb->Initialize(Caster);
		Orb->FinishSpawning(SpawnTransform);
	}
}