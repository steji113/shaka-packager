// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_TS_WRITER_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_TS_WRITER_H_

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "packager/file/file.h"
#include "packager/file/file_closer.h"
#include "packager/media/formats/mp2t/continuity_counter.h"

namespace shaka {
namespace media {
namespace mp2t {

class PesPacket;
class ProgramMapTableWriter;

/// This class takes PesPackets, encapsulates them into TS packets, and write
/// the data to file. This also creates PSI from StreamInfo.
class TsWriter {
 public:
  explicit TsWriter(std::unique_ptr<ProgramMapTableWriter> pmt_writer);
  virtual ~TsWriter();

  /// This will fail if the current segment is not finalized.
  /// @param file_name is the output file name.
  /// @param encrypted must be true if the new segment is encrypted.
  /// @return true on success, false otherwise.
  virtual bool NewSegment(const std::string& file_name);

  /// Signals the writer that the rest of the segments are encrypted.
  virtual void SignalEncrypted();

  /// Flush all the pending PesPackets that have not been written to file and
  /// close the file.
  /// @return true on success, false otherwise.
  virtual bool FinalizeSegment();

  /// Add PesPacket to the instance. PesPacket might not get written to file
  /// immediately.
  /// @param pes_packet gets added to the writer.
  /// @return true on success, false otherwise.
  virtual bool AddPesPacket(std::unique_ptr<PesPacket> pes_packet);

 private:
  TsWriter(const TsWriter&) = delete;
  TsWriter& operator=(const TsWriter&) = delete;

  // True if further segments generated by this instance should be encrypted.
  bool encrypted_ = false;

  ContinuityCounter pat_continuity_counter_;
  ContinuityCounter elementary_stream_continuity_counter_;

  std::unique_ptr<ProgramMapTableWriter> pmt_writer_;

  std::unique_ptr<File, FileCloser> current_file_;
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_TS_WRITER_H_
