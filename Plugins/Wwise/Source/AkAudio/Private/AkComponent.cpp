// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkComponent.cpp:
=============================================================================*/

#include "AkAudioDevice.h"
#include "AkInclude.h"
#include "AkAudioClasses.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Texture2D.h"
#include "Components/BillboardComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "AkComponentCallbackManager.h"
#if WITH_EDITOR
#include "LevelEditorViewport.h"
#include "CameraController.h"
#include "Editor.h"
#endif

/*------------------------------------------------------------------------------------
Component Helpers
------------------------------------------------------------------------------------*/
namespace UAkComponentUtils
{
	APlayerController* GetAPlayerController(const UActorComponent* Component)
	{
		const APlayerCameraManager* AsPlayerCameraManager = Cast<APlayerCameraManager>(Component->GetOwner());
		return AsPlayerCameraManager ? AsPlayerCameraManager->GetOwningPlayerController() : nullptr;
	}

	void GetListenerPosition(const UAkComponent* Component, FVector& Location, FVector& Front, FVector& Up)
	{
		APlayerController* pPlayerController = GetAPlayerController(Component);
		if (pPlayerController != nullptr)
		{
			FVector Right;
			pPlayerController->GetAudioListenerPosition(Location, Front, Right);
			Up = FVector::CrossProduct(Front, Right);
			return;
		}

#if WITH_EDITORONLY_DATA
		TArray<FEditorViewportClient*>* Clients = &GEditor->AllViewportClients;
		static FTransform LastKnownEditorTransform;
		for (int i = 0; i < GEditor->AllViewportClients.Num(); i++)
		{
			FEditorViewportClient* ViewportClient = GEditor->AllViewportClients[i];
			UWorld* World = ViewportClient->GetWorld();
			if (ViewportClient->Viewport && ViewportClient->Viewport->HasFocus() && World->AllowAudioPlayback())
			{
				EWorldType::Type WorldType = World->WorldType;
				if (WorldType == EWorldType::Editor || WorldType == EWorldType::PIE)
				{
					LastKnownEditorTransform = FAkAudioDevice::Get()->GetEditorListenerPosition(i);
					Location = LastKnownEditorTransform.GetLocation();
					Front = LastKnownEditorTransform.GetRotation().GetForwardVector();
					Up = LastKnownEditorTransform.GetRotation().GetUpVector();
					return;
				}
				else if (WorldType != EWorldType::Game && WorldType != EWorldType::GamePreview)
				{
					Location = ViewportClient->GetViewLocation();
					Front = ViewportClient->GetViewRotation().Quaternion().GetForwardVector();
					Up = ViewportClient->GetViewRotation().Quaternion().GetUpVector();
					LastKnownEditorTransform.SetLocation(Location);
					LastKnownEditorTransform.SetRotation(ViewportClient->GetViewRotation().Quaternion());
					return;
				}
			}
		}

		Location = LastKnownEditorTransform.GetLocation();
		Front = LastKnownEditorTransform.GetRotation().GetForwardVector();
		Up = LastKnownEditorTransform.GetRotation().GetUpVector();
#endif
	}

	void GetLocationFrontUp(const UAkComponent* Component, FVector& Location, FVector& Front, FVector& Up)
	{
		if (Component->IsDefaultListener)
		{
			GetListenerPosition(Component, Location, Front, Up);
		}
		else
		{
			auto& Transform = Component->GetTransform();
			Location = Transform.GetTranslation();
			Front = Transform.GetUnitAxis(EAxis::X);
			Up = Transform.GetUnitAxis(EAxis::Z);
		}
	}
}


AkReverbFadeControl::AkReverbFadeControl(const UAkLateReverbComponent& LateReverbComponent)
	: AuxBusId(LateReverbComponent.GetAuxBusId())
	, bIsFadingOut(false)
    , CurrentControlValue(0.f)
    , TargetControlValue(LateReverbComponent.SendLevel)
    , FadeRate(LateReverbComponent.FadeRate)
	, Priority(LateReverbComponent.Priority)
	, FadeControlUniqueId((void*)&LateReverbComponent)
{}

