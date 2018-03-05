#ifndef MEDIA_FORMATS_CV_CV_MEDIA_PARSER_H_
#define MEDIA_FORMATS_CV_CV_MEDIA_PARSER_H_

#include <stdint.h>
#include <vector>

#include "packager/media/base/media_parser.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/codecs/h264_byte_to_unit_stream_converter.h"

namespace shaka {
namespace media {
namespace cv {

class CVMediaParser : public MediaParser {
 public:
  CVMediaParser();
  ~CVMediaParser() override;

  /// @name MediaParser implementation overrides.
  /// @{
  void Init(const InitCB& init_cb,
	  const NewSampleCB& new_sample_cb,
	  KeySource* decryption_key_source) override;
  bool Flush() override WARN_UNUSED_RESULT;
  bool Parse(const uint8_t* buf, int size) override WARN_UNUSED_RESULT;
  /// @}

 private:

	 bool ParseCvMagicBytes();
	 bool ParseCvHeader();
	 std::vector<std::shared_ptr<StreamInfo>> InitializeInternal(
		 std::vector<uint8_t> &decoder_config);

	 enum State
	 {
		 kParsingMagic,
		 kWaitingInit,
		 kParsingHeader,
		 kParsingNal
	 };

	 InitCB init_cb_;
	 NewSampleCB new_sample_cb_;
	 bool got_config_;
	 State state_;
	 bool key_frame_;
	 uint32_t frame_size_;
	 uint64_t pts_;
	 int64_t pts_base_;
	 uint64_t frame_duration_;
	 std::vector<uint8_t> buffer_;
	 std::unique_ptr<H264ByteToUnitStreamConverter> h264_byte_to_unit_stream_converter_;
};
	
}  // namespace cv
}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_CV_CV_MEDIA_PARSER_H_