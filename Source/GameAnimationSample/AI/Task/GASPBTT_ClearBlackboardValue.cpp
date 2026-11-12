#include "GASPBTT_ClearBlackboardValue.h"
#include "BehaviorTree/BlackboardComponent.h"

UGASPBTT_ClearBlackboardValue::UGASPBTT_ClearBlackboardValue()
{
	// This is the name that will appear in your Behavior Tree dropdown!
	NodeName = TEXT("Clear Blackboard Value"); 
}

EBTNodeResult::Type UGASPBTT_ClearBlackboardValue::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	
	if (BlackboardComp)
	{
		// BlackboardKey is a built-in variable from UBTTask_BlackboardBase
		BlackboardComp->ClearValue(BlackboardKey.SelectedKeyName);
		
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}