#include "ISMSubsystem.h"
#include "MeshSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/Material.h"

uint64 UISMSubsystem::MakeIsmHandle(int32 meshId, int32 instanceIndex) {
    return (uint64(uint32(meshId)) << 32) | uint64(uint32(instanceIndex));
}

void UISMSubsystem::SplitIsmHandle(uint64 handle, int32& outMeshId, int32& outInstanceIndex) {
    outMeshId = int32(uint32(handle >> 32));
    outInstanceIndex = int32(uint32(handle));
}

AActor* UISMSubsystem::EnsureRoot(UWorld* world) {
    if (Root && Root->GetWorld() == world) return Root.Get();
    FActorSpawnParameters params;
    params.Name = FName(TEXT("ISMSubsystem_Root"));
    params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* actor = world->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, params);
    if (!actor) return nullptr;
    actor->SetActorHiddenInGame(false);
    actor->SetActorEnableCollision(false);
    if (!actor->GetRootComponent()) {
        USceneComponent* root = NewObject<USceneComponent>(actor);
        actor->SetRootComponent(root);
        root->SetMobility(EComponentMobility::Movable);
        root->RegisterComponent();
    }
    Root = actor;
    return actor;
}

UInstancedStaticMeshComponent* UISMSubsystem::GetOrCreateIsm(UWorld* world, int32 meshId) {
    UInstancedStaticMeshComponent* ism = nullptr;
    if (TObjectPtr<UInstancedStaticMeshComponent>* found = ByMeshId.Find(meshId)) ism = found->Get();
    UMeshSubsystem* meshSub = world->GetSubsystem<UMeshSubsystem>();
    if (!meshSub) return nullptr;
    UStaticMesh* mesh = meshSub->Get(meshId);
    if (!mesh) return nullptr;
    AActor* owner = EnsureRoot(world);
    if (!owner) return nullptr;

    UInstancedStaticMeshComponent* comp = NewObject<UInstancedStaticMeshComponent>(owner);
    if (!comp) return nullptr;
    comp->SetStaticMesh(mesh);
    comp->SetupAttachment(owner->GetRootComponent());
    comp->SetMobility(EComponentMobility::Movable);
    comp->RegisterComponent();
    comp->SetVisibility(true, true);

    if (comp->GetNumMaterials() == 0) 
        comp->SetMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));

    ByMeshId.Add(meshId, comp);
    meshSub->Retain(meshId);
    return comp;
}

uint64 UISMSubsystem::CreateISM(UWorld* world, int32 meshId, const FVector& position, const FRotator& rotation, const FVector& scale) {
    UInstancedStaticMeshComponent* ism = GetOrCreateIsm(world, meshId);
    if (!ism) return 0;
    FTransform transform(rotation, position, scale);
    int32 instanceIndex = ism->AddInstanceWorldSpace(transform);
    if (instanceIndex < 0) return 0;
    return MakeIsmHandle(meshId, instanceIndex);
}

bool UISMSubsystem::UpdateISMTransform(uint64 handle, const FTransform& transform, bool worldSpace, bool markRenderStateDirty, bool teleport) {
    int32 meshId, instanceIndex;
    SplitIsmHandle(handle, meshId, instanceIndex);
    UInstancedStaticMeshComponent* ism = nullptr;
    if (TObjectPtr<UInstancedStaticMeshComponent>* found = ByMeshId.Find(meshId)) ism = found->Get();
    if (!ism) return false;
    if (instanceIndex < 0 || instanceIndex >= ism->GetInstanceCount()) return false;
    return ism->UpdateInstanceTransform(instanceIndex, transform, worldSpace, markRenderStateDirty, teleport);
}

bool UISMSubsystem::SetISMCustomData(uint64 handle, int32 customIndex, float value, bool markRenderStateDirty) {
    int32 meshId, instanceIndex;
    SplitIsmHandle(handle, meshId, instanceIndex);
    UInstancedStaticMeshComponent* ism = nullptr;
    if (TObjectPtr<UInstancedStaticMeshComponent>* found = ByMeshId.Find(meshId)) ism = found->Get();
    if (!ism) return false;
    if (customIndex < 0 || customIndex >= ism->NumCustomDataFloats) return false;
    if (instanceIndex < 0 || instanceIndex >= ism->GetInstanceCount()) return false;
    return ism->SetCustomDataValue(instanceIndex, customIndex, value, markRenderStateDirty);
}

bool UISMSubsystem::SetISMNumCustomDataFloats(int32 meshId, int32 numFloats) {
    UInstancedStaticMeshComponent* ism = nullptr;
    if (TObjectPtr<UInstancedStaticMeshComponent>* found = ByMeshId.Find(meshId)) ism = found->Get();
    if (!ism) return false;
    if (numFloats <= 0) return false;
    ism->SetNumCustomDataFloats(numFloats);
    return true;
}

bool UISMSubsystem::SetISMMaterial(int32 meshId, int32 materialIndex, UMaterialInterface* material) {
    UInstancedStaticMeshComponent* ism = nullptr;
    if (TObjectPtr<UInstancedStaticMeshComponent>* found = ByMeshId.Find(meshId)) ism = found->Get();
    if (!ism) return false;
    if (!material) return false;
    ism->SetMaterial(materialIndex, material);
    return true;
}

bool UISMSubsystem::RemoveISM(UWorld* world, uint64 handle, uint64& outMovedOldHandle) {
    outMovedOldHandle = 0;
    int32 meshId, index;
    SplitIsmHandle(handle, meshId, index);
    UInstancedStaticMeshComponent* ism = nullptr;
    if (TObjectPtr<UInstancedStaticMeshComponent>* found = ByMeshId.Find(meshId)) ism = found->Get();
    if (!ism) return false;
    int32 lastIndex = ism->GetInstanceCount() - 1;
    if (index < 0 || index > lastIndex) return false;
    bool ok = ism->RemoveInstance(index);
    if (!ok) return false;
    if (index != lastIndex) outMovedOldHandle = MakeIsmHandle(meshId, lastIndex);
    if (ism->GetInstanceCount() == 0) {
        ByMeshId.Remove(meshId);
        if (UMeshSubsystem* meshSub = world->GetSubsystem<UMeshSubsystem>()) meshSub->Release(meshId, false);
        ism->DestroyComponent();
    }
    return true;
}

int32 UISMSubsystem::GetISMInstanceCount(int32 meshId) const {
    const UInstancedStaticMeshComponent* ism = nullptr;
    if (const TObjectPtr<UInstancedStaticMeshComponent>* found = ByMeshId.Find(meshId)) ism = found->Get();
    if (!ism) return 0;
    return ism->GetInstanceCount();
}

void UISMSubsystem::DestroyGroup(UWorld* world, int32 meshId) {
    UInstancedStaticMeshComponent* ism = nullptr;
    if (TObjectPtr<UInstancedStaticMeshComponent>* found = ByMeshId.Find(meshId)) ism = found->Get();
    if (!ism) return;
    ByMeshId.Remove(meshId);
    if (UMeshSubsystem* meshSub = world->GetSubsystem<UMeshSubsystem>()) meshSub->Release(meshId, false);
    ism->DestroyComponent();
}

void UISMSubsystem::DestroyAll(UWorld* world) {
    TArray<int32> keys;
    ByMeshId.GetKeys(keys);
    for (int32 meshId : keys) DestroyGroup(world, meshId);
    if (Root) { Root->Destroy(); Root = nullptr; }
}