bool AkReverbFadeControl::Update(float DeltaTime)
{
	if (CurrentControlValue != TargetControlValue || bIsFadingOut)
	{
		// Rate (%/s) * Delta (s) = % for given delta, apply to target.
		const float Increment = DeltaTime * FadeRate * TargetControlValue;
		if (bIsFadingOut)
		{
			CurrentControlValue -= Increment;
			if (CurrentControlValue <= 0.f)
				return false;
		}
		else
			CurrentControlValue = FMath::Min(CurrentControlValue + Increment, TargetControlValue);
	}

	return true;
}

AkAuxSendValue AkReverbFadeControl::ToAkAuxSendValue() const
{
	AkAuxSendValue ret;
	ret.listenerID = AK_INVALID_GAME_OBJECT;
	ret.auxBusID = AuxBusId;
	ret.fControlValue = CurrentControlValue;
	return ret;
}

bool AkReverbFadeControl::Prioritize(const AkReverbFadeControl& A, const AkReverbFadeControl& B)
{
	// Ensure the fading out buffers are sent to the end of the array.
	return (A.bIsFadingOut == B.bIsFadingOut) ? (A.Priority > B.Priority) : (A.bIsFadingOut < B.bIsFadingOut);
}

/*------------------------------------------------------------------------------------
	UAkComponent
------------------------------------------------------------------------------------*/

UAkComponent::UAkComponent(const class FObjectInitializer& ObjectInitializer) :
Super(ObjectInitializer)
{
	// Property initialization

	EnableSpotReflectors = false;
	DrawFirstOrderReflections = false;
	DrawSecondOrderReflections = false;
	DrawHigherOrderReflections = false;
	DrawSoundPropagation = false;
	EarlyReflectionOrder = 1;
	EarlyReflectionMaxPathLength = 100000.f;
	EarlyReflectionBusSendGain = 1.f;
	ReflectionFilter = -1;

 	StopWhenOwnerDestroyed = true;
	bUseReverbVolumes = true;
	OcclusionRefreshInterval = 0.2f;

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	bTickInEditor = true;

	bAutoActivate = true;
	bNeverNeedsRenderUpdate = true;
	bWantsOnUpdateTransform = true;

#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif

	AttenuationScalingFactor = 1.0f;
	bAutoDestroy = false;
	bStarted = false;
	bUseDefaultListeners = true;
}

int32 UAkComponent::PostAssociatedAkEvent()
{
	return PostAkEvent(AkAudioEvent, EventName);
}

int32 UAkComponent::PostAkEvent( class UAkAudioEvent * AkEvent, const FString& in_EventName )
{
	return PostAkEventByName(GET_AK_EVENT_NAME(AkEvent, in_EventName));
}

int32 UAkComponent::PostAkEventByName(const FString& in_EventName)
{
	return PostAkEventByNameWithCallback(in_EventName);
}

bool UAkComponent::VerifyEventName(const FString& in_EventName) const
{
	const bool IsEventNameEmpty = in_EventName.IsEmpty();
	if (IsEventNameEmpty)
	{
		FString OwnerName = FString(TEXT(""));
		FString ObjectName = GetFName().ToString();

		const auto owner = GetOwner();
		if (owner)
			OwnerName = owner->GetName();

		UE_LOG(LogAkAudio, Warning, TEXT("[%s.%s] AkComponent: Attempted to post an empty AkEvent name."), *OwnerName, *ObjectName);
	}

	return !IsEventNameEmpty;
}

bool UAkComponent::AllowAudioPlayback() const
{
	UWorld* CurrentWorld = GetWorld();
	return (CurrentWorld && CurrentWorld->AllowAudioPlayback() && !IsBeingDestroyed());
}

