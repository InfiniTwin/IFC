#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ISMSubsystem.generated.h"

UCLASS()
class UISMSubsystem : public UWorldSubsystem {
    GENERATED_BODY()

public:

    IFC_API void SetISMCustomData(uint64 handle, int32 customIndex, float value);

    static uint64 MakeIsmHandle(int32 meshId, int32 instanceIndex);
    static void SplitIsmHandle(uint64 handle, int32& outMeshId, int32& outInstanceIndex);

    uint64 CreateISM(UWorld* world, int32 meshId, int32 materialId, const FVector& position, const FRotator& rotation, const FVector& scale);
    bool UpdateISMTransform(uint64 handle, const FTransform& transform, bool worldSpace = true, bool markRenderStateDirty = true, bool teleport = true);
    bool SetISMNumCustomDataFloats(int32 meshId, int32 numFloats);
    bool RemoveISM(UWorld* world, uint64 handle, uint64& outMovedOldHandle);
    int32 GetISMInstanceCount(int32 meshId) const;
    void DestroyGroup(UWorld* world, int32 meshId);
    void DestroyAll(UWorld* world);

private:
    AActor* EnsureRoot(UWorld* world);
    UInstancedStaticMeshComponent* GetOrCreateIsm(UWorld* world, int32 meshId, int32 materialId);

    UPROPERTY() TObjectPtr<AActor> Root;
    UPROPERTY() TMap<int32, TObjectPtr<UInstancedStaticMeshComponent>> ByMeshId;
};
