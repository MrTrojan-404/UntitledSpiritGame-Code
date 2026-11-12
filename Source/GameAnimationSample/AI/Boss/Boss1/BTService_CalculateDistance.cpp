#include "BTService_CalculateDistance.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

UBTService_CalculateDistance::UBTService_CalculateDistance()
{
	NodeName = TEXT("Calculate Distance To Target");
	bNotifyTick = true;
}

void UBTService_CalculateDistance::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* AICon = OwnerComp.GetAIOwner();
	if (!BB || !AICon) return;

	APawn* AIPawn = AICon->GetPawn();
	AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));

	if (AIPawn && TargetActor)
	{
		// Calculate the exact 3D distance between the Boss and the Player
		float Distance = FVector::Dist(AIPawn->GetActorLocation(), TargetActor->GetActorLocation());
		
		// Push it to the Blackboard
		BB->SetValueAsFloat(DistanceKey.SelectedKeyName, Distance);
	}
}