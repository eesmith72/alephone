#ifndef _FILE_HANDLER_
#define _FILE_HANDLER_
/*

	Copyright (C) 1991-2001 and beyond by Bungie Studios, Inc.
	and the "Aleph One" developers.
 
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	This license is contained in the file "COPYING",
	which is included with this source code; it is available online at
	http://www.gnu.org/licenses/gpl.html
*/


#include "cseries.h"

// For the filetypes
#include "tags.h"


#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/positioning.hpp>


// Returned by .GetError() for unknown errors
constexpr int unknown_filesystem_error = -1;



/*
	Abstraction for opened files; it does reading, writing, and closing of such files,
	without doing anything to the files' specifications
*/
class OpenedFile
{
	// This class will need to set the refnum and error value appropriately 
	friend class FileSpecifier;
	friend class opened_file_device;
	
public:
	bool IsOpen();
	bool Close();
	
	bool GetPosition(int64_t& Position);
	bool SetPosition(int64_t Position);
	
	bool GetLength(int64_t& Length);
	bool SetLength(int64_t Length);
	
	bool Read(int64_t Count, void *Buffer);
	bool Write(int64_t Count, void *Buffer);
		
	OpenedFile();
	~OpenedFile() {Close();}	// Auto-close when destroying

	int GetError() {return err;}
	SDL_RWops *GetRWops() {return f;}
	SDL_RWops *TakeRWops();		// Hand over SDL_RWops

private:
	SDL_RWops *f;	// File handle
	int err;		// Error code
	bool is_forked;
    int64_t fork_offset, fork_length;
};

class opened_file_device {
public:
	typedef char char_type;
	typedef boost::iostreams::seekable_device_tag category;
	std::streamsize read(char* s, std::streamsize n);
	std::streamsize write(const char* s, std::streamsize n);
	std::streampos seek(boost::iostreams::stream_offset off, std::ios_base::seekdir way);

	opened_file_device(OpenedFile& f);

private:
	OpenedFile& f;
};

/*
	Abstraction for loaded resources;
	this object will release that resource when it finishes.
	MacOS resource handles will be assumed to be locked.
*/
class LoadedResource
{
	// This class grabs a resource to be loaded into here
	friend class OpenedResourceFile;
	
public:
	// Resource loaded?
	bool IsLoaded();
	
	// Unloads the resource
	void Unload();
	
	// Get size of loaded resource
	int64_t GetLength();
	
	// Get pointer (always present)
	void *GetPointer(bool DoDetach = false);

	// Make resource from raw resource data; the caller gives up ownership
	// of the pointed to memory block
	void SetData(void *data, size_t length);
	
	LoadedResource();
	~LoadedResource() {Unload();}	// Auto-unload when destroying

private:
	// Detaches an allocated resource from this object
	// (keep private to avoid memory leaks)
	void Detach();

public:
	void *p;		// Pointer to resource data (malloc()ed)
	size_t size;	// Size of data
};


/*
	Abstraction for opened resource files:
	it does opening, setting, and closing of such files;
	also getting "LoadedResource" objects that return pointers
*/
class OpenedResourceFile
{
	// This class will need to set the refnum and error value appropriately 
	friend class FileSpecifier;
	
public:
	
	// Pushing and popping the current file -- necessary in the MacOS version,
	// since resource forks are globally open with one of them the current top one.
	// Push() saves the earlier top one makes the current one the top one,
	// while Pop() restores the earlier top one.
	// Will leave SetResLoad in the state of true.
	bool Push();
	bool Pop();

	// Pushing and popping are unnecessary for the MacOS versions of Get() and Check()
	// Check simply checks if a resource is present; returns whether it is or not
	// Get loads a resource; returns whether or not one had been successfully loaded
	// CB: added functions that take 4 characters instead of uint32, which is more portable
	bool Check(uint32 Type, int16 ID);
	bool Check(uint8 t1, uint8 t2, uint8 t3, uint8 t4, int16 ID) {return Check(FOUR_CHARS_TO_INT(t1, t2, t3, t4), ID);}
	bool Get(uint32 Type, int16 ID, LoadedResource& Rsrc);
	bool Get(uint8 t1, uint8 t2, uint8 t3, uint8 t4, int16 ID, LoadedResource& Rsrc) {return Get(FOUR_CHARS_TO_INT(t1, t2, t3, t4), ID, Rsrc);}

	bool IsOpen();
	bool Close();
	
	OpenedResourceFile();
	~OpenedResourceFile() {Close();}	// Auto-close when destroying

	int GetError() {return err;}

private:
	int err;		// Error code
	SDL_RWops *f, *saved_f;
};


// Directories are treated like files
#define DirectorySpecifier FileSpecifier

// Directory entry, returned by FileSpecifier::ReadDirectory()
struct dir_entry {
	dir_entry() : is_directory(false), date(0) {}
	dir_entry(const std::string& n, bool is_dir, TimeType d = 0) : name(n), is_directory(is_dir), date(d) {}

	bool operator<(const dir_entry &other) const
	{
		if (is_directory == other.is_directory)
			return name < other.name;
		else	// Sort directories before files
			return is_directory > other.is_directory;
	}

	bool operator==(const dir_entry& other) const {
		return is_directory == other.is_directory && name == other.name;
	}

    std::string name;		// Entry name
	bool is_directory;	// Entry is a directory (plain file otherwise)
	TimeType date;          // modification date
};


