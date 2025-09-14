#include "ISMSubsystem.h"
#include "MaterialSubsystem.h"
#include "MeshSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Logging/LogMacros.h"

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
	params.Name = NAME_None;
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

UInstancedStaticMeshComponent* UISMSubsystem::GetOrCreateIsm(UWorld* world, int32 meshId, int32 materialId) {
	if (TObjectPtr<UInstancedStaticMeshComponent>* found = ByMeshId.Find(meshId)) return found->Get();
	UMeshSubsystem* meshSubsystem = world->GetSubsystem<UMeshSubsystem>();
	UStaticMesh* mesh = meshSubsystem->Get(meshId);
	AActor* owner = EnsureRoot(world);

	UInstancedStaticMeshComponent* ism = NewObject<UInstancedStaticMeshComponent>(owner);
	if (!ism) return nullptr;
	ism->SetStaticMesh(mesh);
	ism->SetupAttachment(owner->GetRootComponent());
	ism->SetMobility(EComponentMobility::Movable);
	ism->RegisterComponent();
	ism->SetVisibility(true, true);

	UMaterialSubsystem* materialSubsystem = world->GetSubsystem<UMaterialSubsystem>();
	UMaterialInterface* material = materialSubsystem->Get(materialId);
	materialSubsystem->Retain(materialId);
	ism->SetMaterial(0, material);
	ism->MarkRenderStateDirty();

	ByMeshId.Add(meshId, ism);
	meshSubsystem->Retain(meshId);
	return ism;
}

uint64 UISMSubsystem::CreateISM(UWorld* world, int32 meshId, int32 materialId, const FVector& position, const FRotator& rotation, const FVector& scale) {
	UInstancedStaticMeshComponent* ism = GetOrCreateIsm(world, meshId, materialId);
	if (!ism) return 0;
	FTransform transform(rotation, position, scale);
	int32 instanceIndex = ism->AddInstance(transform, true);
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

void UISMSubsystem::SetISMCustomData(uint64 handle, int32 customIndex, float value) {
	int32 meshId = -1, instanceIndex = -1;
	SplitIsmHandle(handle, meshId, instanceIndex);

	UInstancedStaticMeshComponent* component = nullptr;
	if (TObjectPtr<UInstancedStaticMeshComponent>* found = ByMeshId.Find(meshId))
		component = found->Get();
	if (!component) return;

	const int32 count = component->GetInstanceCount();
	if (instanceIndex < 0 || instanceIndex >= count) return;

	if (component->NumCustomDataFloats <= customIndex) {
		component->SetNumCustomDataFloats(customIndex + 1);
		component->MarkRenderStateDirty();
	}

	component->SetCustomDataValue(instanceIndex, customIndex, value, true);
}


bool UISMSubsystem::SetISMNumCustomDataFloats(int32 meshId, int32 numFloats) {
	UInstancedStaticMeshComponent* ism = nullptr;
	if (TObjectPtr<UInstancedStaticMeshComponent>* found = ByMeshId.Find(meshId)) ism = found->Get();
	if (!ism) return false;
	if (numFloats <= 0) return false;
	ism->SetNumCustomDataFloats(numFloats);
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
	if (Root) { 
		Root->Destroy(); 
		Root = nullptr;
	}
}
