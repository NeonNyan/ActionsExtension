// Copyright 2015-2017 Piperift. All Rights Reserved.

#include "AIExtensionPrivatePCH.h"

#include "AIGeneric.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

#include "Actions/ActionManagerComponent.h"


static ConstructorHelpers::FObjectFinderOptional<UBehaviorTree> GenericBehavior(TEXT("/AIExtension/Base/Individuals/BT_Generic"));

AAIGeneric::AAIGeneric(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    ActionManagerComponent  = ObjectInitializer.CreateDefaultSubobject<UActionManagerComponent>(this, TEXT("Action Manager"));
    
 	BlackboardComp = ObjectInitializer.CreateDefaultSubobject<UBlackboardComponent>(this, TEXT("BlackBoard"));
	BrainComponent = BehaviorComp = ObjectInitializer.CreateDefaultSubobject<UBehaviorTreeComponent>(this, TEXT("Behavior"));	

    BaseBehavior = GenericBehavior.Get();

    State = ECombatState::Passive;
    Faction = FFaction::NoFaction;
}

void AAIGeneric::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
}

void AAIGeneric::Possess(APawn* InPawn)
{
	Super::Possess(InPawn);

	// start behavior
	if (BaseBehavior)
	{
		if (BaseBehavior->BlackboardAsset)
		{
			BlackboardComp->InitializeBlackboard(*BaseBehavior->BlackboardAsset);
            BlackboardComp->SetValueAsObject(TEXT("SelfActor"), InPawn);
		}

		BehaviorComp->StartTree(*BaseBehavior);
        SetDynamicSubBehavior(FAIExtensionModule::FBehaviorTags::Combat,    CombatBehavior);
        SetDynamicSubBehavior(FAIExtensionModule::FBehaviorTags::Alert,     AlertBehavior);
        SetDynamicSubBehavior(FAIExtensionModule::FBehaviorTags::Suspicion, SuspicionBehavior);
        SetDynamicSubBehavior(FAIExtensionModule::FBehaviorTags::Passive,   PassiveBehavior);
	}
}

void AAIGeneric::UnPossess()
{
	Super::UnPossess();

	BehaviorComp->StopTree();
}

void AAIGeneric::BeginInactiveState()
{
	Super::BeginInactiveState();

	AGameStateBase const* const GameState = GetWorld()->GetGameState();

	const float MinRespawnDelay = GameState ? GameState->GetPlayerRespawnDelay(this) : 1.0f;

	GetWorldTimerManager().SetTimer(TimerHandle_Respawn, this, &AAIGeneric::Respawn, MinRespawnDelay);
}


const bool AAIGeneric::AddChildren(UAction* NewChildren)
{
    check(ActionManagerComponent);
    return ActionManagerComponent->AddChildren(NewChildren);
}

const bool AAIGeneric::RemoveChildren(UAction* RemChildren)
{
    check(ActionManagerComponent);
    return ActionManagerComponent->RemoveChildren(RemChildren);
}

UActionManagerComponent* AAIGeneric::GetTaskOwnerComponent()
{
    check(ActionManagerComponent);
    return ActionManagerComponent;
}


void AAIGeneric::Respawn()
{
	GetWorld()->GetAuthGameMode()->RestartPlayer(this);
}

void AAIGeneric::StartCombat(APawn* InTarget)
{
    if (!CanAttack(InTarget))
        return;

    if (SetState(ECombatState::Combat))
    {
        BlackboardComp->SetValueAsObject(TEXT("Target"), InTarget);

        AddPotentialTarget(InTarget);
        OnCombatStarted(InTarget);
    }
}

void AAIGeneric::FinishCombat(ECombatState DestinationState /*= ECombatState::Alert*/)
{
    if (State != ECombatState::Combat || DestinationState == ECombatState::Combat)
        return;

    SetState(DestinationState);
}

void AAIGeneric::GameHasEnded(AActor* EndGameFocus, bool bIsWinner)
{
	// Stop the behavior tree/logic
	BehaviorComp->StopTree();

	// Stop any movement we already have
	StopMovement();

	// Cancel the respawn timer
	GetWorldTimerManager().ClearTimer(TimerHandle_Respawn);

	// Clear any target
    FinishCombat();
}

const bool AAIGeneric::CanAttack(APawn* Target) const
{
    return Target && Target != GetPawn() && (!IsInCombat() || Target != GetTarget()) && ExecCanAttack(Target);
}

bool AAIGeneric::ExecCanAttack_Implementation(APawn* Target) const
{
    return IsHostileTowards(*Target);
}


/***************************************
* Potential Targets                    *
***************************************/

void AAIGeneric::AddPotentialTarget(APawn* Target)
{
    if (CanAttack(Target))
    {
        PotentialTargets.Add(Target);

        //Start combat if this is the first potential target
        if (!IsInCombat() && GetTarget() != Target)
            StartCombat(Target);
    }
}

void AAIGeneric::RemovePotentialTarget(APawn* Target)
{
    if (!Target)
        return;

    PotentialTargets.Remove(Target);

    //Finish combat if the target is the same
    if (IsInCombat() && GetTarget() == Target)
    {
        FinishCombat();
        if (PotentialTargets.Num() > 0)
        {
            StartCombat(PotentialTargets.Array()[0]);
        }
    }
}


/***************************************
* Squads                               *
***************************************/

void AAIGeneric::JoinSquad(AAISquad * SquadToJoin)
{
    if (SquadToJoin != NULL)
    {
        Squad = SquadToJoin;
        Squad->AddMember(this);
    }
}

void AAIGeneric::LeaveSquad()
{
    Squad->RemoveMember(this);
    Squad = NULL;
}

UClass* AAIGeneric::GetSquadOrder() const
{
    if (!IsInSquad() || !Squad->CurrentOrder)
    {
        return NULL;
    }

    return Squad->CurrentOrder->GetClass();
}


/***************************************
* Squads                               *
***************************************/

FFaction AAIGeneric::GetFaction() const
{
    FFaction EventFaction;
    Execute_EventGetFaction(this, EventFaction);
    return EventFaction.IsNone() ? Faction : EventFaction;
}

void AAIGeneric::SetFaction(const FFaction & InFaction)
{
    Faction = InFaction;
    Execute_EventSetFaction(this, InFaction);
}


void AAIGeneric::SetDynamicSubBehavior(FName GameplayTag, UBehaviorTree* SubBehavior)
{
    if (BehaviorComp && SubBehavior)
    {
        BehaviorComp->SetDynamicSubtree(FGameplayTag::RequestGameplayTag(GameplayTag), SubBehavior);
    }
}

const bool AAIGeneric::SetState(ECombatState InState)
{
    //Only allow State change when squad is not in a more important state
    if (!IsInSquad() || Squad->GetState() < InState) {
        State = InState;
        BlackboardComp->SetValueAsEnum(TEXT("CombatState"), (uint8)InState);
        return true;
    }
    return false;
}

const ECombatState AAIGeneric::GetState() const
{
    if (IsInSquad())
    {
        return FMath::Max(Squad->GetState(), State);
    }
    return  State;
}
