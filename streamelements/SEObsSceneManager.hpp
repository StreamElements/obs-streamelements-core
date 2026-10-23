#pragma once

#include "SESceneItemsMonitor.hpp"
#include "SEScenesListWidgetManager.hpp"
#include "SEVideoComposition.hpp"
#include "SEVideoCompositionManager.hpp"
#include "SEHostEventDispatcher.hpp"
#include "SEAsyncTaskQueue.hpp"

#include "cef-headers.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

#include <QMainWindow>
#include <QApplication>

#include <mutex>

class SESignalHandlerData;
class SEAuxiliaryControlsBuilder;
class SEHostEventDispatcher;
class SEConfig;

class SEObsSceneManager {
private:
	SESignalHandlerData *m_signalHandlerData = nullptr;

public:
	//
	// The composition manager is injected. It used to be fetched from the
	// singleton in this constructor's own body -- the CORE-735 shape:
	// GetInstance() during construction, correct only because s_instance
	// happened to be the instance building us.
	//
	SEObsSceneManager(
		QMainWindow *parent,
		std::shared_ptr<SEVideoCompositionManager> videoCompositionManager,
		std::shared_ptr<SEAuxiliaryControlsBuilder>
			auxiliaryControlsBuilder,
		std::shared_ptr<SEHostEventDispatcher> hostEventDispatcher,
		std::shared_ptr<SEConfig> config);
	virtual ~SEObsSceneManager();

	//
	// The OBS hookup, and deliberately not part of the constructor.
	//
	// SESignalHandlerData holds this object as a weak_ptr, and a weak_ptr
	// cannot be made from an object whose shared_ptr does not exist yet:
	// inside the constructor make_shared has not returned, so there is
	// nothing to be weak about and every callback would find no manager
	// at all. The container therefore calls this on the line after it
	// builds us.
	//
	// The signals are connected here rather than in the constructor for
	// the same reason stated the other way round: the handle is in place
	// before anything can fire, instead of there being a window in which
	// a signal arrives and finds it empty.
	//
	void AttachObsSignals(std::weak_ptr<SEObsSceneManager> self);

private:
	void CleanObsSceneSignals();

public:
	void Update()
	{
		if (m_sceneItemsMonitor)
			m_sceneItemsMonitor->Update();

		if (m_scenesWidgetManager)
			m_scenesWidgetManager->Update();
	}

	void Reset()
	{
		auto out1 = CefValue::Create();
		auto out2 = CefValue::Create();

		DeserializeSceneItemsAuxiliaryActions(CefValue::Create(),
						      out1);

		DeserializeScenesAuxiliaryActions(CefValue::Create(),
						  out2);

		Update();
	}

	/* Sources */

	void SerializeInputSourceClasses(CefRefPtr<CefValue> &output);

	void SerializeSourceClassProperties(CefRefPtr<CefValue> input,
					    CefRefPtr<CefValue> &output);

	/* Scene items */

	void DeserializeObsBrowserSource(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output);

	void DeserializeObsGameCaptureSource(CefRefPtr<CefValue> input,
					     CefRefPtr<CefValue> &output);

	void DeserializeObsVideoCaptureSource(CefRefPtr<CefValue> input,
					      CefRefPtr<CefValue> &output);

	void DeserializeObsNativeSource(CefRefPtr<CefValue> input,
					CefRefPtr<CefValue> &output);

	void DeserializeObsSceneItemGroup(CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output);

	void SerializeObsSceneItems(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output,
				    bool serializeProperties);

	void RemoveObsSceneItemsByIds(CefRefPtr<CefValue> input,
				      CefRefPtr<CefValue> &output);

	void SetObsSceneItemPropertiesById(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output);

	void GetObsSceneItemPropertiesById(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output);

	void UngroupObsSceneItemsByGroupId(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output);

	void
	InvokeCurrentSceneItemDefaultActionById(CefRefPtr<CefValue> input,
						CefRefPtr<CefValue> &output);

	void InvokeCurrentSceneItemDefaultContextMenuById(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output);

	void DeserializeSceneItemsAuxiliaryActions(CefRefPtr<CefValue> input,
						   CefRefPtr<CefValue> &output);