AkPlayingID UAkComponent::PostAkEventByNameWithCallback(const FString& in_EventName, AkUInt32 in_uFlags /*= 0*/, AkCallbackFunc in_pfnUserCallback /*= NULL*/, void * in_pUserCookie /*= NULL*/)
{
	AkPlayingID playingID = AK_INVALID_PLAYING_ID;

	auto AudioDevice = FAkAudioDevice::Get();
	if (AudioDevice)
	{
		playingID = AudioDevice->PostEvent(in_EventName, this, in_uFlags, in_pfnUserCallback, in_pUserCookie);
		if (playingID != AK_INVALID_PLAYING_ID)
			bStarted = true;
	}

	return playingID;
}

AkRoomID UAkComponent::GetSpatialAudioRoom() const
{
	AkRoomID roomID;
	if (CurrentRoom)
		roomID = CurrentRoom->GetRoomID();
	return roomID;
}

void UAkComponent::Stop()
{
	if (FAkAudioDevice::Get() && IsRegisteredWithWwise)
	{
		AK::SoundEngine::StopAll(GetAkGameObjectID());
	}
}

void UAkComponent::SetRTPCValue(FString RTPC, float Value, int32 InterpolationTimeMs = 0)
{
	if (FAkAudioDevice::Get())
	{
		auto szRTPC = TCHAR_TO_AK(*RTPC);
		AK::SoundEngine::SetRTPCValue(szRTPC, Value, GetAkGameObjectID(), InterpolationTimeMs);
	}
}

void UAkComponent::PostTrigger(FString Trigger)
{
	if (FAkAudioDevice::Get())
	{
		auto szTrigger = TCHAR_TO_AK(*Trigger);
		AK::SoundEngine::PostTrigger(szTrigger, GetAkGameObjectID());
	}
}

void UAkComponent::SetSwitch(FString SwitchGroup, FString SwitchState)
{
	if (FAkAudioDevice::Get())
	{
		auto szSwitchGroup = TCHAR_TO_AK(*SwitchGroup);
		auto szSwitchState = TCHAR_TO_AK(*SwitchState);

		AK::SoundEngine::SetSwitch(szSwitchGroup, szSwitchState, GetAkGameObjectID());
	}
}

void UAkComponent::SetStopWhenOwnerDestroyed(bool bStopWhenOwnerDestroyed)
{
	StopWhenOwnerDestroyed = bStopWhenOwnerDestroyed;
}

void UAkComponent::SetListeners(const TArray<UAkComponent*>& NewListeners)
{
	auto AudioDevice = FAkAudioDevice::Get();
	if (AudioDevice)
	{
		if (!bUseDefaultListeners)
		{
			for (auto Listener : Listeners)
			{
				Listener->Emitters.Remove(this);
			}
		}

		bUseDefaultListeners = false;

		Listeners.Reset();
		Listeners.Append(NewListeners);

		for (auto Listener : Listeners)
		{
			Listener->Emitters.Add(this);
		}

		AudioDevice->SetListeners(this, Listeners.Array());
	}
}

void UAkComponent::UseReverbVolumes(bool inUseReverbVolumes)
{
	bUseReverbVolumes = inUseReverbVolumes;
}

void UAkComponent::UseEarlyReflections(
	class UAkAuxBus* AuxBus,
	int Order,
	float BusSendGain,
	float MaxPathLength,
	bool SpotReflectors,
	const FString& AuxBusName)
{
	EarlyReflectionAuxBus = AuxBus;
	EarlyReflectionAuxBusName = AuxBusName;

	if (Order > 4 || Order < 1)
	{
		Order = FMath::Clamp(Order, 1, 4);
		UE_LOG(LogAkAudio, Warning, TEXT("UAkComponent::UseEarlyReflections: The order value is invalid. It was clamped to %d"), Order);
	}

	EarlyReflectionOrder = Order;
	EarlyReflectionBusSendGain = BusSendGain;
	EarlyReflectionMaxPathLength = MaxPathLength;

	EnableSpotReflectors = SpotReflectors;
	
	auto AkAudioDevice = FAkAudioDevice::Get();
	if (AkAudioDevice)
	{
		AkAudioDevice->RegisterSpatialAudioEmitter(this);
	}
}

float UAkComponent::GetAttenuationRadius() const
{
	return AkAudioEvent ? AttenuationScalingFactor * AkAudioEvent->MaxAttenuationRadius : 0.f;
}

