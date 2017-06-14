#ifndef MEDIA_FORMATS_CV_CV_MEDIA_PARSER_H_
#define MEDIA_FORMATS_CV_CV_MEDIA_PARSER_H_

#include <stdint.h>
#include <vector>

#include "packager/media/base/media_parser.h"

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
	 enum State
	 {
		 kWaitingInit,
		 kParsingMagic,
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
	 std::vector<uint8_t> buffer_;
};
	
}  // namespace cv
}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_CV_CV_MEDIA_PARSER_H_