	void SerializeSceneItemsAuxiliaryActions(CefRefPtr<CefValue> &output);

	/* Scenes */

	void SerializeObsScenes(CefRefPtr<CefValue> input,
				CefRefPtr<CefValue> &output);

	void SerializeObsCurrentScene(CefRefPtr<CefValue> input,
				      CefRefPtr<CefValue> &output);

	void DeserializeObsScene(CefRefPtr<CefValue> input,
				 CefRefPtr<CefValue> &output);

	void SetCurrentObsSceneById(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output);

	void RemoveObsScenesByIds(CefRefPtr<CefValue> input,
				  CefRefPtr<CefValue> &output);

	void SetObsScenePropertiesById(CefRefPtr<CefValue> input,
				       CefRefPtr<CefValue> &output);

	void DeserializeScenesAuxiliaryActions(CefRefPtr<CefValue> input,
					       CefRefPtr<CefValue> &output);

	void SerializeScenesAuxiliaryActions(CefRefPtr<CefValue> &output);

	/* Scene collections */

	void SerializeObsSceneCollections(CefRefPtr<CefValue> &output);

	void SerializeObsCurrentSceneCollection(CefRefPtr<CefValue> &output);

	void DeserializeObsSceneCollection(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output);

	void
	DeserializeObsCurrentSceneCollectionById(CefRefPtr<CefValue> input,
						 CefRefPtr<CefValue> &output);

	/* OBS Native Dialogs */

	void OpenSceneItemPropertiesById(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output);
	void OpenSceneItemFiltersById(CefRefPtr<CefValue> input,
				      CefRefPtr<CefValue> &output);
	void OpenSceneItemInteractionById(CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output);
	void OpenSceneItemTransformEditorById(CefRefPtr<CefValue> input,
					      CefRefPtr<CefValue> &output);

	/* Viewport Coords */

	void SerializeSceneItemViewportRotation(CefRefPtr<CefValue> input,
						CefRefPtr<CefValue> &output);
	void DeserializeSceneItemViewportRotation(CefRefPtr<CefValue> input,
						  CefRefPtr<CefValue> &output);

	void SerializeSceneItemViewportBoundingRectangle(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output);

	void DeserializeSceneItemViewportPosition(CefRefPtr<CefValue> input,
						  CefRefPtr<CefValue> &output);

protected:
	void DeserializeAuxiliaryObsSceneItemProperties(
		std::shared_ptr<SEVideoCompositionBase>
			videoComposition,
		obs_sceneitem_t *sceneitem, CefRefPtr<CefDictionaryValue> d);

protected:
	QMainWindow *mainWindow() { return m_parent; }

	void ObsAddSourceInternal(
		obs_source_t *parentScene, obs_sceneitem_t *parentGroup,
		const char *sourceId, const char *sourceName,
		obs_data_t *sourceSettings, obs_data_t *sourceHotkeyData,
		bool preferExistingSource, const char *existingSourceId,
		obs_source_t **output_source,
		obs_sceneitem_t **output_sceneitem, bool *isExistingSource);

	void RefreshObsSceneItemsList(std::shared_ptr<SEVideoCompositionBase> videoComposition);

	std::string ObsGetUniqueSourceName(std::string name);

	std::string ObsGetUniqueSceneCollectionName(std::string name);

	std::string ObsSetUniqueSourceName(obs_source_t *source,
					   std::string name);

private:
	static void handle_obs_frontend_event(enum obs_frontend_event event,
					      void *data);

protected:
	std::recursive_mutex m_mutex;

private:
	QMainWindow *m_parent;
	std::shared_ptr<SEVideoCompositionManager> m_videoCompositionManager;
	std::shared_ptr<SEConfig> m_config;

	// Held only to hand to SESignalHandlerData in AttachObsSignals(),
	// which runs after the constructor.
	std::shared_ptr<SEHostEventDispatcher> m_hostEventDispatcher;

	//
	// Was a file-static that fetched the manager itself. All nineteen
	// call sites are members of this class, so it becomes one and reads
	// the injected handle instead.
	//
	std::shared_ptr<SEVideoCompositionBase>
	GetVideoComposition(CefRefPtr<CefValue> input);
	SESceneItemsMonitor *m_sceneItemsMonitor = nullptr;
	SEScenesListWidgetManager *m_scenesWidgetManager = nullptr;
};

