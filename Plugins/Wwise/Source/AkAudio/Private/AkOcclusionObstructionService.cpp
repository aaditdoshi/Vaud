// Copyright (c) 2006-2017 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
AkOcclusionObstructionService.cpp:
=============================================================================*/

#include "AkAudioDevice.h"
#include "AkOcclusionObstructionService.h"
#include "AkComponent.h"
#include "AkAcousticPortal.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"

#define AK_DEBUG_OCCLUSION_PRINT 0
#if AK_DEBUG_OCCLUSION_PRINT
static int framecounter = 0;
#endif


FAkListenerOcclusionObstruction::FAkListenerOcclusionObstruction(float in_TargetValue, float in_CurrentValue)
	: CurrentValue(in_CurrentValue)
	, TargetValue(in_TargetValue)
	, Rate(0.0f)
{}

void FAkListenerOcclusionObstruction::SetTarget(float in_TargetValue)
{
	TargetValue = FMath::Clamp(in_TargetValue, 0.0f, 1.0f);

	const float UAkComponent_OCCLUSION_FADE_RATE = 2.0f; // from 0.0 to 1.0 in 0.5 seconds
	Rate = FMath::Sign(TargetValue - CurrentValue) * UAkComponent_OCCLUSION_FADE_RATE;
}

bool FAkListenerOcclusionObstruction::Update(float DeltaTime)
{
	auto OldValue = CurrentValue;
	if (OldValue != TargetValue)
	{
		const auto NewValue = OldValue + Rate * DeltaTime;
		if (OldValue > TargetValue)
			CurrentValue = FMath::Clamp(NewValue, TargetValue, OldValue);
		else
			CurrentValue = FMath::Clamp(NewValue, OldValue, TargetValue);

		AKASSERT(CurrentValue >= 0.f && CurrentValue <= 1.f);
		return true;
	}

	return false;
}

bool FAkListenerOcclusionObstruction::ReachedTarget()
{
	return CurrentValue == TargetValue;
}

bool FAkListenerOcclusionObstructionPair::Update(float DeltaTime)
{
	const bool bObsChanged = Obs.Update(DeltaTime);
	const bool bOccChanged = Occ.Update(DeltaTime);
	return bObsChanged || bOccChanged;
}

bool FAkListenerOcclusionObstructionPair::ReachedTarget()
{
	return Obs.ReachedTarget() && Occ.ReachedTarget();
}


void AkOcclusionObstructionService::_Init(UWorld* in_world, float in_refreshInterval)
{
	if (in_refreshInterval > 0 && in_world != nullptr)
		LastObstructionOcclusionRefresh = in_world->GetTimeSeconds() + FMath::RandRange(0.0f, in_refreshInterval);
	else
		LastObstructionOcclusionRefresh = -1;

}

void AkOcclusionObstructionService::Init(UAkComponent* in_akComponent, float in_refreshInterval)
{
	_Init(in_akComponent->GetWorld(), in_refreshInterval);

	if (in_refreshInterval > 0)
	{
		AkGameObjectID gameObjId = (AkGameObjectID)in_akComponent;
		FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
		if (AkAudioDevice && AkAudioDevice->UsingSpatialAudioRooms(in_akComponent->GetWorld()))
			SetOcclusionObstructionFcn = std::bind(AK::SpatialAudio::SetEmitterObstruction, gameObjId, std::placeholders::_2);
		else
			SetOcclusionObstructionFcn = std::bind(AK::SoundEngine::SetObjectObstructionAndOcclusion, gameObjId, std::placeholders::_1, 0.f, std::placeholders::_2);
	}
}

void AkOcclusionObstructionService::Init(AAkAcousticPortal* in_portalId, float in_refreshInterval)
{
	_Init(in_portalId->GetWorld(), in_refreshInterval);

	SetOcclusionObstructionFcn = std::bind(AK::SpatialAudio::SetPortalObstruction, (AkPortalID)in_portalId, std::placeholders::_2);
}

