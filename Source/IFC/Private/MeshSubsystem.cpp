// Fill out your copyright notice in the Description page of Project Settings.

#include "MeshSubsystem.h"
#include "HAL/PlatformTime.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"

static uint64 ComputeContentHash(const TArray<FVector3f>& points, const TArray<int32>& indices) {
    TArray<uint8> buffer;
    buffer.Reserve(points.Num() * sizeof(FVector3f) + indices.Num() * sizeof(int32));
    if (points.Num() > 0) buffer.Append(reinterpret_cast<const uint8*>(points.GetData()), points.Num() * sizeof(FVector3f));
    if (indices.Num() > 0) buffer.Append(reinterpret_cast<const uint8*>(indices.GetData()), indices.Num() * sizeof(int32));
    return FXxHash64::HashBuffer(buffer.GetData(), buffer.Num()).Hash;
}

int32 UMeshSubsystem::CreateMesh(UWorld* world, const TArray<FVector3f>& points, const TArray<int32>& indices) {
    if (!world) return INDEX_NONE;
    if (points.Num() == 0) return INDEX_NONE;
    if (indices.Num() == 0 || (indices.Num() % 3) != 0) return INDEX_NONE;

    UStaticMesh* mesh = NewObject<UStaticMesh>(this, NAME_None, RF_Transient);
    if (!mesh) return INDEX_NONE;

    FMeshDescription md;
    FStaticMeshAttributes a(md);
    a.Register();

    TVertexAttributesRef<FVector3f> pos = a.GetVertexPositions();
    TVertexInstanceAttributesRef<FVector4f> vtxColors = a.GetVertexInstanceColors();
    TVertexInstanceAttributesRef<FVector2f> uvs = a.GetVertexInstanceUVs();
    if (uvs.GetNumChannels() < 1) uvs.SetNumChannels(1);
    TVertexInstanceAttributesRef<FVector3f> normals = a.GetVertexInstanceNormals();

    TArray<FVertexID> vids; vids.Reserve(points.Num());
    for (const FVector3f& p : points) { FVertexID v = md.CreateVertex(); pos[v] = p; vids.Add(v); }

    FPolygonGroupID pg = md.CreatePolygonGroup();

    TPolygonGroupAttributesRef<FName> pgSlotNames = a.GetPolygonGroupMaterialSlotNames();
    const FName slotName = TEXT("Slot0");
    pgSlotNames[pg] = slotName;

    auto PlanarUV = [](const FVector3f& p0, const FVector3f& p1, const FVector3f& p2, FVector2f& uv0, FVector2f& uv1, FVector2f& uv2) {
        const FVector3f e0 = p1 - p0;
        const FVector3f e1 = p2 - p0;
        const FVector3f n = FVector3f::CrossProduct(e0, e1);
        const FVector3f an(FMath::Abs(n.X), FMath::Abs(n.Y), FMath::Abs(n.Z));
        if (an.X >= an.Y && an.X >= an.Z) { uv0 = FVector2f(p0.Y, p0.Z); uv1 = FVector2f(p1.Y, p1.Z); uv2 = FVector2f(p2.Y, p2.Z); }
        else if (an.Y >= an.X && an.Y >= an.Z) { uv0 = FVector2f(p0.X, p0.Z); uv1 = FVector2f(p1.X, p1.Z); uv2 = FVector2f(p2.X, p2.Z); }
        else { uv0 = FVector2f(p0.X, p0.Y); uv1 = FVector2f(p1.X, p1.Y); uv2 = FVector2f(p2.X, p2.Y); }
        };

    for (int32 i = 0; i < indices.Num(); i += 3) {
        const int32 i0 = indices[i + 0], i1 = indices[i + 1], i2 = indices[i + 2];
        if (!vids.IsValidIndex(i0) || !vids.IsValidIndex(i1) || !vids.IsValidIndex(i2)) continue;

        const FVector3f p0 = pos[vids[i0]];
        const FVector3f p1 = pos[vids[i1]];
        const FVector3f p2 = pos[vids[i2]];

        FVertexInstanceID vi0 = md.CreateVertexInstance(vids[i0]);
        FVertexInstanceID vi1 = md.CreateVertexInstance(vids[i1]);
        FVertexInstanceID vi2 = md.CreateVertexInstance(vids[i2]);

        vtxColors[vi0] = FVector4f(1, 1, 1, 1);
        vtxColors[vi1] = FVector4f(1, 1, 1, 1);
        vtxColors[vi2] = FVector4f(1, 1, 1, 1);

        FVector2f uv0, uv1, uv2;
        PlanarUV(p0, p1, p2, uv0, uv1, uv2);
        uvs.Set(vi0, 0, uv0);
        uvs.Set(vi1, 0, uv1);
        uvs.Set(vi2, 0, uv2);

        const FVector3f e0 = p1 - p0;
        const FVector3f e1 = p2 - p0;
        FVector3f faceN = FVector3f::CrossProduct(e0, e1);
        if (!faceN.Normalize()) faceN = FVector3f(0, 0, 1);
        normals[vi0] = faceN;
        normals[vi1] = faceN;
        normals[vi2] = faceN;

        TArray<FVertexInstanceID> tri; tri.Add(vi0); tri.Add(vi1); tri.Add(vi2);
        md.CreatePolygon(pg, tri);
    }

    FStaticMeshOperations::ComputeTriangleTangentsAndNormals(md);
    FStaticMeshOperations::ComputeTangentsAndNormals(md, EComputeNTBsFlags::Tangents | EComputeNTBsFlags::UseMikkTSpace);

    if (mesh->GetStaticMaterials().Num() == 0) mesh->GetStaticMaterials().Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface), TEXT("Slot0")));

    UStaticMesh::FBuildMeshDescriptionsParams params;
    params.bAllowCpuAccess = true;
    params.bBuildSimpleCollision = false;
    params.bCommitMeshDescription = false;
    params.bFastBuild = true;
    
    if (mesh->GetStaticMaterials().Num() == 0) {
        mesh->GetStaticMaterials().Reset();
        mesh->GetStaticMaterials().Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface), slotName));
    }
    
    TArray<const FMeshDescription*> mds; mds.Add(&md);
    if (!mesh->BuildFromMeshDescriptions(mds, params)) return INDEX_NONE;

    mesh->InitResources();
    mesh->CalculateExtendedBounds();

    uint64 h = ComputeContentHash(points, indices);
    return RegisterMesh(mesh, h);
}