class SESignalHandlerData {
private:
	std::shared_ptr<SEAsyncTaskQueue> m_asyncTaskQueue =
		nullptr;

public:
	//
	// Holds the composition manager strongly, and that is the point: the
	// callbacks this object is handed to run on the graphics thread and
	// on the task-queue worker, where "is the plugin still alive?" used
	// to be asked of the singleton. Holding the lookup service for as
	// long as the callbacks can fire answers it by construction. No ring
	// -- the composition manager does not own the scene manager.
	//
	// It was weak on every path while this comment said otherwise, and it
	// mattered in exactly the window the comment is about: this object is
	// refcounted, a callback can hold a reference past
	// ~SEObsSceneManager -- the destructor does Wait() then Release(), so
	// anything still in flight keeps it alive -- and the scene manager is
	// the only other holder. During that window the lock failed and the
	// event went nowhere.
	//
	// Strong only on this path, though, which is why there are two
	// members. SEVideoComposition builds one of these too and passes an
	// empty handle for obsSceneManager; there a strong handle closes a
	// ring, because the composition manager owns the composition which
	// owns this. So
	// that is the discriminator below, and the weak handle is what the
	// composition path gets.
	//
	SESignalHandlerData(
		std::weak_ptr<SEObsSceneManager> obsSceneManager,
		std::weak_ptr<SEVideoCompositionManager>
			videoCompositionManager,
		std::weak_ptr<SEVideoCompositionBase> videoComposition,
		std::shared_ptr<SEHostEventDispatcher> hostEventDispatcher)
		: m_obsSceneManager(obsSceneManager),
		  m_videoCompositionManager(videoCompositionManager),
		  m_videoCompositionManagerStrong(
			  obsSceneManager.expired()
				  ? nullptr
				  : videoCompositionManager.lock()),
		  m_videoComposition(videoComposition),
		  m_hostEventDispatcher(hostEventDispatcher)
	{
		// Cached now, while the composition is provably alive. Every
		// later reader wants the id and nothing else, so caching it is
		// what makes it safe for the composition to die first.
		auto composition = videoComposition.lock();

		m_videoCompositionId = composition ? composition->GetId()
						   : std::string();

		m_asyncTaskQueue = std::make_shared<SEAsyncTaskQueue>(
			(std::string(
				 "SESignalHandlerData for video composition ") +
			 (m_videoCompositionId.size() ? m_videoCompositionId
						      : std::string("unknown")))
				.c_str());

		AddRef();

		m_wait_promise.set_value(); // release by default
	}

	~SESignalHandlerData()
	{
		if (m_asyncTaskQueue.get()) {
			m_asyncTaskQueue->Shutdown();
		}

		Clear();
	}

	void EnqueueAsyncTask(std::function<void()> task)
	{
		if (m_parent) {
			m_parent->EnqueueAsyncTask(task);

			return;
		}

		if (m_asyncTaskQueue.get()) {
			m_asyncTaskQueue->Enqueue(task);
		}
	}

private:
	SESignalHandlerData(
		SESignalHandlerData *parent,
		std::weak_ptr<SEObsSceneManager> obsSceneManager,
		std::weak_ptr<SEVideoCompositionManager>
			videoCompositionManager,
		std::weak_ptr<SEVideoCompositionBase> videoComposition,
		std::shared_ptr<SEHostEventDispatcher> hostEventDispatcher,
		obs_scene_t *scene)
		: m_parent(parent),
		  m_obsSceneManager(obsSceneManager),
		  m_videoCompositionManager(videoCompositionManager),
		  //
		  // Same rule as the public constructor: a child of a
		  // scene-manager root inherits the strong handle, a child of
		  // a composition root does not.
		  //
		  m_videoCompositionManagerStrong(
			  obsSceneManager.expired()
				  ? nullptr
				  : videoCompositionManager.lock()),
		  m_videoComposition(videoComposition),
		  m_hostEventDispatcher(hostEventDispatcher),
		  m_scene(SETRACE_ADDREF(obs_scene_get_ref(scene)))
	{
		// No m_asyncTaskQueue here, since there is a parent to refer to
	}

