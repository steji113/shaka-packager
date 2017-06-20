// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FILE_MEDIA_FILE_H_
#define MEDIA_FILE_MEDIA_FILE_H_

#include <stdint.h>

#include <string>
#include <vector>
#include <map>
#include <memory>

#include "packager/media/file/file.h"

namespace shaka {
namespace media {

// A helper filesystem object.  This holds the data for the memory files.
class FileSystem {
public:
	~FileSystem() {}

	static FileSystem* Instance() {
		if (!g_file_system_)
			g_file_system_.reset(new FileSystem());

		return g_file_system_.get();
	}

	bool Exists(const std::string& file_name) const {
		return files_.find(file_name) != files_.end();
	}

	std::vector<uint8_t>* GetFile(const std::string& file_name) {
		return &files_[file_name];
	}

	void Delete(const std::string& file_name) { files_.erase(file_name); }

	void DeleteAll() { files_.clear(); }

	void Add(const std::string &file_name, std::vector<uint8_t> &file) { files_[file_name] = file; }

private:
	FileSystem() {}

	static std::unique_ptr<FileSystem> g_file_system_;

	std::map<std::string, std::vector<uint8_t> > files_;
	DISALLOW_COPY_AND_ASSIGN(FileSystem);
};

/// Implements a File that is stored in memory.  This should be only used for
/// testing, since this does not support larger files.
class MemoryFile : public File {
 public:
  MemoryFile(const std::string& file_name, const std::string& mode);

  /// @name File implementation overrides.
  /// @{
  bool Close() override;
  int64_t Read(void* buffer, uint64_t length) override;
  int64_t Write(const void* buffer, uint64_t length) override;
  int64_t Size() override;
  bool Flush() override;
  bool Seek(uint64_t position) override;
  bool Tell(uint64_t* position) override;
  /// @}

  /// Deletes all memory file data created.  This assumes that there are no
  /// MemoryFile objects alive.  Any alive objects will be in an undefined
  /// state.
  static void DeleteAll();
  /// Deletes the memory file data with the given file_name.  Any objects open
  /// with that file name will be in an undefined state.
  static void Delete(const std::string& file_name);

 protected:
  ~MemoryFile() override;
  bool Open() override;

 private:
  std::string mode_;
  std::vector<uint8_t>* file_;
  uint64_t position_;

  DISALLOW_COPY_AND_ASSIGN(MemoryFile);
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FILE_MEDIA_FILE_H_