void AkOcclusionObstructionService::SetObstructionOcclusion(const UAkComponentSet& in_Listeners, const FVector& SourcePosition, const AActor* Actor, AkRoomID RoomID, ECollisionChannel in_collisionChannel, const float DeltaTime, float OcclusionRefreshInterval)
{
	auto AudioDevice = FAkAudioDevice::Get();

	// Fade the active occlusions
	bool StillClearingObsOcc = false;
	for (auto It = ListenerInfoMap.CreateIterator(); It; ++It)
	{
		AkGameObjectID Listener = It->Key;

		if (in_Listeners.Find((UAkComponent*)Listener) == nullptr)
		{
			It.RemoveCurrent();
			continue;
		}

		FAkListenerOcclusionObstructionPair& ObsOccPair = It->Value;
		if (ObsOccPair.Update(DeltaTime) && AudioDevice)
		{
			SetOcclusionObstructionFcn(Listener, ObsOccPair.Obs.CurrentValue);
		}

		if (ClearingOcclusionObstruction)
		{
			StillClearingObsOcc |= !ObsOccPair.ReachedTarget();
		}
	}

	if (ClearingOcclusionObstruction)
	{
		ClearingOcclusionObstruction = StillClearingObsOcc;
		return;
	}

	// Compute occlusion only when needed.
	// Have to have "LastObstructionOcclusionRefresh == -1" because GetWorld() might return nullptr in UAkComponent's constructor,
	// preventing us from initializing it to something smart.
	const UWorld* CurrentWorld = Actor ? Actor->GetWorld() : nullptr;
	if (CurrentWorld)
	{
		float CurrentTime = CurrentWorld->GetTimeSeconds();
		if (CurrentTime < LastObstructionOcclusionRefresh && LastObstructionOcclusionRefresh - CurrentTime > OcclusionRefreshInterval)
		{
			// Occlusion refresh interval was made shorter since the last refresh, we need to re-distribute the next random calculation
			LastObstructionOcclusionRefresh = CurrentTime + FMath::RandRange(0.0f, OcclusionRefreshInterval);
		}

		if (LastObstructionOcclusionRefresh == -1 || (CurrentTime - LastObstructionOcclusionRefresh) >= OcclusionRefreshInterval)
			CalculateObstructionOcclusionValues(in_Listeners, SourcePosition, Actor, RoomID, in_collisionChannel, true);
	}
}


