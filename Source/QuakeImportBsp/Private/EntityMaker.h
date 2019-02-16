// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/*
=======================================
Attribute

=======================================
*/

class Attribute
{
public:
    Attribute();
    Attribute(FString name, FString value);

    float   ToFloat() const;
    int     ToInteger() const;
    FString ToString() const;
    FVector ToVector3f() const;

private:
    void Tokenize();

    FString name_;
    TArray<FString> tokens_;
    FString value_;
};

/*
=======================================
AttributeGroup

=======================================
*/

class AttributeGroup
{
public:
    AttributeGroup() :
        attributes_()
    {
        mask_.Add(TEXT("angle"), false);
        mask_.Add(TEXT("classname"), false);
        mask_.Add(TEXT("light"), false);
        mask_.Add(TEXT("message"), false);
        mask_.Add(TEXT("model"), false);
        mask_.Add(TEXT("origin"), false);
        mask_.Add(TEXT("spawnflags"), false);
        mask_.Add(TEXT("sound"), false);
        mask_.Add(TEXT("speed"), false);
        mask_.Add(TEXT("wait"), false);
    }

    bool Set(FString name, FString value);
    const Attribute *Get(FString name) const;

private:
    TMap<FString, bool> mask_;
    TMap<FString, Attribute> attributes_;
};

/*
=======================================
Map entity description Deserializer

=======================================
*/

void DeserializeBlock(const FString& in, AttributeGroup& attributes);
void DeserializeGroup(const FString& in, TArray<AttributeGroup>& attributeGroup);
void EntityMaker(UWorld& world, const TArray<AttributeGroup>& entities);