void UAkComponent::SetOutputBusVolume(float BusVolume)
{
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if (AudioDevice)
	{
		for (auto It = Listeners.CreateIterator(); It; ++It)
		{
			AudioDevice->SetGameObjectOutputBusVolume(this, *It, BusVolume);
		}
	}
}

void UAkComponent::OnRegister()
{
	UWorld* CurrentWorld = GetWorld();
	if(!IsRegisteredWithWwise && CurrentWorld->WorldType != EWorldType::Inactive && CurrentWorld->WorldType != EWorldType::None)
		RegisterGameObject(); // Done before parent so that OnUpdateTransform follows registration and updates position correctly.

	ObstructionService.Init(this, OcclusionRefreshInterval);

	Super::OnRegister();

#if WITH_EDITORONLY_DATA
	UpdateSpriteTexture();
#endif
}

#if WITH_EDITORONLY_DATA
void UAkComponent::UpdateSpriteTexture()
{
	if (SpriteComponent)
	{
		SpriteComponent->SetSprite(LoadObject<UTexture2D>(NULL, TEXT("/Wwise/S_AkComponent.S_AkComponent")));
	}
}
#endif

void UAkComponent::OnUnregister()
{
	// Route OnUnregister event.
	Super::OnUnregister();

	// Don't stop audio and clean up component if owner has been destroyed (default behaviour). This function gets
	// called from AActor::ClearComponents when an actor gets destroyed which is not usually what we want for one-
	// shot sounds.
	AActor* Owner = GetOwner();
	UWorld* CurrentWorld = GetWorld();
	if( !Owner || !CurrentWorld || StopWhenOwnerDestroyed || CurrentWorld->bIsTearingDown || (Owner->GetClass() == APlayerController::StaticClass() && CurrentWorld->WorldType == EWorldType::PIE))
	{
		Stop();
	}
}

void UAkComponent::OnComponentDestroyed( bool bDestroyingHierarchy )
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

	UnregisterGameObject();
}

void UAkComponent::ShutdownAfterError( void )
{
	UnregisterGameObject();

	Super::ShutdownAfterError();
}

void UAkComponent::ApplyAkReverbVolumeList(float DeltaTime)
{
	for (int32 Idx = 0; Idx < ReverbFadeControls.Num(); )
	{
		if (!ReverbFadeControls[Idx].Update(DeltaTime))
			ReverbFadeControls.RemoveAt(Idx);
		else
			++Idx;
	}

	if (ReverbFadeControls.Num() > 1)
		ReverbFadeControls.Sort(AkReverbFadeControl::Prioritize);

	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	if (AkAudioDevice)
	{
		TArray<AkAuxSendValue> AuxSendValues;
		for (int32 Idx = 0; Idx < ReverbFadeControls.Num() && Idx < AkAudioDevice->GetMaxAuxBus(); Idx++)
		{
			AkAuxSendValue* FoundAuxSend = AuxSendValues.FindByPredicate([=](const AkAuxSendValue& ItemInArray) { return ItemInArray.auxBusID == ReverbFadeControls[Idx].AuxBusId; });
			if (FoundAuxSend)
			{
				FoundAuxSend->fControlValue += ReverbFadeControls[Idx].ToAkAuxSendValue().fControlValue;
			}
			else
			{
				AuxSendValues.Add(ReverbFadeControls[Idx].ToAkAuxSendValue());
			}
		}

		AkAudioDevice->SetAuxSends(GetAkGameObjectID(), AuxSendValues);
	}
}

void UAkComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	if (AK::SoundEngine::IsInitialized())
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

		// If we're a listener, update our position here instead of in OnUpdateTransform. 
		// This is because PlayerController->GetAudioListenerPosition caches its value, and it can be out of sync
		if (IsDefaultListener)
			UpdateGameObjectPosition();


		FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
		if (AkAudioDevice && bUseReverbVolumes && AkAudioDevice->GetMaxAuxBus() > 0)
			ApplyAkReverbVolumeList(DeltaTime);

		ObstructionService.Tick(Listeners, GetPosition(), GetOwner(), GetSpatialAudioRoom(), OcclusionCollisionChannel, DeltaTime, OcclusionRefreshInterval);

		if (!HasActiveEvents() && bAutoDestroy && bStarted)
			DestroyComponent();