void AkOcclusionObstructionService::CalculateObstructionOcclusionValues(const UAkComponentSet& in_Listeners, const FVector& SourcePosition, const AActor* Actor, AkRoomID RoomID, ECollisionChannel in_collisionChannel, bool CalledFromTick)
{
	const UWorld* CurrentWorld = Actor->GetWorld();
	if (!CurrentWorld)
		return;

	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	if (!AkAudioDevice)
		return;

	if (CalledFromTick)
	LastObstructionOcclusionRefresh = CurrentWorld->GetTimeSeconds();

	static const FName NAME_SoundOcclusion = TEXT("SoundOcclusion");
	FCollisionQueryParams CollisionParams(NAME_SoundOcclusion, true, Actor);
	APlayerController* PlayerController = CurrentWorld->GetFirstPlayerController();
	if (PlayerController)
		CollisionParams.AddIgnoredActor(PlayerController->GetPawn());

	for (auto& Listener : in_Listeners)
	{
		auto& MapEntry = ListenerInfoMap.FindOrAdd(Listener->GetAkGameObjectID());

		if (RoomID != Listener->GetSpatialAudioRoom())
			continue;

		const FVector ListenerPosition = Listener->GetPosition();

		FHitResult OutHit;
		const bool bNowOccluded = CurrentWorld->LineTraceSingleByChannel(OutHit, SourcePosition, ListenerPosition, in_collisionChannel, CollisionParams);
		auto& Occlusion = MapEntry.Occ;
		auto& Obstruction = MapEntry.Obs;

		if (bNowOccluded)
		{
			FBox BoundingBox;

			if (OutHit.Actor.IsValid())
			{
				BoundingBox = OutHit.Actor->GetComponentsBoundingBox();
			}
			else if (OutHit.Component.IsValid())
			{
				BoundingBox = OutHit.Component->Bounds.GetBox();
			}

			// Translate the impact point to the bounding box of the obstacle
			const FVector Points[] =
			{
				FVector(OutHit.ImpactPoint.X, BoundingBox.Min.Y, BoundingBox.Min.Z),
				FVector(OutHit.ImpactPoint.X, BoundingBox.Min.Y, BoundingBox.Max.Z),
				FVector(OutHit.ImpactPoint.X, BoundingBox.Max.Y, BoundingBox.Min.Z),
				FVector(OutHit.ImpactPoint.X, BoundingBox.Max.Y, BoundingBox.Max.Z),

				FVector(BoundingBox.Min.X, OutHit.ImpactPoint.Y, BoundingBox.Min.Z),
				FVector(BoundingBox.Min.X, OutHit.ImpactPoint.Y, BoundingBox.Max.Z),
				FVector(BoundingBox.Max.X, OutHit.ImpactPoint.Y, BoundingBox.Min.Z),
				FVector(BoundingBox.Max.X, OutHit.ImpactPoint.Y, BoundingBox.Max.Z),

				FVector(BoundingBox.Min.X, BoundingBox.Min.Y, OutHit.ImpactPoint.Z),
				FVector(BoundingBox.Min.X, BoundingBox.Max.Y, OutHit.ImpactPoint.Z),
				FVector(BoundingBox.Max.X, BoundingBox.Min.Y, OutHit.ImpactPoint.Z),
				FVector(BoundingBox.Max.X, BoundingBox.Max.Y, OutHit.ImpactPoint.Z)
			};
			enum { NumPoints = (sizeof(Points) / sizeof(*Points)) };

			// Compute the number of "second order paths" that are also obstructed. This will allow us to approximate
			// "how obstructed" the source is.
			int32 NumObstructedPaths = 0;
			for (const auto& Point : Points)
			{
				if (CurrentWorld->LineTraceSingleByChannel(OutHit, ListenerPosition, Point, in_collisionChannel, CollisionParams) ||
					CurrentWorld->LineTraceSingleByChannel(OutHit, SourcePosition, Point, in_collisionChannel, CollisionParams))
					++NumObstructedPaths;
			}

			// Modulate occlusion by blocked secondary paths. 
			const float ratio = (float)NumObstructedPaths / NumPoints;
			Occlusion.SetTarget(ratio);
			Obstruction.SetTarget(ratio);

#define AK_DEBUG_OCCLUSION 0
#if AK_DEBUG_OCCLUSION
			// Draw bounding box and "second order paths"
			//UE_LOG(LogAkAudio, Log, TEXT("Target Occlusion level: %f"), ListenerOcclusionInfo[ListenerIdx].TargetValue);
			::FlushPersistentDebugLines(CurrentWorld);
			::FlushDebugStrings(CurrentWorld);
			::DrawDebugBox(CurrentWorld, BoundingBox.GetCenter(), BoundingBox.GetExtent(), FColor::White, false, 4);
			::DrawDebugPoint(CurrentWorld, ListenerPosition, 10.0f, FColor(0, 255, 0), false, 4);
			::DrawDebugPoint(CurrentWorld, SourcePosition, 10.0f, FColor(0, 255, 0), false, 4);
			::DrawDebugPoint(CurrentWorld, OutHit.ImpactPoint, 10.0f, FColor(0, 255, 0), false, 4);

			for (int32 i = 0; i < NumPoints; i++)
			{
				::DrawDebugPoint(CurrentWorld, Points[i], 10.0f, FColor(255, 255, 0), false, 4);
				::DrawDebugString(CurrentWorld, Points[i], FString::Printf(TEXT("%d"), i), nullptr, FColor::White, 4);
				::DrawDebugLine(CurrentWorld, Points[i], ListenerPosition, FColor::Cyan, false, 4);
				::DrawDebugLine(CurrentWorld, Points[i], SourcePosition, FColor::Cyan, false, 4);
			}
			FColor LineColor = FColor::MakeRedToGreenColorFromScalar(1.0f - Occlusion.TargetValue);
			::DrawDebugLine(CurrentWorld, ListenerPosition, SourcePosition, LineColor, false, 4);
#endif // AK_DEBUG_OCCLUSION
		}
		else
		{
			Obstruction.SetTarget(0.0f);
			Occlusion.SetTarget(0.0f);
		}

		if (!CalledFromTick)
		{
			Occlusion.CurrentValue = Occlusion.TargetValue;
			SetOcclusionObstructionFcn((AkGameObjectID)Listener, Obstruction.CurrentValue/*, Occlusion.CurrentValue*/);
		}
	}
}

void AkOcclusionObstructionService::ClearOcclusionValues()
{
	ClearingOcclusionObstruction = false;

	for (auto& ListenerPack : ListenerInfoMap)
	{
		FAkListenerOcclusionObstructionPair& Pair = ListenerPack.Value;
		Pair.Occ.SetTarget(0.0f);
		Pair.Obs.SetTarget(0.0f);
		ClearingOcclusionObstruction |= Pair.ReachedTarget();
	}
}

void AkOcclusionObstructionService::Tick(const UAkComponentSet& in_Listeners, const FVector& SourcePosition, const AActor* Actor, AkRoomID RoomID, ECollisionChannel in_collisionChannel, float DeltaTime, float OcclusionRefreshInterval)
{
	// Check Occlusion/Obstruction, if enabled
	if (OcclusionRefreshInterval > 0.f || ClearingOcclusionObstruction)
		SetObstructionOcclusion(in_Listeners, SourcePosition, Actor, RoomID, in_collisionChannel, DeltaTime, OcclusionRefreshInterval);
	else
		ClearOcclusionValues();
}

void AkOcclusionObstructionService::Refresh(const UAkComponentSet& in_Listeners, const FVector& SourcePosition, const AActor* Actor, AkRoomID RoomID, ECollisionChannel in_collisionChannel, float OcclusionRefreshInterval)
{
	if (OcclusionRefreshInterval > 0.f)
		CalculateObstructionOcclusionValues(in_Listeners, SourcePosition, Actor, RoomID, in_collisionChannel, false);
}

