/*
===========================================================================

Daemon GPL Source Code
Copyright (C) 2013 Unvanquished Developers

This file is part of the Daemon GPL Source Code (Daemon Source Code).

Daemon Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Daemon Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon Source Code.  If not, see <http://www.gnu.org/licenses/>.

===========================================================================
*/

#ifndef FRAMEWORK_FILESYSTEM_H_
#define FRAMEWORK_FILESYSTEM_H_

#include <system_error>
#include <iterator>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include "../../common/String.h"

namespace FS {

// File offset type. Using 64bit to allow large files.
typedef int64_t offset_t;

// Special value to indicate the function should throw a system_error instead
// of returning an error code. This avoids the need to have 2 overloads for each
// function.
inline std::error_code& throws()
{
	std::error_code* ptr = nullptr;
	return *ptr;
}

// Wrapper around stdio
class File {
public:
	File()
		: fd(nullptr) {}
	explicit File(FILE* fd)
		: fd(fd) {}

	// Files are noncopyable but movable
	File(const File&) = delete;
	File& operator=(const File&) = delete;
	File(File&& other)
		: fd(other.fd)
	{
		other.fd = nullptr;
	}
	File& operator=(File&& other)
	{
		std::swap(fd, other.fd);
		return *this;
	}

	// Check if a file is open
	explicit operator bool() const
	{
		return fd != nullptr;
	}

	// Close the file
	void Close(std::error_code& err = throws());
	~File()
	{
		// Don't throw in destructor
		std::error_code err;
		Close(err);
	}

	// Get the length of the file
	offset_t Length(std::error_code& err = throws()) const;

	// Get the timestamp (time of last modification) of the file
	time_t Timestamp(std::error_code& err = throws()) const;

	// File position manipulation
	void SeekCur(offset_t off, std::error_code& err = throws()) const;
	void SeekSet(offset_t off, std::error_code& err = throws()) const;
	void SeekEnd(offset_t off, std::error_code& err = throws()) const;
	offset_t Tell() const;

	// Read/Write from the file
	size_t Read(void* buffer, size_t length, std::error_code& err = throws()) const;
	void Write(const void* data, size_t length, std::error_code& err = throws()) const;

	// Flush buffers
	void Flush(std::error_code& err = throws()) const;

	// Read the entire file into a string
	std::string ReadAll(std::error_code& err = throws()) const;

	// Copy the entire file to another file
	void CopyTo(const File& dest, std::error_code& err = throws()) const;

private:
	FILE* fd;
};

// Path manipulation functions
namespace Path {

	// Check whether a path is valid. Validation rules:
	// - The path separator is the forward slash (/)
	// - A path element must start and end with [0-9][A-Z][a-z]
	// - A path element may contain [0-9][A-Z][a-z]-_.
	// - A path element may not contain two successive -_. characters
	// Note that paths are validated in all filesystem functions, so manual
	// validation is usually not required.
	// Implicitly disallows the following:
	// - Special files . and ..
	// - Absolute paths starting with /
	bool IsValid(Str::StringRef path, bool allowDir);

	// Build a path from components
	std::string Build(Str::StringRef base, Str::StringRef path);

	// Get the directory portion of a path:
	// a/b/c => a/b/
	// a/b/ => a/
	// a => ""
	std::string DirName(Str::StringRef path);