#if !UE_BUILD_SHIPPING
		if (DrawFirstOrderReflections || DrawSecondOrderReflections || DrawHigherOrderReflections)
			DebugDrawReflections();
		if (DrawSoundPropagation)
		{
			DebugDrawSoundPropagation();
		}
#endif
	}
}

void UAkComponent::Activate(bool bReset)
{
	Super::Activate(bReset);

	UpdateGameObjectPosition();

	// If spawned inside AkReverbVolume(s), we do not want the fade in effect to kick in.
	UpdateAkLateReverbComponentList(GetComponentLocation());
	for (auto& ReverbFadeControl : ReverbFadeControls)
		ReverbFadeControl.ForceCurrentToTargetValue();

	SetAttenuationScalingFactor(AttenuationScalingFactor);
}

void UAkComponent::SetAttenuationScalingFactor(float Value)
{
	AttenuationScalingFactor = Value;
	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (AudioDevice)
		AudioDevice->SetAttenuationScalingFactor(this, AttenuationScalingFactor);
}

void UAkComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

	// If we're a listener, our position will be updated from Tick instead of here.
	// This is because PlayerController->GetAudioListenerPosition caches its value, and it can be out of sync
	if(!IsDefaultListener)
		UpdateGameObjectPosition();
}

bool UAkComponent::HasActiveEvents() const
{
	auto CallbackManager = FAkComponentCallbackManager::GetInstance();
	return (CallbackManager != nullptr) && CallbackManager->HasActiveEvents(GetAkGameObjectID());
}

AkGameObjectID UAkComponent::GetAkGameObjectID() const
{
	return (AkGameObjectID)this;
}

void UAkComponent::GetAkGameObjectName(FString& Name) const
{
	AActor* parentActor = GetOwner();
	if (parentActor)
		Name = parentActor->GetFName().ToString() + ".";

	Name += GetFName().ToString();

	UWorld* CurrentWorld = GetWorld();
	switch (CurrentWorld->WorldType)
	{
	case  EWorldType::Editor:
		Name += "(Editor)";
		break;
	case  EWorldType::EditorPreview:
		Name += "(EditorPreview)";
		break;
	case  EWorldType::GamePreview:
		Name += "(GamePreview)";
		break;
	case  EWorldType::Inactive:
		Name += "(Inactive)";
		break;
	}
}

void UAkComponent::RegisterGameObject()
{
	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	if ( AkAudioDevice )
	{
		if ( bUseDefaultListeners )
		{
			const auto& DefaultListeners = AkAudioDevice->GetDefaultListeners();
			Listeners.Empty(DefaultListeners.Num());
			
			for (auto Listener : DefaultListeners)
			{
				Listeners.Add(Listener);
				// NOTE: We do not add this to Listener's emitter list, the list is only for user specified (non-default) emitters.
			}
		}

		AkAudioDevice->RegisterComponent(this);
		IsRegisteredWithWwise = true;
	}
}

void UAkComponent::UnregisterGameObject()
{
	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	if (AkAudioDevice)
	{
		AkAudioDevice->UnregisterComponent(this);
		IsRegisteredWithWwise = false;
	}

	for (auto Listener : Listeners)
		Listener->Emitters.Remove(this);

	for (auto Emitter : Emitters)
		Emitter->Listeners.Remove(this);
}

