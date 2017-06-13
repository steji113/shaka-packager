#ifndef MEDIA_FORMATS_CV_CV_MEDIA_PARSER_H_
#define MEDIA_FORMATS_CV_CV_MEDIA_PARSER_H_

#include <stdint.h>

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
	 InitCB init_cb_;
	 NewSampleCB new_sample_cb_;
};
	
}  // namespace cv
}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_CV_CV_MEDIA_PARSER_H_