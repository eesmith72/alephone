/*
 resource_manager.h - MacOS resource handling for non-Mac platforms
 
 Written in 2000 by Christian Bauer
 
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

#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include "cseries.hpp"
#include "DataFile.hpp"


void initialize_resources(void);


// a little odd as this seems an obvious candidate to make an object
SDL_RWops *open_resource_file(const ao_path& path);

void close_file_resource(SDL_RWops *file);

SDL_RWops *get_current_resource_file(); // hmmm

void use_file_resource(SDL_RWops *file);





/*
    Abstraction for loaded resources; this object will release that resource when it finishes. // almost certainly completely useless, it's just a pointer into a block of memory, plus its length; given that the parsers for this data are old M2 code, it's probably best to get a ptr into the managed memory and return a simple {uint8_t* data,size_t size} struct for the parser to chew on
    MacOS resource handles will be assumed to be locked. // really
*/
class LoadedResource
{
    // This class grabs a resource to be loaded into here // TODO: euwwww
    friend class ResourceFile;
    
public:
    LoadedResource() : p(NULL), size(0) {}
    ~LoadedResource() { Unload(); }
    
    // Make resource from raw resource data; the caller gives up ownership of the pointed to memory block
    void SetData(void *data, size_t length);
    
    
    // Resource loaded?
    bool IsLoaded();
    
    // Unloads the resource
    void Unload();
    
    // Get size of loaded resource
    int64_t get_length();
    
    // Get pointer (always present)
    void *GetPointer(bool DoDetach = false);

private:
    // Detaches an allocated resource from this object
    // (keep private to avoid memory leaks)
    void Detach();

public:
    void *p;        // Pointer to resource data (malloc()ed)
    size_t size;    // Size of data
};






// from DataFile
class ResourceFile // TODO: what does this do that DataFile can't? resource forks really should've been converted to standard WAD format (how did M2/Win do it?)
{
    // This class will need to set the refnum and error value appropriately
   // friend class FileSpecifier;
    
public:
    
    
    // Opens either a MacOS resource fork or some imitation of it:
   // bool Open(ResourceFile& OFile, bool Writable=false);
    
    ResourceFile();
    ~ResourceFile() {Close();}    // Auto-close when destroying
    
    
    ao_err open(const ao_path& path)
    {
        Close();
        fh = open_resource_file(path);
        return fh ? no_err : STRID(strERRORS, cantReadFile); // TODO: would be worth defining an enum for commonly used error codes, e.g. errMissingFile, errCantReadFile, errCantWriteFile
    }
    
    bool IsOpen();
    void Close();
    
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

    int GetError() {return err;}

private:
    int err;        // Error code
    SDL_RWops *fh, *saved_f;
};













bool is_applesingle(SDL_RWops *f, bool rsrc_fork, int32 &offset, int32 &length);
bool is_macbinary(SDL_RWops *f, int32 &data_length, int32 &rsrc_length);


// the _1 versions call count on the current iterator's pointee, though why there's an iterator-in-progress in a static var is anyone's guess
size_t count_1_resources(uint32 type);
size_t count_resources(uint32 type);

void get_1_resource_id_list(uint32 type, std::vector<int> &ids);
void get_resource_id_list(uint32 type, std::vector<int> &ids);

bool get_1_resource(uint32 type, int id, LoadedResource &rsrc);
bool get_resource(uint32 type, int id, LoadedResource &rsrc);

bool get_1_ind_resource(uint32 type, int index, LoadedResource &rsrc);
bool get_ind_resource(uint32 type, int index, LoadedResource &rsrc);

bool has_1_resource(uint32 type, int id);
bool has_resource(uint32 type, int id);

void open_m1_external_resources_file(const ao_path& path);


#endif