void UAkComponent::UpdateAkLateReverbComponentList( FVector Loc )
{
	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	if (!AkAudioDevice)
		return;

	TArray<UAkLateReverbComponent*> FoundComponents = AkAudioDevice->FindLateReverbComponentsAtLocation(Loc, GetWorld());

	// Add the new volumes to the current list
	for (const auto& LateReverbComponent : FoundComponents)
	{
		const auto AuxBusId = LateReverbComponent->GetAuxBusId();
		const int32 FoundIdx = ReverbFadeControls.IndexOfByPredicate([=](const AkReverbFadeControl& Candidate)
		{
			return Candidate.FadeControlUniqueId == (void*)LateReverbComponent;
		});

		if (FoundIdx == INDEX_NONE)
		{
			// The volume was not found, add it to the list
			ReverbFadeControls.Add(AkReverbFadeControl(*LateReverbComponent));
		}
		else
		{
			// The volume was found. We still have to check if it is currently fading out, in case we are
			// getting back in a volume we just exited.
			ReverbFadeControls[FoundIdx].bIsFadingOut = false;
		}
	}

	// Fade out the current volumes not found in the new list
	for (auto& ReverbFadeControl : ReverbFadeControls)
	{
		const int32 FoundIdx = FoundComponents.IndexOfByPredicate([=](const UAkLateReverbComponent* const Candidate)
		{
			return ReverbFadeControl.FadeControlUniqueId == (void*)Candidate;
		});

		if (FoundIdx == INDEX_NONE)
			ReverbFadeControl.bIsFadingOut = true;
	}
}

const FTransform& UAkComponent::GetTransform() const
{
#if UE_4_17_OR_LATER
	return GetComponentTransform();
#else
	return ComponentToWorld;
#endif // UE_4_17_OR_LATER
}

FVector UAkComponent::GetPosition() const
{
	FVector Location, Front, Up;
	UAkComponentUtils::GetLocationFrontUp(this, Location, Front, Up);
	return Location;
}

void UAkComponent::UpdateGameObjectPosition()
{
#ifdef _DEBUG
	CheckEmitterListenerConsistancy();
#endif
	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	if (bIsActive && AkAudioDevice)
	{
		if (AllowAudioPlayback())
		{
			UpdateSpatialAudioRoom(GetComponentLocation());

			AkSoundPosition soundpos;
			FVector Location, Front, Up;
			UAkComponentUtils::GetLocationFrontUp(this, Location, Front, Up);
			FAkAudioDevice::FVectorsToAKTransform(Location, Front, Up, soundpos);
			AkAudioDevice->SetPosition(this, soundpos);
		}
		 
		// Find and apply all AkReverbVolumes at this location
		if (bUseReverbVolumes && AkAudioDevice->GetMaxAuxBus() > 0)
		{
			UpdateAkLateReverbComponentList(GetComponentLocation());
		}
	}
}

void UAkComponent::UpdateSpatialAudioRoom(FVector Location)
{
	if (IsRegisteredWithWwise)
	{
		FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
		if (AkAudioDevice)
		{
			TArray<UAkRoomComponent*> RoomComponents = AkAudioDevice->FindRoomComponentsAtLocation(Location, GetWorld(), 1);
			if (RoomComponents.Num() == 0)
			{
				if (AkAudioDevice->WorldHasActiveRooms(GetWorld()))
				{
					CurrentRoom = nullptr;
					AkAudioDevice->SetInSpatialAudioRoom(GetAkGameObjectID(), GetSpatialAudioRoom());
				}
			}
			else if (CurrentRoom != RoomComponents[0])
			{
				CurrentRoom = RoomComponents[0];
				AkAudioDevice->SetInSpatialAudioRoom(GetAkGameObjectID(), GetSpatialAudioRoom());
			}
		}
	}
}

const TSet<UAkComponent*>& UAkComponent::GetEmitters()
{
	FAkAudioDevice* Device = FAkAudioDevice::Get();
	if (Device)
	{
		auto DefaultListeners = Device->GetDefaultListeners();
		if (DefaultListeners.Contains(this))
			return Device->GetDefaultEmitters();
		else
			return Emitters;
	}
	return Emitters;
}

void UAkComponent::CheckEmitterListenerConsistancy()
{
	for (auto Emitter : GetEmitters())
	{
		check(Emitter->Listeners.Contains(this));
	}

	for (auto Listener : Listeners)
	{
		check(Listener->GetEmitters().Contains(this));
	}
}