	void Clear()
	{
		if (!m_parent) {
			Wait();
		}

		// std::unique_lock lock(m_scenes_mutex);

		{
			std::lock_guard<std::mutex> lock(
				m_videoCompositionMutex);

			m_videoComposition.reset();
		}

		m_obsSceneManager.reset();

		for (auto kv : m_scenes) {
			delete kv.second;
		}

		m_scenes.clear();
		m_scenes_refcount.clear();
		m_groups_refcount.clear();

		if (m_scene) {
			obs_scene_release(SETRACE_DECREF(m_scene));
		}
	}

public:
	void AddRef() {
		if (m_parent) {
			m_parent->AddRef();
			
			return;
		} else {
			std::unique_lock lock(m_refcount_mutex);

			++m_refcount;
		}
	}

	void Release() {
		bool shouldDelete = false;
		
		if (m_parent) {
			m_parent->Release();
			
			return;
		} else {
			std::unique_lock lock(m_refcount_mutex);

			--m_refcount;
			
			if (m_refcount == 0) {
				shouldDelete = true;
			}
		}
		
		if (shouldDelete) {
			// The cached id, not the composition itself:
			// this runs on the teardown path, which is exactly when
			// the composition is most likely to be gone already.
			blog(LOG_INFO,
			     "[obs-streamelements-core]: released SESignalHandlerData for video composition '%s'",
			     m_videoCompositionId.c_str());

			delete this;
		}
	}

	void GetScenes(std::vector<obs_scene_t *> &scenes)
	{
		SESignalHandlerData *self = this;

		while (self->m_parent)
			self = self->m_parent;

		std::shared_lock lock(self->m_scenes_mutex);
		for (auto kv : self->m_scenes)
			scenes.push_back(kv.first);
	}

	SESignalHandlerData *GetSceneRef(obs_scene_t *scene)
	{
		if (obs_scene_is_group(scene) && m_scene) {
			// If it's a group, use OUR scene (root scene) instead
			scene = m_scene;
		}

		if (m_parent) {
			return m_parent->GetSceneRef(scene);
		}

		std::shared_lock lock(m_scenes_mutex);

		if (!m_scenes.count(scene))
			return nullptr;

		return m_scenes[scene];
	}

	SESignalHandlerData *AddSceneRef(obs_scene_t *scene)
	{
		obs_scene_t *group = nullptr;

		if (obs_scene_is_group(scene) && m_scene) {
			// If it's a group, use OUR scene (root scene) instead
			group = scene;
			scene = m_scene;
		}

		return GetRoot()->AddSceneRefAtRoot(scene, group);
	}

	void RemoveSceneRef(obs_scene_t *scene)
	{
		obs_scene_t *group = nullptr;

		if (obs_scene_is_group(scene) && m_scene) {
			// If it's a group, use OUR scene (root scene) instead
			group = scene;
			scene = m_scene;
		}

		GetRoot()->RemoveSceneRefAtRoot(scene, group);
	}

	// The handler data that owns the scene map, the task queue and the
	// Lock()/Unlock() count. It drains its queue before it is destroyed, so
	// unlike a per-scene child it is safe for queued work to hold.
	SESignalHandlerData *GetRoot()
	{
		return m_parent ? m_parent->GetRoot() : this;
	}

private:
	SESignalHandlerData *AddSceneRefAtRoot(obs_scene_t *scene,
					       obs_scene_t *group)
	{
		std::unique_lock lock(m_scenes_mutex);

		if (!m_scenes.count(scene)) {
			m_scenes[scene] = new SESignalHandlerData(
				this, m_obsSceneManager,
				m_videoCompositionManager, m_videoComposition,
				m_hostEventDispatcher, scene);

			m_scenes_refcount[scene] = 1;

			AddRef();
		} else {
			++m_scenes_refcount[scene];
		}

		// A group is counted against its parent scene. Remember which
		// group added that count, so that only its own removal can take
		// it back -- see RemoveSceneRefAtRoot().
		if (group)
			++m_groups_refcount[group];

		return m_scenes[scene];
	}