	// Get the filename portion of a path
	// a/b/c => c
	// a/b/ => b/
	// a => a
	std::string BaseName(Str::StringRef path);

} // namespace Path

// Iterator which iterates over all files in a directory
template<typename State> class DirectoryIterator: public std::iterator<std::input_iterator_tag, std::string> {
	// State interface:
	// bool Advance(std::error_code& err);
	// std::string current;
public:
	DirectoryIterator()
		: state(nullptr) {}
	explicit DirectoryIterator(State* state)
		: state(state) {}

	const std::string& operator*() const
	{
		return state->current;
	}
	const std::string* operator->() const
	{
		return &state->current;
	}

	DirectoryIterator& increment(std::error_code& err = throws())
	{
		if (!state->Advance(err))
			state = nullptr;
		return *this;
	}
	DirectoryIterator& operator++()
	{
		return increment(throws());
	}
	DirectoryIterator operator++(int)
	{
		DirectoryIterator out = *this;
		increment(throws());
		return out;
	}

	friend bool operator==(const DirectoryIterator& a, const DirectoryIterator& b)
	{
		return a.state == b.state;
	}
	friend bool operator!=(const DirectoryIterator& a, const DirectoryIterator& b)
	{
		return a.state != b.state;
	}

private:
	State* state;
};

// Type of pak
enum pakType_t {
	PAK_ZIP, // Zip archive
	PAK_DIR // Directory
};

// Name, version and checksum used to describe a pak file
struct PakDesc {
	// Base name of the pak, may include directories
	std::string name;

	// Version of the pak
	std::string version;

	// Checksum of the pak
	std::string checksum;
};

// Information about a loaded pak
struct LoadedPak {
	// Pak information
	PakDesc pak;

	// Type of pak
	pakType_t type;

	// Full path to the pak
	std::string path;
};

// A namespace is a set of pak files which contain files
class PakNamespace {
	struct pakFileInfo_t {
		LoadedPak* pak; // Points to an element in the pakList vector
		offset_t offset;
	};
	typedef std::unordered_map<std::string, pakFileInfo_t> fileMap_t;
	template<bool recursive> class BasicDirectoryRange {
	public:
		DirectoryIterator<BasicDirectoryRange> begin()
		{
			return DirectoryIterator<BasicDirectoryRange>(empty() ? nullptr : this);
		}
		DirectoryIterator<BasicDirectoryRange> end()
		{
			return DirectoryIterator<BasicDirectoryRange>();
		}
		bool empty() const
		{
			return iter == iter_end;
		}

	private:
		friend class DirectoryIterator<BasicDirectoryRange>;
		friend class PakNamespace;
		bool Advance(std::error_code& err);
		bool InternalAdvance();
		std::string current;
		std::string prefix;
		fileMap_t::const_iterator iter, iter_end;
	};
	std::vector<LoadedPak> pakList;
	fileMap_t fileMap;

public:
	// Load a pak into the namespace with all its dependencies
	bool LoadPak(Str::StringRef name);
	bool LoadPak(Str::StringRef name, Str::StringRef version);
	bool LoadPak(Str::StringRef name, Str::StringRef version, Str::StringRef checksum);

	// Get a list of all the loaded paks
	const std::vector<LoadedPak>& GetLoadedPaks() const
	{
		return pakList;
	}

	// Read an entire file into a string
	std::string ReadFile(Str::StringRef path, std::error_code& err = throws()) const;

	// Copy an entire file to another file
	void CopyFile(Str::StringRef path, const File& dest, std::error_code& err = throws()) const;

	// Check if a file exists
	bool FileExists(Str::StringRef path) const;

	// Get the pak a file is in
	const LoadedPak* LocateFile(Str::StringRef path, std::error_code& err = throws()) const;

	// Get the timestamp of a file
	time_t FileTimestamp(Str::StringRef path, std::error_code& err = throws()) const;

	// List all files in the given subdirectory, optionally recursing into subdirectories
	// Directory names are returned with a trailing slash to differentiate them from files
	typedef BasicDirectoryRange<false> DirectoryRange;
	typedef BasicDirectoryRange<true> RecursiveDirectoryRange;
	DirectoryRange ListFiles(Str::StringRef path, std::error_code& err = throws()) const;
	RecursiveDirectoryRange ListFilesRecursive(Str::StringRef path, std::error_code& err = throws()) const;
};

// Operations which work on raw OS paths. Note that no validation on file names is performed
namespace RawPath {

