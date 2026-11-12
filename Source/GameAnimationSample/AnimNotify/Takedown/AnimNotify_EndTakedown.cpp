#include "AnimNotify_EndTakedown.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"

void UAnimNotify_EndTakedown::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		if (AGASPBaseCharacter* Attacker = Cast<AGASPBaseCharacter>(MeshComp->GetOwner()))
		{
			Attacker->EndTakedownLock();
		}
	}
}