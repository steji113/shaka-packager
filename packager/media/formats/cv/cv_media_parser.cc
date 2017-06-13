// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/cv/cv_media_parser.h"

#include <algorithm>
#include <limits>

#include "packager/base/callback.h"
#include "packager/base/callback_helpers.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/buffer_reader.h"
#include "packager/media/base/decrypt_config.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/macros.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/rcheck.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/codecs/avc_decoder_configuration_record.h"
#include "packager/media/codecs/es_descriptor.h"
#include "packager/media/codecs/hevc_decoder_configuration_record.h"
#include "packager/media/codecs/vp_codec_configuration_record.h"
#include "packager/media/file/file.h"
#include "packager/media/file/file_closer.h"

namespace
{
	const int kMagicHeaderSize = 4;
	const int kFrameHeaderSize = 5;
	const uint32_t kMagicBytes = 0xDEADBEEF;

	uint32_t Read32(uint8_t *buf)
	{
		return buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
	}
}

namespace shaka {
namespace media {
namespace cv {
	
CVMediaParser::CVMediaParser() :
	state_(kParsingMagic), key_frame_(false), frame_size_(0), pts_(0)
{}

CVMediaParser::~CVMediaParser() {}

void CVMediaParser::Init(const InitCB& init_cb,
	const NewSampleCB& new_sample_cb,
	KeySource* decryption_key_source) {
	DCHECK(init_cb_.is_null());
	DCHECK(!init_cb.is_null());
	DCHECK(!new_sample_cb.is_null());

	init_cb_ = init_cb;
	new_sample_cb_ = new_sample_cb;

	std::vector<std::shared_ptr<StreamInfo>> streams;
	// Should be like 33333333
	uint32_t timescale = 1 / 30 * (10 >> 8);
	uint64_t duration = 0;
	std::string codec = "";
	uint16_t codedWidth = 1920;
	uint16_t codedHeight = 1080;
	uint32_t pixelWidth = 0;
	uint32_t pixelHeight = 0;
	// We do not have NAL unit length prefixes (I don’t think)
	uint8_t nalUnitLengthSize = 0;

	std::shared_ptr<VideoStreamInfo> video_stream_info(new VideoStreamInfo(0, timescale,
		duration, Codec::kCodecH264, H26xStreamFormat::kNalUnitStreamWithoutParameterSetNalus,
		codec, nullptr, 0, codedWidth, codedHeight, pixelWidth, pixelHeight, 0, nalUnitLengthSize,
		std::string(), false));

	streams.push_back(video_stream_info);
	init_cb_.Run(streams);
}

bool CVMediaParser::Flush() {
	return true;
}

bool CVMediaParser::Parse(const uint8_t* buf, int size) {
	// Expand our buffer
	size_t current_size = buffer_.size();
	buffer_.resize(current_size + size);

	// Copy new data to the back
	memcpy(&buffer_[current_size], buf, size);

	// Parsing magic bytes
	if (state_ == State::kParsingMagic && buffer_.size() >= kMagicHeaderSize)
	{
		// Grab the bytes
		uint32_t magicBytes = Read32(&buffer_[0]);
		if (magicBytes == kMagicBytes)
		{
			// All good, change state and clear data
			state_ = State::kParsingHeader;
			buffer_.erase(buffer_.begin(), buffer_.begin() + kMagicHeaderSize);
		}
		else
		{
			return false;
		}
	}
	// Parsing header
	if (state_ == State::kParsingHeader && buffer_.size() >= kFrameHeaderSize)
	{
		// Get the data
		// {1:0|1} - key frame flag
		// {4:size} - nalu size
		key_frame_ = buffer_[0] == 1 ? true : false;
		frame_size_ = Read32(&buffer_[1]);
		// Sanity check the frame size?
		if (frame_size_ > 0)
		{
			// All good, change state and clear data
			state_ = State::kParsingNal;
			buffer_.erase(buffer_.begin(), buffer_.begin() + kFrameHeaderSize);
		}
		else
		{
			return false;
		}
	}
	// Parsing NALU frame
	if (state_ == State::kParsingNal && buffer_.size() >= frame_size_)
	{
		std::shared_ptr<MediaSample> stream_sample(
			MediaSample::CopyFrom(&buffer_[0], frame_size_, key_frame_));
		
		uint64_t ts = pts_++;
		stream_sample->set_dts(ts);
		stream_sample->set_pts(ts);
		uint64_t duration = 33333333;
		stream_sample->set_duration(duration);

		DVLOG(3) << "Pushing frame: "
			<< ", key=" << stream_sample->is_key_frame()
			<< ", dur=" << stream_sample->duration()
			<< ", dts=" << stream_sample->dts()
			<< ", pts=" << stream_sample->pts()
			<< ", size=" << stream_sample->data_size();

		if (!new_sample_cb_.Run(0, stream_sample)) {
			LOG(ERROR) << "Failed to process the sample.";
			return false;
		}
		// All good, change state and clear data
		state_ = State::kParsingHeader;
		buffer_.erase(buffer_.begin(), buffer_.begin() + frame_size_);
	}

	return true;
}
	
}  // namespace cv
}  // namespace media
}  // namespace shaka
