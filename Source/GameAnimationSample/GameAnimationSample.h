// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#define ECC_SkeletalMesh ECollisionChannel::ECC_GameTraceChannel2
#define ECC_HitBox ECollisionChannel::ECC_GameTraceChannel3
static constexpr float INTERACTION_DISTANCE = 400.f;
DECLARE_LOG_CATEGORY_EXTERN(LogAI, Log, All);
