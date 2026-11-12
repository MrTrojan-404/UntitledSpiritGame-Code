#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_TakedownFX.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UAnimNotify_TakedownFX : public UAnimNotify
{
	GENERATED_BODY()

public:
	// These show up when you click the Notify inside the Montage
	UPROPERTY(EditAnywhere, Category = "Feedback")
	TSubclassOf<UCameraShakeBase> CameraShake;

	UPROPERTY(EditAnywhere, Category = "Feedback")
	class UNiagaraSystem* BloodVFX;
	
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};