	// Open a file for reading/writing/appending/editing
	File OpenRead(Str::StringRef path, std::error_code& err = throws());
	File OpenWrite(Str::StringRef path, std::error_code& err = throws());
	File OpenAppend(Str::StringRef path, std::error_code& err = throws());

	// Check if a file exists
	bool FileExists(Str::StringRef path);

	// Get the timestamp of a file
	time_t FileTimestamp(Str::StringRef path, std::error_code& err = throws());

	// Move/delete a file
	void MoveFile(Str::StringRef dest, Str::StringRef src, std::error_code& err = throws());
	void DeleteFile(Str::StringRef path, std::error_code& err = throws());

	// List all files in the given subdirectory, optionally recursing into subdirectories
	// Directory names are returned with a trailing slash to differentiate them from files
	class DirectoryRange {
	public:
		DirectoryIterator<DirectoryRange> begin()
		{
			return DirectoryIterator<DirectoryRange>(empty() ? nullptr : this);
		}
		DirectoryIterator<DirectoryRange> end()
		{
			return DirectoryIterator<DirectoryRange>();
		}
		bool empty() const
		{
			return handle == nullptr;
		}

	private:
		friend class DirectoryIterator<DirectoryRange>;
		friend DirectoryRange ListFiles(Str::StringRef path, std::error_code& err);
		bool Advance(std::error_code& err);
		std::string current;
		std::shared_ptr<void> handle;
#ifndef _WIN32
		std::string path;
#endif
	};
	class RecursiveDirectoryRange {
	public:
		DirectoryIterator<RecursiveDirectoryRange> begin()
		{
			return DirectoryIterator<RecursiveDirectoryRange>(empty() ? nullptr : this);
		}
		DirectoryIterator<RecursiveDirectoryRange> end()
		{
			return DirectoryIterator<RecursiveDirectoryRange>();
		}
		bool empty() const
		{
			return dirs.empty();
		}

	private:
		friend class DirectoryIterator<RecursiveDirectoryRange>;
		friend RecursiveDirectoryRange ListFilesRecursive(Str::StringRef path, std::error_code& err);
		bool Advance(std::error_code& err);
		std::string current;
		std::string path;
		std::vector<DirectoryRange> dirs;
	};
	DirectoryRange ListFiles(Str::StringRef path, std::error_code& err = throws());
	RecursiveDirectoryRange ListFilesRecursive(Str::StringRef path, std::error_code& err = throws());

} // namespace RawPath

// Operations which work on the home path
namespace HomePath {

	// Open a file for reading/writing/appending/editing
	File OpenRead(Str::StringRef path, std::error_code& err = throws());
	File OpenWrite(Str::StringRef path, std::error_code& err = throws());
	File OpenAppend(Str::StringRef path, std::error_code& err = throws());

	// Check if a file exists
	bool FileExists(Str::StringRef path);

	// Get the timestamp of a file
	time_t FileTimestamp(Str::StringRef path, std::error_code& err = throws());

	// Move/delete a file
	void MoveFile(Str::StringRef dest, Str::StringRef src, std::error_code& err = throws());
	void DeleteFile(Str::StringRef path, std::error_code& err = throws());

	// List all files in the given subdirectory, optionally recursing into subdirectories
	// Directory names are returned with a trailing slash to differentiate them from files
	typedef RawPath::DirectoryRange DirectoryRange;
	typedef RawPath::RecursiveDirectoryRange RecursiveDirectoryRange;
	DirectoryRange ListFiles(Str::StringRef path, std::error_code& err = throws());
	RecursiveDirectoryRange ListFilesRecursive(Str::StringRef path, std::error_code& err = throws());

} // namespace HomePath

// Initialize the filesystem and the main paths
void Initialize();

// Get the home path
const std::string& GetHomePath();

// Get the path containing program binaries
const std::string& GetLibPath();

} // namespace FS

#endif // FRAMEWORK_FILESYSTEM_H_
