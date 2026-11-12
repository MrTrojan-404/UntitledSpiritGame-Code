#include "AnimNotify_MeleeHit.h"
#include "GameAnimationSample/Interface/CombatInterface.h"

void UAnimNotify_MeleeHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();
    
	// Only the server needs to calculate the hit detection
	if (!Owner || !Owner->HasAuthority()) return;

	if (ICombatInterface* Combat = Cast<ICombatInterface>(Owner))
	{
		ICombatInterface::Execute_OnMeleeHit(Owner);
	}
}