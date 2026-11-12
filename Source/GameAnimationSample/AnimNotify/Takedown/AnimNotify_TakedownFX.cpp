#include "AnimNotify_TakedownFX.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"

void UAnimNotify_TakedownFX::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		if (AGASPBaseCharacter* Attacker = Cast<AGASPBaseCharacter>(MeshComp->GetOwner()))
		{
			// Call the new cosmetic-only function!
			Attacker->ExecuteTakedownFX();
		}
	}
}