/*
	Abstraction for file specifications;
	designed to encapsulate both directly-specified paths
	and MacOS FSSpecs
*/
class FileSpecifier
{	
public:
	// The typecodes here are the symbolic constants defined in tags.h (_typecode_creator, etc.)
	
	// Get the name (final path element)
	std::string GetName() const;
	
	//   Looks in all directories in the current data search
	//   path for a file with the relative path "NameWithPath" and
	//   sets the file specifier to the full path of the first file
	//   found.
	// "NameWithPath" follows Unix-like syntax: <dirname>/<dirname>/<dirname>/filename
	// A ":" will be translated into a "/" in the MacOS.
	// Returns whether or not the setting was successful
	bool SetNameWithPath(const std::string& NameWithPath);
	bool SetNameWithPath(const std::string& NameWithPath, const DirectorySpecifier& Directory);

	void SetTempName(const FileSpecifier& other);

	// Move the directory specification
	void ToDirectory(DirectorySpecifier& Dir);
	void FromDirectory(DirectorySpecifier& Dir);

	// These functions take an appropriate one of the typecodes used earlier;
	// this is to try to cover the cases of both typecode attributes
	// and typecode suffixes.
	bool Create(Typecode Type);
	
	// Opens a file:
	bool Open(OpenedFile& OFile, bool Writable=false);
	bool OpenForWritingText(OpenedFile& OFile); // converts LF to CRLF on Windows
	
	// Opens either a MacOS resource fork or some imitation of it:
	bool Open(OpenedResourceFile& OFile, bool Writable=false);
	
	// These calls are for creating dialog boxes to set the filespec
	// A null pointer means an empty string
	bool ReadDirectoryDialog();
	bool ReadDialog(Typecode Type, const std::string& Prompt = "");
	bool WriteDialog(Typecode Type, const std::string& Prompt = "", const std::string& DefaultName = "");
	
	// Write dialog box for savegames (must be asynchronous, allowing the sound
	// to continue in the background)
	bool WriteDialogAsync(Typecode Type, const std::string& Prompt = "", const std::string& DefaultName = "");
	
	// Check on whether a file exists, and its type
	bool Exists();
	bool IsDir();
	
	// Gets the modification date
	TimeType GetDate();
	
	// Returns _typecode_unknown if the type could not be identified;
	// the types returned are the _typecode_stuff in tags.h
	Typecode GetType();
	
	// Copy file contents
	bool CopyContents(FileSpecifier& File);
	
	// Delete file
	bool Delete();

	// Rename file
	bool Rename(const FileSpecifier& Destination);

	// Copy file specification
	const FileSpecifier &operator=(const FileSpecifier &other);

	// hide extensions known to Aleph One
	static std::string HideExtension(const std::string& filename);
	
	const std::string GetPath() const { return name; }

	FileSpecifier();
	FileSpecifier(const std::string& s) : name(s), err(0) {canonicalize_path();}
	FileSpecifier(const FileSpecifier &other) : name(other.name), err(other.err) {}

	bool operator==(const FileSpecifier &other) const {return name == other.name;}
	bool operator!=(const FileSpecifier &other) const {return name != other.name;}

	void SetToLocalDataDir();		// Per-user directory (for temporary files)
	void SetToPreferencesDir();		// Directory for preferences (per-user)
	void SetToSavedGamesDir();		// Directory for saved games (per-user)
	void SetToQuickSavesDir();		// Directory for auto-named saved games (per-user)
	void SetToImageCacheDir();		// Directory for image cache (per-user)
	void SetToRecordingsDir();		// Directory for recordings (per-user)

	void AddPart(const std::string &part);
	FileSpecifier &operator+=(const FileSpecifier &other) {AddPart(other.name); return *this;}
	FileSpecifier &operator+=(const std::string& part) {AddPart(std::string(part)); return *this;}
	FileSpecifier operator+(const FileSpecifier &other) const {FileSpecifier a(name); a.AddPart(other.name); return a;}
	FileSpecifier operator+(const std::string& part) const {FileSpecifier a(name); a.AddPart(std::string(part)); return a;}

	void SplitPath(std::string &base, std::string &part) const;
	void SplitPath(DirectorySpecifier &base, std::string &part) const {std::string b; SplitPath(b, part); base = b;}

	bool MakeDirectory();
	
	// Return directory contents (following symlinks), excluding dot-prefixed files
	bool ReadDirectory(std::vector<dir_entry> &vec);
    std::vector<dir_entry> ReadDirectory() {std::vector<dir_entry> vec; ReadDirectory(vec); return vec;}
	
	// Return the names of all entries in a ZIP archive
	bool ReadZIP(std::vector<std::string> &vec);
    std::vector<std::string> ReadZIP() {std::vector<std::string> vec; ReadZIP(vec); return vec;}

	int GetError() const {return err;}

private:
	void canonicalize_path(void);

    std::string name;	// Path name
	int err;
};

// inserts dir before the search path, then restores the original path
// when going out of scope
class ScopedSearchPath
{
public:
	ScopedSearchPath(const DirectorySpecifier& dir);
	~ScopedSearchPath();

private:
	ScopedSearchPath(const ScopedSearchPath&) = delete;
	ScopedSearchPath& operator=(const ScopedSearchPath&) = delete;

	const DirectorySpecifier d;
};

#endif

