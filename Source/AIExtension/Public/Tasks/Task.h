// Copyright 2015-2017 Piperift. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptInterface.h"
#include "TaskOwnerInterface.h"

#include "Task.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(TaskLog, Log, All);

/**
 * Result of a node execution
 */
UENUM()
enum class ETaskState : uint8
{
    RUNNING  UMETA(DisplayName = "Running"),
    SUCCESS  UMETA(DisplayName = "Success"),
    FAILURE  UMETA(DisplayName = "Failure"),
    ABORTED  UMETA(DisplayName = "Abort"),
    CANCELED UMETA(DisplayName = "Canceled"),
    NOT_RUN  UMETA(DisplayName = "Not Run")
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTaskActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTaskFinished, const ETaskState, Reason);

class UTaskManagerComponent;


/**
 * 
 */
UCLASS(Blueprintable, meta = (ExposedAsyncProxy))
class AIEXTENSION_API UTask : public UObject, public FTickableGameObject, public ITaskOwnerInterface
{
    GENERATED_UCLASS_BODY()

public:

    UFUNCTION(BlueprintCallable, Category = Task)
    void Activate();

    virtual const bool AddChildren(UTask* NewChildren) override;
    virtual const bool RemoveChildren(UTask* Children) override;


    /** Event when tick is received for this tickable object . */
    UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Tick"))
    void ReceiveTick(float DeltaTime);

    UFUNCTION(BlueprintCallable, Category = Task)
    void Finish(bool bSuccess);

    /** Called when any error occurs */
    UFUNCTION(BlueprintCallable, Category = Task)
    void Abort();

    /** Called when the task needs to be stopped from running */
    UFUNCTION(BlueprintCallable, Category = Task)
    void Cancel();

    void Destroy();


    virtual UTaskManagerComponent* GetTaskOwnerComponent() override;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = Task, meta = (DisplayName = "GetTaskOwnerComponent"))
    UTaskManagerComponent* ExposedGetTaskOwnerComponent();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = Task)
    AActor* GetTaskOwnerActor();


    //~ Begin Ticking
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Object)
    uint8 bWantsToTick:1;

    //Tick length in seconds. 0 is default tick rate
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Object)
    float TickRate;

private:

    float TickTimeElapsed;
    //~ End Ticking


protected:

    UPROPERTY()
    ETaskState State;

    UPROPERTY()
    TArray<UTask*> ChildrenTasks;

    //~ Begin Tickable Object Interface
    virtual void Tick(float DeltaTime) override;
    virtual void TaskTick(float DeltaTime) {}

    virtual bool IsTickable() const override {
        return !IsDefaultSubobject() && bWantsToTick && IsActivated() && !GetParent()->IsPendingKill();
    }

    virtual TStatId GetStatId() const override {
        RETURN_QUICK_DECLARE_CYCLE_STAT(UTask, STATGROUP_Tickables);
    }
    //~ End Tickable Object Interface

public:

    inline virtual void OnActivation() {
        OnTaskActivation.Broadcast();
        ReceiveActivate();
    }

    virtual void OnFinish(const ETaskState Reason);

    /** Event when play begins for this actor. */
    UFUNCTION(BlueprintNativeEvent, meta = (DisplayName = "Activate"))
    void ReceiveActivate();

    /** Event when finishing this task. */
    UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Finished"))
    void ReceiveFinished(const ETaskState Reason);

    // DELEGATES
    UPROPERTY()
    FTaskActivated OnTaskActivation;

    UPROPERTY()
    FTaskFinished OnTaskFinished;



    // INLINES

    const bool IsValid() const {
        UObject const * const Outer = GetOuter();
        return !IsPendingKill() && Outer->GetClass()->ImplementsInterface(UTaskOwnerInterface::StaticClass());
    }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = Task)
    FORCEINLINE bool IsActivated() const {
        return IsValid() && State == ETaskState::RUNNING;
    }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = Task)
    FORCEINLINE bool Succeeded() const { return IsValid() && State == ETaskState::SUCCESS; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = Task)
    FORCEINLINE bool Failed() const { return IsValid() && State == ETaskState::FAILURE; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = Task)
    FORCEINLINE bool IsCanceled() const { return State == ETaskState::CANCELED; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = Task)
    FORCEINLINE ETaskState GetState() const { return State; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = Task)
    FORCEINLINE UObject* const GetParent() const {
        return IsValid() ? GetOuter() : nullptr;
    }

    FORCEINLINE ITaskOwnerInterface* GetParentInterface() const {
        return Cast<ITaskOwnerInterface>(GetOuter());
    }


    virtual UWorld* GetWorld() const override {
        const UObject* const InParent = GetParent();

        return InParent ? InParent->GetWorld() : nullptr;
    }

    static FORCEINLINE FString StateToString(ETaskState Value) {
        const UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, TEXT("ETaskState"), true);
        if (!EnumPtr) return FString("Invalid");

        return EnumPtr->GetNameByValue((int64)Value).ToString();
    }
};
