// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify_ThrowGrenadeFinished.h"

#include "GameAnimationSample/Interface/CombatInterface.h"

void UAnimNotify_ThrowGrenadeFinished::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner || !Owner->HasAuthority()) return;

	if (ICombatInterface* Combat = Cast<ICombatInterface>(Owner))
	{
		ICombatInterface::Execute_OnThrowGrenadeFinished(Owner);
	}
	
}
