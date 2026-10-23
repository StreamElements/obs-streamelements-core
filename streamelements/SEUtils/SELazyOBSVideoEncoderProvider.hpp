#pragma once

//
// Lazily allocated OBS video encoders, reference counted so that several
// outputs can share one encoder instance.
//
// Moved out of SEUtils.hpp, where it was 450 lines of header-only class in
// the middle of a list of free functions. test_lazy_encoder_refcount asserts
// against this file now.
//

#include "SEObsDataUtils.hpp"
#include "SEObsEncoderUtils.hpp"
#include "SEStringUtils.hpp"

#include "../SETrace.hpp"

#include <cef-headers.hpp>

#include <obs.h>

// OBSEncoderAutoRelease and OBSDataAutoRelease
#include <obs.hpp>
#include <obs-module.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <cstdint>
#include <shared_mutex>

class SELazyOBSVideoEncoderAllocatorBase {
private:
	std::string m_slug;

public:
	SELazyOBSVideoEncoderAllocatorBase(std::string slug) : m_slug(slug) {}
	virtual ~SELazyOBSVideoEncoderAllocatorBase() {}

	virtual obs_encoder_t *AllocRef() = 0;
	virtual void Reset() = 0;
	virtual obs_data_t *GetSettingsRef() = 0;

	virtual std::string GetId() const = 0;
	virtual std::string GetName() const = 0;

public:
	std::string slug() const { return m_slug; }
};

