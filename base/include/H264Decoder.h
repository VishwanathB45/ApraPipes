#pragma once

#include "Module.h"

class H264DecoderProps : public ModuleProps
{
public:
	H264DecoderProps() {}
};

class H264Decoder : public Module
{
public:
	H264Decoder(H264DecoderProps _props);
	virtual ~H264Decoder();
	bool init();
	bool term();
	bool processEOS(string& pinId);

protected:
	bool process(frame_container& frames);
	bool processSOS(frame_sp& frame);
	void addInputPin(framemetadata_sp& metadata, string& pinId);
	bool validateInputPins();
	bool validateOutputPins();
	bool shouldTriggerSOS();

private:
	class Detail;
	boost::shared_ptr<Detail> mDetail;
	bool mShouldTriggerSOS;
	framemetadata_sp mOutputMetadata;
	std::string mOutputPinId;
	H264DecoderProps mProps;
	void sendDecodedFrame();
	void bufferDecodedFrame(frame_sp& frame);
	bool checkFrameDirection(frame_sp& frames);
	std::deque<std::deque<frame_sp>> gop;
	std::deque<frame_sp> tempGop ;
	std::deque<std::deque<frame_sp>> decodedFrames;
	std::deque<frame_sp> tempDecodedFrames;
	std::queue<uint> framesInGopCount;
	uint gopSize = 0;
	uint frameCount = 0;
	bool direction;
	bool foundReverseGopIFrame = false;
};