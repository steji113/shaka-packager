// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webvtt/webvtt_parser.h"

#include <string>
#include <vector>

#include "packager/base/logging.h"
#include "packager/base/strings/string_split.h"
#include "packager/base/strings/string_util.h"
#include "packager/media/base/text_stream_info.h"
#include "packager/media/formats/webvtt/webvtt_timestamp.h"

namespace shaka {
namespace media {
namespace {

std::string BlockToString(const std::string* block, size_t size) {
  std::string out = " --- BLOCK START ---\n";

  for (size_t i = 0; i < size; i++) {
    out.append("    ");
    out.append(block[i]);
    out.append("\n");
  }

  out.append(" --- BLOCK END ---");

  return out;
}

// Comments are just blocks that are preceded by a blank line, start with the
// word "NOTE" (followed by a space or newline), and end at the first blank
// line.
// SOURCE: https://www.w3.org/TR/webvtt1
bool IsLikelyNote(const std::string& line) {
  return line == "NOTE" ||
         base::StartsWith(line, "NOTE ", base::CompareCase::SENSITIVE) ||
         base::StartsWith(line, "NOTE\t", base::CompareCase::SENSITIVE);
}

// As cue time is the only part of a WEBVTT file that is allowed to have
// "-->" appear, then if the given line contains it, we can safely assume
// that the line is likely to be a cue time.
bool IsLikelyCueTiming(const std::string& line) {
  return line.find("-->") != std::string::npos;
}

// A WebVTT cue identifier is any sequence of one or more characters not
// containing the substring "-->" (U+002D HYPHEN-MINUS, U+002D HYPHEN-MINUS,
// U+003E GREATER-THAN SIGN), nor containing any U+000A LINE FEED (LF)
// characters or U+000D CARRIAGE RETURN (CR) characters.
// SOURCE: https://www.w3.org/TR/webvtt1/#webvtt-cue-identifier
bool MaybeCueId(const std::string& line) {
  return line.find("-->") == std::string::npos;
}
}  // namespace

WebVttParser::WebVttParser(std::unique_ptr<FileReader> source,
                           const std::string& language)
    : reader_(std::move(source)), language_(language) {}

Status WebVttParser::InitializeInternal() {
  return Status::OK;
}

bool WebVttParser::ValidateOutputStreamIndex(size_t stream_index) const {
  // Only support one output
  return stream_index == 0;
}

Status WebVttParser::Run() {
  return Parse()
             ? FlushDownstream(0)
             : Status(error::INTERNAL_ERROR,
                      "Failed to parse WebVTT source. See log for details.");
}

void WebVttParser::Cancel() {
  keep_reading_ = false;
}

bool WebVttParser::Parse() {
  std::vector<std::string> block;
  if (!reader_.Next(&block)) {
    LOG(ERROR) << "Failed to read WEBVTT HEADER - No blocks in source.";
    return false;
  }

  // Check the header. It is possible for a 0xFEFF BOM to come before the
  // header text.
  if (block.size() != 1) {
    LOG(ERROR) << "Failed to read WEBVTT header - "
               << "block size should be 1 but was " << block.size() << ".";
    return false;
  }
  if (block[0] != "WEBVTT" && block[0] != "\xFE\xFFWEBVTT") {
    LOG(ERROR) << "Failed to read WEBVTT header - should be WEBVTT but was "
               << block[0];
    return false;
  }

  const Status send_stream_info_result = DispatchTextStreamInfo();

  if (send_stream_info_result != Status::OK) {
    LOG(ERROR) << "Failed to send stream info down stream:"
               << send_stream_info_result.error_message();
    return false;
  }

  while (reader_.Next(&block) && keep_reading_) {
    // NOTE
    if (IsLikelyNote(block[0])) {
      // We can safely ignore the whole block.
      continue;
    }

    // CUE with ID
    if (block.size() > 2 && MaybeCueId(block[0]) &&
        IsLikelyCueTiming(block[1]) && ParseCueWithId(block)) {
      continue;
    }

    // CUE with no ID
    if (block.size() > 1 && IsLikelyCueTiming(block[0]) &&
        ParseCueWithNoId(block)) {
      continue;
    }

    LOG(ERROR) << "Failed to determine block classification:\n"
               << BlockToString(block.data(), block.size());
    return false;
  }

  return keep_reading_;
}

bool WebVttParser::ParseCueWithNoId(const std::vector<std::string>& block) {
  const Status status = ParseCue("", block.data(), block.size());

  if (!status.ok()) {
    LOG(ERROR) << "Failed to parse cue: " << status.error_message();
  }

  return status.ok();
}

bool WebVttParser::ParseCueWithId(const std::vector<std::string>& block) {
  const Status status = ParseCue(block[0], block.data() + 1, block.size() - 1);

  if (!status.ok()) {
    LOG(ERROR) << "Failed to parse cue: " << status.error_message();
  }

  return status.ok();
}

Status WebVttParser::ParseCue(const std::string& id,
                              const std::string* block,
                              size_t block_size) {
  const std::vector<std::string> time_and_style = base::SplitString(
      block[0], " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  uint64_t start_time = 0;
  uint64_t end_time = 0;

  const bool parsed_time =
      time_and_style.size() >= 3 && time_and_style[1] == "-->" &&
      WebVttTimestampToMs(time_and_style[0], &start_time) &&
      WebVttTimestampToMs(time_and_style[2], &end_time);

  if (!parsed_time) {
    return Status(
        error::INTERNAL_ERROR,
        "Could not parse start time, -->, and end time from " + block[0]);
  }

  // According to the WebVTT spec
  // (https://www.w3.org/TR/webvtt1/#webvtt-cue-timings) end time must be
  // greater than the start time of the cue. Since we are seeing content with
  // zero-duration cues in the field, we are going to drop the cue instead of
  // failing to package.
  //
  // Print a warning so that those packaging content can know that their
  // content is not spec compliant.
  if (start_time == end_time) {
    LOG(WARNING) << "WebVTT input is not spec compliant."
                    " Skipping zero-duration cue\n"
                 << BlockToString(block, block_size);

    return Status::OK;
  }

  std::shared_ptr<TextSample> sample = std::make_shared<TextSample>();
  sample->set_id(id);
  sample->SetTime(start_time, end_time);

  // The rest of time_and_style are the style tokens.
  for (size_t i = 3; i < time_and_style.size(); i++) {
    sample->AppendStyle(time_and_style[i]);
  }

  // The rest of the block is the payload.
  for (size_t i = 1; i < block_size; i++) {
    sample->AppendPayload(block[i]);
  }

  return DispatchTextSample(0, sample);
}

Status WebVttParser::DispatchTextStreamInfo() {
  // The resolution of timings are in milliseconds.
  const int kTimescale = 1000;

  // The duration passed here is not very important. Also the whole file
  // must be read before determining the real duration which doesn't
  // work nicely with the current demuxer.
  const int kDuration = 0;

  const char kWebVttCodecString[] = "wvtt";

  StreamInfo* info = new TextStreamInfo(0, kTimescale, kDuration, kCodecWebVtt,
                                        kWebVttCodecString, "",
                                        0,  // width
                                        0,  // height
                                        language_);

  return DispatchStreamInfo(0, std::shared_ptr<StreamInfo>(info));
}
}  // namespace media
}  // namespace shaka
