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

#include "packager/build/build_config.h"
#include "packager/file/file.h"

#if defined(SHARED_LIBRARY_BUILD)
#if defined(OS_WIN)

#if defined(SHAKA_IMPLEMENTATION)
#define SHAKA_EXPORT __declspec(dllexport)
#else
#define SHAKA_EXPORT __declspec(dllimport)
#endif  // defined(SHAKA_IMPLEMENTATION)

#else  // defined(OS_WIN)

#if defined(SHAKA_IMPLEMENTATION)
#define SHAKA_EXPORT __attribute__((visibility("default")))
#else
#define SHAKA_EXPORT
#endif

#endif  // defined(OS_WIN)

#else  // defined(SHARED_LIBRARY_BUILD)
#define SHAKA_EXPORT
#endif  // defined(SHARED_LIBRARY_BUILD)

namespace shaka {

	// A helper filesystem object.  This holds the data for the memory files.
	class FileSystem {
	public:
		~FileSystem() {}

		static FileSystem* Instance() {
			static FileSystem g_file_system_;

			return &g_file_system_;
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

}  // namespace shaka

#endif  // MEDIA_FILE_MEDIA_FILE_H_