class SELazyOBSVideoEncoderProvider
	: public std::enable_shared_from_this<SELazyOBSVideoEncoderProvider> {
public:
	class SELazyOBSEncoderReference {
	private:
		std::shared_ptr<SELazyOBSVideoEncoderProvider> m_provider =
			nullptr;

	public:
		SELazyOBSEncoderReference(
			std::shared_ptr<SELazyOBSVideoEncoderProvider> provider)
			: m_provider(provider)
		{
			m_provider->AddConsumer();
		}

		~SELazyOBSEncoderReference() { m_provider->RemoveConsumer(); }

		inline operator obs_encoder_t *() const
		{
			return m_provider->GetPtr();
		}
		obs_encoder_t *Get() const { return m_provider->GetPtr(); }

		inline bool operator==(obs_encoder_t *p) const
		{
			return m_provider->GetPtr() == p;
		}
		inline bool operator!=(obs_encoder_t *p) const
		{
			return m_provider->GetPtr() != p;
		}
	};

public:
	class ExternallyAllocatedEncoderAllocator
		: public SELazyOBSVideoEncoderAllocatorBase {
	private:
		struct Private {};

	private:
		obs_encoder_t *m_externallyAllocatedObject;

	public:
		static std::shared_ptr<ExternallyAllocatedEncoderAllocator>
		Create(obs_encoder_t *externallyAllocatedObject)
		{
			if (!externallyAllocatedObject)
				return nullptr;
			return std::make_shared<
				ExternallyAllocatedEncoderAllocator>(
				Private{}, externallyAllocatedObject);
		}

		ExternallyAllocatedEncoderAllocator(
			Private, obs_encoder_t *externallyAllocatedObject)
			: SELazyOBSVideoEncoderAllocatorBase(FormatString(
				  "externallyAllocatedObject: %s",
				  GetIdFromPointer(externallyAllocatedObject)
					  .c_str()))
		{
			m_externallyAllocatedObject = SETRACE_ADDREF(
				obs_encoder_get_ref(externallyAllocatedObject));
		}

		virtual ~ExternallyAllocatedEncoderAllocator()
		{
			if (m_externallyAllocatedObject) {
				obs_encoder_release(SETRACE_DECREF(
					m_externallyAllocatedObject));

				m_externallyAllocatedObject = nullptr;
			}
		}

		virtual obs_encoder_t *AllocRef() override
		{
			return obs_encoder_get_ref(m_externallyAllocatedObject);
		}

		virtual void Reset() override {}

		virtual obs_data_t *GetSettingsRef() override
		{
			return obs_encoder_get_settings(
				m_externallyAllocatedObject);
		}

		virtual std::string GetId() const override
		{
			const auto id =
				obs_encoder_get_id(m_externallyAllocatedObject);

			if (id)
				return id;
			else
				return "";
		}

		virtual std::string GetName() const override
		{
			const auto name = obs_encoder_get_name(
				m_externallyAllocatedObject);

			if (name)
				return name;
			else
				return GetId();
		}
	};

	class CreateEncoderAllocator
		: public SELazyOBSVideoEncoderAllocatorBase {
	private:
		struct Private {};

	private:
		std::string m_id;
		std::string m_name;
		obs_data_t *m_settings = nullptr;
		obs_data_t *m_hotkeys = nullptr;
		uint32_t m_width;
		uint32_t m_height;
		video_t *m_video;

	public:
		static std::shared_ptr<CreateEncoderAllocator>
		Create(std::string id, std::string name, obs_data_t *settings,
		       obs_data_t *hotkeys, uint32_t width, uint32_t height,
		       video_t *video)
		{
			return std::make_shared<CreateEncoderAllocator>(
				Private{}, id, name, settings, hotkeys, width,
				height, video);
		}

		CreateEncoderAllocator(Private, std::string id,
				       std::string name, obs_data_t *settings,
				       obs_data_t *hotkeys, uint32_t width,
				       uint32_t height, video_t *video)
			: SELazyOBSVideoEncoderAllocatorBase(FormatString(
				  "CreateEncoderAllocator: [%d x %d] %s - %s",
				  width, height, id.c_str(), name.c_str()))
		{
			m_id = id;
			m_name = name;
			m_width = width;
			m_height = height;
			m_video = video;

			if (hotkeys) {
				obs_data_addref(SETRACE_ADDREF(hotkeys));

				m_hotkeys = hotkeys;
			}

			m_settings = SETRACE_ADDREF(obs_data_create());

			OBSEncoderAutoRelease encoder =
				obs_video_encoder_create(
					m_id.c_str(),
					CreateGloballyUniqueIdString().c_str(),
					settings, nullptr);

			if (encoder) {
				OBSDataAutoRelease defaultsContainer =
					obs_encoder_get_defaults(encoder);

				OBSDataAutoRelease defaults =
					obs_data_get_defaults(
						defaultsContainer);

				obs_data_apply(m_settings, defaults);

				OBSDataAutoRelease settingsData =
					obs_encoder_get_settings(encoder);

				obs_data_apply(m_settings, settingsData);
			} else if (settings) {
				OBSDataAutoRelease defaults =
					obs_data_get_defaults(settings);

				obs_data_apply(m_settings, settings);
			}
		}

		virtual ~CreateEncoderAllocator()
		{
			if (m_settings) {
				obs_data_release(SETRACE_DECREF(m_settings));
				m_settings = nullptr;
			}
			if (m_hotkeys) {
				obs_data_release(SETRACE_DECREF(m_hotkeys));
				m_hotkeys = nullptr;
			}
		}

		virtual obs_encoder_t *AllocRef() override
		{
			auto created_encoder = obs_video_encoder_create(
				m_id.c_str(), m_name.c_str(), m_settings,
				m_hotkeys);

			if (!created_encoder) {
				blog(LOG_ERROR,
				     "[obs-streamelements-core]: failed lazy-creating video encoder: %s",
				     slug().c_str());

				return nullptr;
			}

			switch (video_output_get_format(obs_get_video())) {
			case VIDEO_FORMAT_I420:
			case VIDEO_FORMAT_NV12:
			case VIDEO_FORMAT_I010:
			case VIDEO_FORMAT_P010:
				break;
			default:
				obs_encoder_set_preferred_video_format(
					created_encoder, VIDEO_FORMAT_NV12);
			}

			//
			// This will prevent obs_encoder_get_width & obs_encoder_get_height from crashing due to video output being improperly initialized for SOME REASON
			// https://app.bugsplat.com/v2/crash?database=OBS_Live&id=1488897
			//
			obs_encoder_set_scaled_size(created_encoder, m_width,
						    m_height);

			obs_encoder_set_video(created_encoder, m_video);

			return created_encoder;
		}

		virtual void Reset() override {}

		virtual obs_data_t *GetSettingsRef() override
		{
			obs_data_t *result = obs_data_create();

			if (m_id.size() > 0) {
				OBSDataAutoRelease defaults =
					obs_encoder_defaults(m_id.c_str());

				obs_data_apply(result, defaults);
			}

			if (m_settings) {
				obs_data_apply(result, m_settings);
			}

			return result;
		}

		virtual std::string GetId() const override { return m_id; }

		virtual std::string GetName() const override { return m_name; }
	};

private:
	std::string m_slug;

	std::shared_mutex m_mutex;
	int m_refCount = 0;
	obs_encoder_t *m_object = nullptr;

	std::shared_ptr<SELazyOBSVideoEncoderAllocatorBase> m_allocator =
		nullptr;

public:
	SELazyOBSVideoEncoderProvider(
		std::string slug,
		std::shared_ptr<SELazyOBSVideoEncoderAllocatorBase> allocator)
		: m_slug(slug), m_allocator(allocator)
	{
	}

	virtual ~SELazyOBSVideoEncoderProvider()
	{
		if (m_refCount > 0) {
			blog(LOG_ERROR,
			     "[obs-streamelements-core]: error: SELazyOBSVideoEncoderProvider is destroyed while there are active references to the underlying object: %s",
			     slug().c_str());
		}
	}

	std::string slug() const
	{
		if (m_slug.size())
			return m_slug + ": " + m_allocator->slug();
		else
			return m_allocator->slug();
	}

	obs_data_t *GetSettingsRef()
	{
		std::shared_lock lock(m_mutex);

		if (m_object) {
			return obs_encoder_get_settings(m_object);
		}

		return m_allocator->GetSettingsRef();
	}

	std::shared_ptr<SELazyOBSVideoEncoderProvider::SELazyOBSEncoderReference>
	GetLazyObjectReference()
	{
		auto shared = this->shared_from_this();

		return std::make_shared<
			SELazyOBSVideoEncoderProvider::SELazyOBSEncoderReference>(
			shared);
	}

	std::shared_ptr<SELazyOBSVideoEncoderProvider::SELazyOBSEncoderReference>
	TryGetLazyObjectReference()
	{
		if (!m_object)
			return nullptr;

		return GetLazyObjectReference();
	}

	std::string GetId() const { return m_allocator->GetId(); }

	std::string GetName() const { return m_allocator->GetName(); }

	CefRefPtr<CefDictionaryValue> SerializeEncoder()
	{
		std::shared_lock lock(m_mutex);

		if (m_object) {
			return SerializeObsEncoder(m_object);
		}

		auto result = CefDictionaryValue::Create();

		result->SetString("class", GetId());
		result->SetString("name", GetName());
		result->SetString("label", GetName());

		auto codec = obs_get_encoder_codec(GetId().c_str());

		if (codec)
			result->SetString("codec", codec);
		else
			result->SetNull("codec");

		auto encoder_type = obs_get_encoder_type(GetId().c_str());

		if (encoder_type == OBS_ENCODER_VIDEO) {
			result->SetString("type", "video");
		} else if (encoder_type == OBS_ENCODER_AUDIO) {
			result->SetString("type", "audio");
		}

		// Settings & properties

		obs_data_t *settings = SETRACE_ADDREF(GetSettingsRef());

		result->SetValue("settings", SerializeObsData(settings));

		result->SetValue("properties", SerializeObsEncoderProperties(
						       GetId(), settings));

		if (settings) {
			obs_data_release(SETRACE_DECREF(settings));
		}

		return result;
	}

protected:
	inline obs_encoder_t *GetPtr() const { return m_object; }

private:
	void AddConsumer()
	{
		std::unique_lock lock(m_mutex);

		++m_refCount;

		if (!m_object) {
			m_object = SETRACE_ADDREF(m_allocator->AllocRef());

			blog(LOG_INFO,
			     "[obs-streamelements-core]: SELazyOBSVideoEncoderProvider::AddConsumer called allocator->AllocRef(): (refCount: %d, object: %s): %s",
			     m_refCount, GetIdFromPointer(m_object).c_str(),
			     slug().c_str());
		}
	}

	void RemoveConsumer()
	{
		std::unique_lock lock(m_mutex);

		--m_refCount;

		if (m_refCount <= 0 && m_object) {
			blog(LOG_INFO,
			     "[obs-streamelements-core]: error: SELazyOBSVideoEncoderProvider::RemoveConsumer calling allocator->Reset(): (refCount: %d, object: %s): %s",
			     m_refCount, GetIdFromPointer(m_object).c_str(),
			     slug().c_str());

			//
			// Release here, and only here (CORE-865).
			//
			// This used to run unconditionally, above the guard. But
			// AddConsumer only takes a reference for the FIRST
			// consumer -- it is guarded by `if (!m_object)` -- so
			// releasing once per consumer meant one acquire against
			// N releases.
			//
			// Several providers share a single obs_encoder_t: an
			// allocator's AllocRef() resolves to another provider's
			// object and takes its own reference on it. In the
			// reported crash one encoder was wrapped by three
			// providers and another by four. So the second consumer
			// to leave dropped the encoder's count to zero, libobs
			// destroyed it, and m_object was left dangling for
			// everyone still holding it -- the next release wrote
			// through freed memory inside obs_encoder_release.
			//
			// Acquire once per object, release once per object.
			//
			ReleaseRef(m_object);

			m_allocator->Reset();

			m_object = nullptr;
		}

		if (m_refCount < 0) {
			blog(LOG_ERROR,
			     "[obs-streamelements-core]: error: SELazyOBSVideoEncoderProvider::RemoveConsumer call results in negative reference count (%d): %s",
			     m_refCount, slug().c_str());
		}
	}

protected:
	obs_encoder_t *AddRef(obs_encoder_t *object)
	{
		return SETRACE_ADDREF(obs_encoder_get_ref(object));
	}

	void ReleaseRef(obs_encoder_t *object)
	{
		obs_encoder_release(SETRACE_DECREF(object));
	}
};
