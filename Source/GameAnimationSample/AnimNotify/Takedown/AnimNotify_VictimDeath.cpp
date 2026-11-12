#include "AnimNotify_VictimDeath.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"

void UAnimNotify_VictimDeath::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		// Get the Victim (who is playing the animation)
		if (AGASPBaseCharacter* Victim = Cast<AGASPBaseCharacter>(MeshComp->GetOwner()))
		{
			// Tell the victim to kill themselves!
			Victim->ExecuteVictimDeath();
		}
	}
}