	void RemoveSceneRefAtRoot(obs_scene_t *scene, obs_scene_t *group)
	{
		SESignalHandlerData *retired = nullptr;

		{
			std::unique_lock lock(m_scenes_mutex);

			if (group) {
				// libobs announces the removal of a group but not
				// always its creation: obs_scene_add_group() --
				// which is how SE.Live creates groups -- emits no
				// item_add, yet removing or ungrouping that group
				// emits item_remove. Subtracting a count that was
				// never added deleted the parent scene's handler
				// data while every signal on that scene was still
				// connected to it (CORE-1715).
				auto it = m_groups_refcount.find(group);

				if (it == m_groups_refcount.end())
					return;

				if (--it->second == 0)
					m_groups_refcount.erase(it);
			}

			if (!m_scenes.count(scene))
				return;

			--m_scenes_refcount[scene];

			if (m_scenes_refcount[scene] != 0)
				return;

			retired = m_scenes[scene];

			m_scenes_refcount.erase(scene);
			m_scenes.erase(scene);
		}

		// Both outside the lock. Release() can drop the last reference
		// and delete this object, and the lock must not outlive the
		// mutex it guards.
		delete retired;

		Release();
	}

public:
	obs_scene_t* GetRootSceneRef()
	{
		std::shared_lock lock(m_scenes_mutex);

		auto item = this;

		obs_scene_t* scene = m_scene;

		while (scene && item->m_scene && item->m_parent) {
			item = item->m_parent;

			if (item->m_scene)
				scene = item->m_scene;
		}

		return SETRACE_ADDREF(obs_scene_get_ref(scene));
	}

	void Lock()
	{
		if (m_parent) {
			m_parent->Lock();

			return;
		}

		std::unique_lock lock(m_wait_mutex);

		++m_wait_count;

		if (m_wait_count == 1) {
			m_wait_promise = std::promise<void>();
			m_wait_future = m_wait_promise.get_future();
		}
	}

	void Unlock()
	{
		if (m_parent) {
			m_parent->Unlock();

			return;
		}

		std::unique_lock lock(m_wait_mutex);

		--m_wait_count;

		if (m_wait_count == 0) {
			m_wait_promise.set_value();
		}
	}

	void Wait()
	{
		if (m_parent)
		{
			m_parent->Wait();

			return;
		}

		if (m_asyncTaskQueue.get()) {
			m_asyncTaskQueue->Drain();
		}

		std::shared_future<void> future;

		{
			std::shared_lock lock(m_wait_mutex);

			if (m_wait_count == 0)
				return;

			future = m_wait_future;
		}

		future.wait();
	}

public:
	//
	// The id of the video composition these signals belong to, or an empty
	// string once it has been detached.
	//
	// Readers want the id, never the object. Handing back the cached string
	// keeps the composition pointer from escaping to the OBS signal
	// callbacks that run on the graphics thread -- which is where it used
	// to be dereferenced after the composition had been destroyed
	// (CORE-1114).
	//
	std::string GetVideoCompositionId()
	{
		// Only the root is given the composition, so only the root caches
		// its id -- but every OBS signal is connected with a per-scene
		// child. Answering from the child returned "" for every scene-item
		// event, which sent each one down a fallback that searches OBS's
		// scene list off the UI thread (CORE-1715).
		if (m_parent)
			return m_parent->GetVideoCompositionId();

		std::lock_guard<std::mutex> lock(m_videoCompositionMutex);

		return m_videoComposition.expired() ? std::string()
						    : m_videoCompositionId;
	}

	//
	// Called by the video composition as it is destroyed.
	//
	// Release() alone is not enough: it only reaches ~SESignalHandlerData,
	// and therefore Clear(), when the refcount happens to drop to zero. If
	// anything else still holds a reference, the handle survives the
	// composition.
	//
	// Surviving it is no longer dangerous: the handle is a weak_ptr, so a
	// reader that locks it after the composition is gone is told as much,
	// where the raw pointer this used to be handed it freed memory. The
	// drain below is what still earns this method its keep.
	//
	void DetachVideoComposition()
	{
		// Drain queued work first, so nothing is still using the
		// composition when we let go of it.
		Wait();

		std::lock_guard<std::mutex> lock(m_videoCompositionMutex);

		m_videoComposition.reset();
	}

	// char m_header[7] = "header"; // TODO: Remvoe debug marker

