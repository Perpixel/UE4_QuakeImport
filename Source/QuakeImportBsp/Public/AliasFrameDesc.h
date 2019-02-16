// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "AliasFrameDesc.generated.h"

USTRUCT(BlueprintType)
struct FAliasFrameDesc : public FTableRowBase
{
    GENERATED_BODY()

        UPROPERTY(EditAnywhere, BlueprintReadOnly)
        FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
        int Type; // 0 single, 1 group

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
        int Start;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
        int NumPoses; // 1 for frames. Multiple for groups.

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
        float Interval; // group only
};