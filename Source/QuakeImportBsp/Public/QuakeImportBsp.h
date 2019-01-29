// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
// Copyright 2018 Guillaume Plourde. All Rights Reserved.
// https://github.com/Perpixel/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FQuakeImportBspModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
