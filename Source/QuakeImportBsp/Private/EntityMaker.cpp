// Fill out your copyright notice in the Description page of Project Settings.

#include "EntityMaker.h"
#include "Containers/UnrealString.h"
#include "Containers/StringConv.h"
#include "Engine/Light.h"
#include "Engine/World.h"

#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Editor/EditorEngine.h"
#include "Editor.h"
#include "UObject/UObjectGlobals.h"
#include "FileHelpers.h"
#include "Components/PointLightComponent.h"

/*
=======================================
Attribute

=======================================
*/

Attribute::Attribute() :
    name_(),
    value_()
{
    /* do nothing */
}

Attribute::Attribute(FString name, FString value) :
    name_(name),
    value_(value)
{
    Tokenize();
}

float Attribute::ToFloat() const
{
    if (!tokens_.Num())
    {
        return 0.0f;
    }

    return FCString::Atof(*tokens_[0]);
}

int Attribute::ToInteger() const
{
    if (!tokens_.Num())
    {
        return 0;
    }

    return FCString::Atoi(*tokens_[0]);
}

FString Attribute::ToString() const
{
    return value_;
}

FVector Attribute::ToVector3f() const
{
    if (tokens_.Num() < 3)
    {
        return FVector(0, 0, 0);
    }

    FVector vec;

    for (int i = 0; i < 3; i++)
    {
        vec[i] = FCString::Atof(*tokens_[i]);
    }

    vec.X = -vec.X;

    return vec;
}

void Attribute::Tokenize()
{
    FString chunk;

    for (const auto& c : value_)
    {
        if (c == ' ')
        {
            if (chunk.Len() > 0)
            {
                tokens_.Add(chunk);
            }

            chunk = "";
        }
        else
        {
            chunk += c;
        }
    }

    if (chunk.Len() > 0)
    {
        tokens_.Add(chunk);
    }
}

/*
=======================================
AttributeGroup

=======================================
*/

bool AttributeGroup::Set(FString name, FString value)
{
    if (mask_.Find(name))
    {
        mask_[name] = true;
        attributes_.Add(name, Attribute(name, value));
        return true;
    }

    return false; // exist already // TODO assert
}

const Attribute* AttributeGroup::Get(FString name) const
{
    return attributes_.Find(name);
}

void DeserializeBlock(const FString& in, AttributeGroup& attributes)
{
    TArray<FString> tokens;

    FString input = in;
    FString left;
    FString right;

    while (input.Split("\"", &left, &right, ESearchCase::IgnoreCase, ESearchDir::FromStart))
    {
        right.Split("\"", &left, &right, ESearchCase::IgnoreCase, ESearchDir::FromStart);
        tokens.Add(left);
        input = right; // process leftover
    }

    // make the pairs
    for (auto it = tokens.CreateConstIterator(); it; ++it)
    {
        FString name = *it;
        FString value = *(++it);
        attributes.Set(name, value);
    }
}

void DeserializeGroup(const FString& in, TArray<AttributeGroup>& attributeGroup)
{
    FString input = in;
    FString left;
    FString right;

    while (input.Split("{", &left, &right, ESearchCase::IgnoreCase, ESearchDir::FromStart))
    {
        right.Split("}", &left, &right, ESearchCase::IgnoreCase, ESearchDir::FromStart);
        AttributeGroup entityDesc;
        DeserializeBlock(left, entityDesc);
        attributeGroup.Add(entityDesc);
        input = right; // process leftover
    }
}

void EntityMaker(UWorld& world, const TArray<AttributeGroup>& entities)
{
    for (const auto& it : entities)
    {
        FString classname;

        if (it.Get("classname") == nullptr)
        {
            // TODO log error
            continue;
        }

        classname = it.Get("classname")->ToString();

        float   angle = 0;
        FVector origin(0, 0, 0);

        if (it.Get("origin"))
        {
            origin = it.Get("origin")->ToVector3f();
        }

        if (it.Get("angle"))
        {
            angle = it.Get("angle")->ToFloat();
        }

        if (classname == "light")
        {
            ULevel* level = world.GetCurrentLevel();
            APointLight* PointLight = Cast<APointLight>(GEditor->AddActor(world.GetCurrentLevel(), APointLight::StaticClass(), FTransform(origin)));

            UPointLightComponent* pointlightComponent = PointLight->FindComponentByClass<UPointLightComponent>();
            pointlightComponent->SetMobility(EComponentMobility::Static);

            if (it.Get("light"))
            {
                pointlightComponent->Intensity = it.Get("light")->ToInteger() * 10 * 2;
            }

            GEditor->EditorUpdateComponents();
            world.UpdateWorldComponents(true, false);
        }
    }
}
