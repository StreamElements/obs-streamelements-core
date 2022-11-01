#pragma once

#include <obs.h>
//#include <media-io/video-io.h>

#include <memory>
#include <string>

#include "cef-headers.hpp"

class StreamElementsObsOutput;

class StreamElementsObsOutputManager {
public:
	StreamElementsObsOutputManager() {}
	~StreamElementsObsOutputManager() {}

public:
	void DeserializeOutput(CefRefPtr<CefValue> input,
			       CefRefPtr<CefValue> output);

private:
	std::map<std::string, std::shared_ptr<StreamElementsObsOutput>>
		m_outputs;
};


class StreamElementsObsOutput {
public:
	StreamElementsObsOutput() {}
	~StreamElementsObsOutput() {}

public:
	bool Start(video_t* video) {
		auto video_info = video_output_get_info(video);
		//video_info->
	}
	void Stop();
};
