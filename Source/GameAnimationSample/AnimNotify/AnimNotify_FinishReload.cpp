// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify_FinishReload.h"

#include "GameAnimationSample/Interface/CombatInterface.h"

void UAnimNotify_FinishReload::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();

	if (ICombatInterface* Combat = Cast<ICombatInterface>(Owner))
	{
		ICombatInterface::Execute_OnFinishReload(Owner);
	}
	
}
