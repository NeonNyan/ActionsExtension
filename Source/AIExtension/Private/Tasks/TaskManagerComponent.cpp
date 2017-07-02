// Copyright 2015-2017 Piperift. All Rights Reserved.

#include "AIExtensionPrivatePCH.h"
#include "TaskManagerComponent.h"


// Sets default values for this component's properties
UTaskManagerComponent::UTaskManagerComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = true;

    // ...
}


// Called when the game starts
void UTaskManagerComponent::BeginPlay()
{
    Super::BeginPlay();

    // ...
    
}


void UTaskManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    for (auto* Children : ChildrenTasks)
    {
        if(Children) {
            //Cancel task
            Children->Cancel();
        }
    }
}

// Called every frame
void UTaskManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // ...
}

const bool UTaskManagerComponent::AddChildren(UTask* NewChildren)
{
    return ChildrenTasks.AddUnique(NewChildren) != INDEX_NONE;
}

const bool UTaskManagerComponent::RemoveChildren(UTask* Children)
{
    return ChildrenTasks.Remove(Children) > 0;
}

UTaskManagerComponent* UTaskManagerComponent::GetTaskOwnerComponent()
{
    return this;
}

