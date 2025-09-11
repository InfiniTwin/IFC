// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MaterialSubsystem.generated.h"

struct MaterialEntryData {
    int32 RefCount = 0;
    uint64 ContentHash = 0;
    double LastAccess = 0.0;
};

UCLASS()
class UMaterialSubsystem : public UWorldSubsystem {
    GENERATED_BODY()

    virtual void Initialize(FSubsystemCollectionBase& Collection) override {
        Super::Initialize(Collection);
        
        MOpaque = LoadObject<UMaterialInterface>(nullptr, OpaquePath);
        MTranslucent = LoadObject<UMaterialInterface>(nullptr, TranslucentPath);
    }

public:
    int32 CreateMaterial(UWorld* world, const FVector4f& rgba);
    void Retain(int32 id);
    void Release(int32 id);
    UMaterialInstanceDynamic* Get(int32 id) const;

private:
    static uint64 MakeHash(UMaterialInterface* master, const FVector4f& rgba, bool opaque);
    int32 Register(UMaterialInstanceDynamic* mid, uint64 contentHash);

    const TCHAR* OpaquePath = TEXT("/Game/Materials/Opaque.Opaque");
    const TCHAR* TranslucentPath = TEXT("/Game/Materials/Translucent.Translucent");
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> MOpaque = nullptr;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> MTranslucent = nullptr;
    UPROPERTY(Transient) TMap<int32, TObjectPtr<UMaterialInstanceDynamic>> Materials;

    TMap<int32, MaterialEntryData> EntryData;
    TMap<uint64, int32> HashToId;
    int32 NextId = 1;
};