int32 UMeshSubsystem::RegisterMesh(UStaticMesh* mesh, uint64 contentHash) {
    if (!mesh) return INDEX_NONE;
    int32 existingId = INDEX_NONE;
    if (contentHash != 0) {
        const int32* foundId = HashToId.Find(contentHash);
        if (foundId) existingId = *foundId;
    }
    if (existingId != INDEX_NONE) {
        MeshEntryData* entry = EntryData.Find(existingId);
        if (!entry) return INDEX_NONE;
        ++entry->RefCount;
        entry->LastAccess = FPlatformTime::Seconds();
        return existingId;
    }
    int32 newId = NextId++;
    Meshes.Add(newId, mesh);
    MeshEntryData& newEntry = EntryData.Add(newId);
    newEntry.RefCount = 1;
    newEntry.ContentHash = contentHash;
    newEntry.LastAccess = FPlatformTime::Seconds();
    if (contentHash != 0) HashToId.Add(contentHash, newId);
    return newId;
}

bool UMeshSubsystem::TryFindByHash(uint64 contentHash, int32& outId) const {
    if (contentHash == 0) return false;
    const int32* foundId = HashToId.Find(contentHash);
    if (!foundId) return false;
    outId = *foundId;
    return true;
}

void UMeshSubsystem::Retain(int32 id) {
    MeshEntryData* entry = EntryData.Find(id);
    if (!entry) return;
    ++entry->RefCount;
    entry->LastAccess = FPlatformTime::Seconds();
}

void UMeshSubsystem::Release(int32 id, bool destroyNow) {
    MeshEntryData* entry = EntryData.Find(id);
    if (!entry) return;
    --entry->RefCount;
    if (entry->RefCount > 0) {
        entry->LastAccess = FPlatformTime::Seconds();
        return;
    }
    uint64 hash = entry->ContentHash;
    EntryData.Remove(id);
    UStaticMesh* mesh = nullptr;
    if (const TObjectPtr<UStaticMesh>* meshPtr = Meshes.Find(id)) mesh = meshPtr->Get();
    Meshes.Remove(id);
    if (hash != 0) HashToId.Remove(hash);
    if (destroyNow && mesh) mesh->MarkAsGarbage();
}

UStaticMesh* UMeshSubsystem::Get(int32 id) const {
    const TObjectPtr<UStaticMesh>* meshPtr = Meshes.Find(id);
    if (!meshPtr) return nullptr;
    return meshPtr->Get();
}

void UMeshSubsystem::Touch(int32 id) {
    MeshEntryData* entry = EntryData.Find(id);
    if (!entry) return;
    entry->LastAccess = FPlatformTime::Seconds();
}

MeshStats UMeshSubsystem::GetStats() const {
    MeshStats stats;
    stats.Count = EntryData.Num();
    int32 totalRefCount = 0;
    for (const auto& entryPair : EntryData) totalRefCount += entryPair.Value.RefCount;
    stats.TotalRefCount = totalRefCount;
    return stats;
}