#if WITH_EDITOR
void UAkComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAkComponent, EarlyReflectionAuxBus) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UAkComponent, EarlyReflectionAuxBusName) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UAkComponent, EarlyReflectionBusSendGain) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UAkComponent, EarlyReflectionOrder) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UAkComponent, EnableSpotReflectors) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UAkComponent, ReflectionFilter) )
	{
		if (EarlyReflectionAuxBus || !EarlyReflectionAuxBusName.IsEmpty())
			FAkAudioDevice::Get()->RegisterSpatialAudioEmitter(this);
		else
			FAkAudioDevice::Get()->UnregisterSpatialAudioEmitter(this);
	}


	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UAkComponent::_DebugDrawReflections( const AkVector& akEmitterPos, const AkVector& akListenerPos, const AkSoundPathInfo* paths, AkUInt32 uNumPaths) const
{
	::FlushDebugStrings(GWorld);

	for (AkInt32 idxPath = uNumPaths-1; idxPath >= 0; --idxPath)
	{
		const AkSoundPathInfo& path = paths[idxPath];

		unsigned int order = path.numReflections;

		if ((DrawFirstOrderReflections && order == 1) ||
			(DrawSecondOrderReflections && order == 2) ||
			(DrawHigherOrderReflections && order > 2))
		{
			FColor colorLight;
			FColor colorMed;
			FColor colorDark;

			switch ((order - 1))
			{
			case 0:
				colorLight = FColor(0x9DEBF3);
				colorMed = FColor(0x318087);
				colorDark = FColor(0x186067);
				break;
			case 1:
				colorLight = FColor(0xFCDBA2);
				colorMed = FColor(0xDEAB4E);
				colorDark = FColor(0xA97B27);
				break;
			case 2:
			default:
				colorLight = FColor(0xFCB1A2);
				colorMed = FColor(0xDE674E);
				colorDark = FColor(0xA93E27);
				break;
			}

			FColor colorLightGrey(75, 75, 75);
			FColor colorMedGrey(50, 50, 50);
			FColor colorDarkGrey(35, 35, 35);

			const int kPathThickness = 5.f;
			const float kRadiusSphere = 25.f;
			const int kNumSphereSegments = 8;

			const FVector emitterPos = FAkAudioDevice::AKVectorToFVector(akEmitterPos);
			FVector listenerPt = FAkAudioDevice::AKVectorToFVector(akListenerPos);

			for (int idxSeg = path.numReflections-1; idxSeg >= 0; --idxSeg)
			{
				const FVector reflectionPt = FAkAudioDevice::AKVectorToFVector(path.reflectionPoint[idxSeg]);
				
				if (idxSeg != path.numReflections - 1)
				{
					// Note: Not drawing the first leg of the path from the listener.  Often hard to see because it is typically the camera position.
					::DrawDebugLine(GWorld, listenerPt, reflectionPt, path.isOccluded ? colorLightGrey : colorLight, false, -1.f, (uint8)'\000', kPathThickness / order);

					::DrawDebugSphere(GWorld, reflectionPt, (kRadiusSphere/2) / order, kNumSphereSegments, path.isOccluded ? colorLightGrey : colorLight);
				}
				else
				{
					::DrawDebugSphere(GWorld, reflectionPt, kRadiusSphere / order, kNumSphereSegments, path.isOccluded ? colorMedGrey : colorMed);
				}

				if (!path.isOccluded)
				{
					const FVector triPt0 = FAkAudioDevice::AKVectorToFVector(path.triangles[idxSeg].point0);
					const FVector triPt1 = FAkAudioDevice::AKVectorToFVector(path.triangles[idxSeg].point1);
					const FVector triPt2 = FAkAudioDevice::AKVectorToFVector(path.triangles[idxSeg].point2);

					::DrawDebugLine(GWorld, triPt0, triPt1, colorDark);
					::DrawDebugLine(GWorld, triPt1, triPt2, colorDark);
					::DrawDebugLine(GWorld, triPt2, triPt0, colorDark);
					::DrawDebugString(GWorld, reflectionPt, FString(path.triangles[idxSeg].strName));
				}
				
				// Draw image source point.  Not as useful as I had hoped.
				//const FVector imageSrc = FAkAudioDevice::AKVectorToFVector(path.imageSource);
				//::DrawDebugSphere(GWorld, imageSrc, kRadiusSphere/order, kNumSphereSegments, colorDark);

				listenerPt = reflectionPt;
			}

			if (!path.isOccluded)
			{
				// Finally the last path segment towards the emitter.
				::DrawDebugLine(GWorld, listenerPt, emitterPos, path.isOccluded ? colorLightGrey : colorLight, false, -1.f, (uint8)'\000', kPathThickness / order);
			}
			else
			{
				const FVector occlusionPt = FAkAudioDevice::AKVectorToFVector(path.occlusionPoint);
				::DrawDebugSphere(GWorld, occlusionPt, kRadiusSphere / order, kNumSphereSegments, colorDarkGrey);
			}
		}
	}
	
}

