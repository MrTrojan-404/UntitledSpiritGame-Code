#include "AnimNotify_MeleeFinished.h"
#include "GameAnimationSample/Interface/CombatInterface.h"

void UAnimNotify_MeleeFinished::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	if (ICombatInterface* Combat = Cast<ICombatInterface>(Owner))
	{
		ICombatInterface::Execute_OnMeleeFinished(Owner);
	}
}