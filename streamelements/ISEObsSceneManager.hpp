#pragma once

#include "cef-headers.hpp"

//
// The largest contract in this series at 37 methods, and the least
// interesting one to write: every signature is CefValue in, CefValue out.
// Part 1 fixed six dirty service signatures and none of them was here, so
// there was nothing to clean off this one first.
//
// The groups below are the header's own -- sources, scene items, scenes,
// scene collections, OBS's native dialogs, viewport coordinates -- kept so
// the two files can be read side by side.
//
// What stays on SEObsSceneManager, because only the root calls it:
// AttachObsSignals, which takes a weak_ptr<SEObsSceneManager> and must name
// the concrete type for the reason its own comment gives; Update(), driven
// by the root and by Reset() below; CleanObsSceneSignals; and the whole
// protected half, which is where every obs type in the class lives.
//
// Reset() is on the contract by the union rule rather than by the handler:
// SESessionManager calls it when signing out, so it is not root-only.
//
class ISEObsSceneManager {
public:
	virtual ~ISEObsSceneManager() {}

public:
	virtual void Reset() = 0;

	/* Sources */

	virtual void
	SerializeInputSourceClasses(CefRefPtr<CefValue> &output) = 0;

	virtual void
	SerializeSourceClassProperties(CefRefPtr<CefValue> input,
				       CefRefPtr<CefValue> &output) = 0;

	/* Scene items */

	virtual void
	DeserializeObsBrowserSource(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output) = 0;

	virtual void
	DeserializeObsGameCaptureSource(CefRefPtr<CefValue> input,
					CefRefPtr<CefValue> &output) = 0;

	virtual void
	DeserializeObsVideoCaptureSource(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output) = 0;

	virtual void
	DeserializeObsNativeSource(CefRefPtr<CefValue> input,
				   CefRefPtr<CefValue> &output) = 0;

	virtual void
	DeserializeObsSceneItemGroup(CefRefPtr<CefValue> input,
				     CefRefPtr<CefValue> &output) = 0;

	virtual void SerializeObsSceneItems(CefRefPtr<CefValue> input,
					    CefRefPtr<CefValue> &output,
					    bool serializeProperties) = 0;

	virtual void RemoveObsSceneItemsByIds(CefRefPtr<CefValue> input,
					      CefRefPtr<CefValue> &output) = 0;

	virtual void
	SetObsSceneItemPropertiesById(CefRefPtr<CefValue> input,
				      CefRefPtr<CefValue> &output) = 0;

	virtual void
	GetObsSceneItemPropertiesById(CefRefPtr<CefValue> input,
				      CefRefPtr<CefValue> &output) = 0;

	virtual void
	UngroupObsSceneItemsByGroupId(CefRefPtr<CefValue> input,
				      CefRefPtr<CefValue> &output) = 0;

	virtual void InvokeCurrentSceneItemDefaultActionById(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) = 0;

	virtual void InvokeCurrentSceneItemDefaultContextMenuById(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) = 0;

	virtual void
	DeserializeSceneItemsAuxiliaryActions(CefRefPtr<CefValue> input,
					      CefRefPtr<CefValue> &output) = 0;

	virtual void
	SerializeSceneItemsAuxiliaryActions(CefRefPtr<CefValue> &output) = 0;

	/* Scenes */

	virtual void SerializeObsScenes(CefRefPtr<CefValue> input,
					CefRefPtr<CefValue> &output) = 0;

	virtual void SerializeObsCurrentScene(CefRefPtr<CefValue> input,
					      CefRefPtr<CefValue> &output) = 0;

	virtual void DeserializeObsScene(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output) = 0;

	virtual void SetCurrentObsSceneById(CefRefPtr<CefValue> input,
					    CefRefPtr<CefValue> &output) = 0;

	virtual void RemoveObsScenesByIds(CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output) = 0;

	virtual void SetObsScenePropertiesById(CefRefPtr<CefValue> input,
					       CefRefPtr<CefValue> &output) = 0;

	virtual void
	DeserializeScenesAuxiliaryActions(CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output) = 0;

	virtual void
	SerializeScenesAuxiliaryActions(CefRefPtr<CefValue> &output) = 0;

	/* Scene collections */

	virtual void
	SerializeObsSceneCollections(CefRefPtr<CefValue> &output) = 0;

	virtual void
	SerializeObsCurrentSceneCollection(CefRefPtr<CefValue> &output) = 0;

	virtual void
	DeserializeObsSceneCollection(CefRefPtr<CefValue> input,
				      CefRefPtr<CefValue> &output) = 0;

	virtual void DeserializeObsCurrentSceneCollectionById(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) = 0;

	/* OBS Native Dialogs */

	virtual void
	OpenSceneItemPropertiesById(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output) = 0;
	virtual void OpenSceneItemFiltersById(CefRefPtr<CefValue> input,
					      CefRefPtr<CefValue> &output) = 0;
	virtual void
	OpenSceneItemInteractionById(CefRefPtr<CefValue> input,
				     CefRefPtr<CefValue> &output) = 0;
	virtual void
	OpenSceneItemTransformEditorById(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output) = 0;

	/* Viewport Coords */

	virtual void
	SerializeSceneItemViewportRotation(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output) = 0;
	virtual void
	DeserializeSceneItemViewportRotation(CefRefPtr<CefValue> input,
					     CefRefPtr<CefValue> &output) = 0;

	virtual void SerializeSceneItemViewportBoundingRectangle(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) = 0;

	virtual void
	DeserializeSceneItemViewportPosition(CefRefPtr<CefValue> input,
					     CefRefPtr<CefValue> &output) = 0;
};
