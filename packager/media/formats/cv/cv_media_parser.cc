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
#include "packager/media/codecs/avc_decoder_configuration_record.h"
#include "packager/media/codecs/es_descriptor.h"
#include "packager/media/codecs/hevc_decoder_configuration_record.h"
#include "packager/media/codecs/vp_codec_configuration_record.h"
#include "packager/media/file/file.h"
#include "packager/media/file/file_closer.h"

namespace
{
	// Who knows 30,000 ticks per second
	const uint32_t kTimescale = 9999990;
	const uint32_t kFps = 30;
	// TODO: Fix
	const int kSpsPpsSize = 38;
	const int kMagicHeaderSize = 4;
	const int kFrameHeaderSize = 17;
	const uint32_t kMagicBytes = 0xDEADBEEF;

	uint32_t Read32(uint8_t *buf)
	{
		return buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
	}

	uint64_t Read64(uint8_t *buf)
	{
		uint64_t high = Read32(buf);
		uint64_t low = Read32(buf + 4);
		return (high << 32) | low;
	}
}

namespace shaka {
namespace media {
namespace cv {
	
CVMediaParser::CVMediaParser() :
	got_config_(false), state_(kParsingMagic), key_frame_(false), frame_size_(0), pts_(0),
	frame_duration_(0)
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
}

bool CVMediaParser::Flush() {
	// TODO: What should we do here?
	return true;
}

bool CVMediaParser::Parse(const uint8_t* buf, int size) {
	// Expand our buffer
	size_t current_size = buffer_.size();
	buffer_.resize(current_size + size);

	// Copy new data to the back
	memcpy(&buffer_[current_size], buf, size);

	// MP4 format ref: http://xhelmboyx.tripod.com/formats/mp4-layout.txt
	
	while ((state_ == State::kParsingMagic && buffer_.size() >= kMagicHeaderSize) ||
		(state_ == State::kParsingHeader && buffer_.size() >= kFrameHeaderSize) ||
		((state_ == State::kWaitingInit || state_ == State::kParsingNal) &&
			buffer_.size() >= frame_size_))
	{

		// Parsing magic bytes
		if (state_ == State::kParsingMagic && buffer_.size() >= kMagicHeaderSize)
		{
			if (!ParseCvMagicBytes())
			{
				return false;
			}
		}
		// Parsing header
		if (state_ == State::kParsingHeader && buffer_.size() >= kFrameHeaderSize)
		{
			if (!ParseCvHeader())
			{
				return false;
			}
		}
		// Parsing NALU frame
		if ((state_ == State::kWaitingInit || state_ == State::kParsingNal) &&
			buffer_.size() >= frame_size_)
		{
			if (state_ == State::kWaitingInit)
			{
				// TODO: Figure out SPS and PPS length?
				std::vector<uint8_t> sps_pps(buffer_.begin(), buffer_.begin() + kSpsPpsSize);

				std::vector<std::shared_ptr<StreamInfo>> streams = InitializeInternal(sps_pps);
				if (streams.empty())
				{
					LOG(ERROR) << "Could not find any streams.";
					return false;
				}

				// We are ready
				init_cb_.Run(streams);

				// All good, change state and clear data
				state_ = State::kParsingNal;
				//buffer_.erase(buffer_.begin(), buffer_.begin() + kSpsPpsSize);
				//frame_size_ -= kSpsPpsSize;
				got_config_ = true;
			}

			// TODO: In the future, we might need to strip any inband SPS/PPS
			std::shared_ptr<MediaSample> stream_sample(
				MediaSample::CopyFrom(&buffer_[0], frame_size_, key_frame_));

			stream_sample->set_dts(pts_);
			stream_sample->set_pts(pts_);
			stream_sample->set_duration(frame_duration_);

			LOG(INFO) << "Pushing frame: "
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

	}

	return true;
}

bool CVMediaParser::ParseCvMagicBytes() {

	// Grab the bytes
	uint32_t magicBytes = Read32(&buffer_[0]);
	if (magicBytes != kMagicBytes)
	{
		LOG(ERROR) << "Unexpected magic byte sequence: " << std::hex << magicBytes;
		return false;
	}

	// All good, change state and clear data
	state_ = State::kParsingHeader;
	buffer_.erase(buffer_.begin(), buffer_.begin() + kMagicHeaderSize);

	return true;
}

bool CVMediaParser::ParseCvHeader() {
	// Get the data
	// {1:0|1} - key frame flag
	// {4:size} - nalu size
	// {4:duration} - frame duration
	// {8:timestamp} - frame timestamp
	key_frame_ = buffer_[0] == 1 ? true : false;
	frame_size_ = Read32(&buffer_[1]);
	frame_duration_ = Read32(&buffer_[5]);
	pts_ = Read64(&buffer_[9]);

	// Sanity check the frame size?
	if (frame_size_ <= 0)
	{
		LOG(ERROR) << "Got an unexpected frame size of: " << frame_size_;
		return false;
	}

	// All good, change state and clear data
	state_ = got_config_ ? State::kParsingNal : State::kWaitingInit;
	buffer_.erase(buffer_.begin(), buffer_.begin() + kFrameHeaderSize);

	return true;
}
	
std::vector<std::shared_ptr<StreamInfo>> CVMediaParser::InitializeInternal(
	std::vector<uint8_t> &sps_pps) {
	std::vector<std::shared_ptr<StreamInfo>> streams;

	// See: https://github.com/FFmpeg/FFmpeg/blob/5c6efaffd09de059aa5c7fb9d62bc2e53ba96baf/libavformat/avc.c
	// for parsing of AVC decoder configuration record

	std::vector<uint8_t> decoder_config;
	// TODO: Fix these, I looked at the hex editor
	uint32_t sps_size = 25;
	uint32_t pps_size = 5;
	// Skip over the 4 byte start code
	uint8_t *sps = sps_pps.data() + 4;
	// Skip over the SPS and then another 4 byte start code
	uint8_t *pps = sps + sps_size + 4;

	if (!sps || !pps || sps_size < 4 || sps_size > UINT16_MAX || pps_size > UINT16_MAX)
	{
		return streams;
	}

	// Version
	decoder_config.push_back(1);
	// H.264 profile
	decoder_config.push_back(sps[1]);
	// H.264 profile compatibility
	decoder_config.push_back(sps[2]);
	// H.264 profile level
	decoder_config.push_back(sps[3]);
	// TODO: This could be wrong here
	// 6 bits reserved (111111) + 2 bits nal size length - 1 (11)
	decoder_config.push_back(0xff);
	// 3 bits reserved (111) + 5 bits number of SPS (00001)
	decoder_config.push_back(0xe1);

	uint8_t high = (sps_size >> 8) & 0xff;
	uint8_t low = sps_size & 0xff;
	decoder_config.push_back(high);
	decoder_config.push_back(low);

	size_t current_size = decoder_config.size();
	decoder_config.resize(current_size + sps_size);
	memcpy(&decoder_config[current_size], sps, sps_size);

	// Number of PPS
	decoder_config.push_back(1);

	high = (pps_size >> 8) & 0xff;
	low = pps_size & 0xff;
	decoder_config.push_back(high);
	decoder_config.push_back(low);
	current_size = decoder_config.size();
	decoder_config.resize(current_size + pps_size);
	memcpy(&decoder_config[current_size], pps, pps_size);

	AVCDecoderConfigurationRecord avc_config;
	if (!avc_config.Parse(decoder_config)) {
		LOG(ERROR) << "Failed to parse avcc.";
		return streams;
	}

	// See: https://wiki.whatwg.org/wiki/Video_type_parameters
	// See: https://en.wikipedia.org/wiki/H.264/MPEG-4_AVC
	std::string codec_string = avc_config.GetCodecString(FOURCC_avc1);
	uint8_t nalu_length_size = avc_config.nalu_length_size();
	uint16_t coded_width = avc_config.coded_width();
	uint16_t coded_height = avc_config.coded_height();
	uint32_t pixel_width = avc_config.pixel_width();
	uint32_t pixel_height = avc_config.pixel_height();
	// Now it works out to like ~1m
	uint64_t duration = kTimescale * 60 * 1;

	std::shared_ptr<VideoStreamInfo> video_stream_info(new VideoStreamInfo(0, kTimescale,
		duration, Codec::kCodecH264, H26xStreamFormat::kNalUnitStreamWithoutParameterSetNalus,
		codec_string, decoder_config.data(), decoder_config.size(), coded_width, coded_height,
		pixel_width, pixel_height, 0, nalu_length_size, std::string(), false));

	streams.push_back(video_stream_info);
	return streams;
}

}  // namespace cv
}  // namespace media
}  // namespace shaka