void UAkComponent::_DebugDrawSoundPropagation(const AkVector& akEmitterPos, const AkVector& akListenerPos, const AkPropagationPathInfo* paths, AkUInt32 uNumPaths) const
{
	::FlushDebugStrings(GWorld);

	for (AkInt32 idxPath = uNumPaths - 1; idxPath >= 0; --idxPath)
	{
		const AkPropagationPathInfo& path = paths[idxPath];

		FColor purple(0x492E74);
		FColor green(0x267158);
		FColor red(0xAA4339);

		{
			const int kPathThickness = 5.f;
			const float kRadiusSphereMax = 35.f;
			const float kRadiusSphereMin = 2.f;
			const int kNumSphereSegments = 8;

			const FVector emitterPos = FAkAudioDevice::AKVectorToFVector(akEmitterPos);
			FVector prevPt = FAkAudioDevice::AKVectorToFVector(akListenerPos);

			for (int idxSeg = 0; idxSeg < (int)path.numNodes; ++idxSeg)
			{
				const FVector portalPt = FAkAudioDevice::AKVectorToFVector(path.nodePoint[idxSeg]);

				if (idxSeg != 0)
				{
					::DrawDebugLine(GWorld, prevPt, portalPt, purple, false, -1.f, (uint8)'\000', kPathThickness);
				}

				float radWet = kRadiusSphereMin + (1.f - path.wetDiffractionAngle / PI) * (kRadiusSphereMax - kRadiusSphereMin);
				float radDry = kRadiusSphereMin + (1.f - path.dryDiffractionAngle / PI) * (kRadiusSphereMax - kRadiusSphereMin);
				
				::DrawDebugSphere(GWorld, portalPt, radWet, 4, green);
				::DrawDebugSphere(GWorld, portalPt, radDry, 8, red);

				prevPt = portalPt;
			}

			// Finally the last path segment towards the emitter.
			::DrawDebugLine(GWorld, prevPt, emitterPos, purple, false, -1.f, (uint8)'\000', kPathThickness);
		}
	}

}

void UAkComponent::DebugDrawReflections() const
{
	enum { kMaxPaths = 64 };
	AkSoundPathInfo paths[kMaxPaths];
	AkUInt32 uNumPaths = kMaxPaths;
	AkVector listenerPos, emitterPos;
	 
	if (AK::SpatialAudio::QueryIndirectPaths(GetAkGameObjectID(), listenerPos, emitterPos, paths, uNumPaths) == AK_Success && uNumPaths > 0)
		_DebugDrawReflections(emitterPos, listenerPos, paths, uNumPaths);
}

void UAkComponent::DebugDrawSoundPropagation() const
{
	enum { kMaxPaths = 16 };
	AkPropagationPathInfo paths[kMaxPaths];
	AkUInt32 uNumPaths = kMaxPaths;

	AkVector listenerPos, emitterPos;

	if (AK::SpatialAudio::QuerySoundPropagationPaths(GetAkGameObjectID(), listenerPos, emitterPos, paths, uNumPaths) == AK_Success)
	{
		if (uNumPaths > 0)
			_DebugDrawSoundPropagation(emitterPos, listenerPos, paths, uNumPaths);
	}
}