	//
	// Weak, which is the only strength that is correct here: the scene
	// manager owns this object -- it news it, and Release()s it in its
	// own destructor -- so anything stronger is a two-node ring that
	// refcounting cannot unwind.
	//
	// It was a raw back-pointer, which is worse than either: five
	// file-static signal callbacks dereference it to call Update(), and
	// nothing but the ordering inside ~SEObsSceneManager said the object
	// was still there. CORE-757 lists it as the one hidden edge that
	// mattered. Reach it through ObsSceneManager(), which locks.
	//
	std::weak_ptr<SEObsSceneManager> m_obsSceneManager;

	//
	// Two handles on one object, because the strength that is safe
	// depends on who created this one. See the public constructor: the
	// scene-manager path takes the strong one and the composition path
	// cannot, since the composition manager owns the composition that
	// owns this. VideoCompositionManager() prefers the strong handle and
	// falls back to locking the weak one.
	//
	std::weak_ptr<SEVideoCompositionManager> m_videoCompositionManager;
	std::shared_ptr<SEVideoCompositionManager>
		m_videoCompositionManagerStrong;
	//
	// Weak for the same reason from the other direction: the composition
	// manager owns the composition, and the composition owns this. Only
	// ever asked whether the composition is still there -- the id it
	// would otherwise be asked for is cached in the constructor -- so
	// expired() is the whole of its job, and it can answer that
	// truthfully where a raw pointer could not.
	//
	std::weak_ptr<SEVideoCompositionBase> m_videoComposition;

	//
	// Strong, for the same reason the composition manager is: the
	// callbacks this object is handed to announce scene and scene-item
	// changes to pages from the graphics thread and from the task-queue
	// worker, and holding what they announce with for as long as they can
	// fire is what replaced asking the singleton whether the plugin is
	// still alive. No ring -- the dispatcher owns nothing here.
	//
	std::shared_ptr<SEHostEventDispatcher> m_hostEventDispatcher;

public:
	//
	// For the file-static helpers that are handed this object as their
	// callback context and need to look a composition up.
	//
	std::shared_ptr<SEVideoCompositionManager> VideoCompositionManager()
	{
		if (m_videoCompositionManagerStrong)
			return m_videoCompositionManagerStrong;

		return m_videoCompositionManager.lock();
	}

	std::shared_ptr<SEHostEventDispatcher> HostEventDispatcher()
	{
		return m_hostEventDispatcher;
	}

	//
	// The scene manager that owns this, or null once it is gone. Locked
	// per call rather than held, because it is the owner: see
	// m_obsSceneManager. Gone is an ordinary answer on the teardown path
	// rather than an error -- the callbacks that ask simply skip their
	// Update().
	//
	std::shared_ptr<SEObsSceneManager> ObsSceneManager()
	{
		return m_obsSceneManager.lock();
	}

private:
	obs_scene_t* m_scene = nullptr;
	std::string m_videoCompositionId;
	std::mutex m_videoCompositionMutex;
	// char m_footer[7] = "footer"; // TODO: Remvoe debug marker

private:
	std::shared_mutex m_wait_mutex;
	std::promise<void> m_wait_promise;
	std::shared_future<void> m_wait_future = m_wait_promise.get_future();
	uint32_t m_wait_count = 0;

private:
	std::shared_mutex m_scenes_mutex;
	std::map<obs_scene_t *, SESignalHandlerData *> m_scenes;
	std::map<obs_scene_t *, uint32_t> m_scenes_refcount;
	// Groups whose add was counted against their parent scene, by group.
	std::map<obs_scene_t *, uint32_t> m_groups_refcount;
	SESignalHandlerData *m_parent = nullptr;

	std::shared_mutex m_refcount_mutex;
	uint32_t m_refcount = 0;
};

void add_scene_signals(obs_source_t *scene, SESignalHandlerData *data);
void add_scene_signals(obs_scene_t *scene, SESignalHandlerData *data);
void add_source_signals(obs_source_t *source, SESignalHandlerData *data);
void remove_scene_signals(obs_source_t *scene, SESignalHandlerData *data);
void remove_scene_signals(obs_scene_t *scene, SESignalHandlerData *data);
void remove_source_signals(obs_source_t *source, SESignalHandlerData *data);
