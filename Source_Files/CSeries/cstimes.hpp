/*

	Copyright (C) 1991-2001 and beyond by Bo Lindbergh
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
#ifndef _CSERIES_MISC_
#define _CSERIES_MISC_

#include "cstypes.hpp"

//-----------------------------------------------------------------------------
// from csmisc.h


#define MACHINE_TICKS_PER_SECOND    (1000)
#define TICKS_PER_SECOND            (30)
#define TICKS_PER_MINUTE            (60 * TICKS_PER_SECOND)


extern uint64_t machine_tick_count();

extern void sleep_for_machine_ticks(uint32 ticks);

extern void sleep_until_machine_tick_count(uint64_t ticks);

extern void yield();

extern bool wait_for_click_or_keypress(uint32 ticks);


//-----------------------------------------------------------------------------
// moved here from mytm.h

// ZZZ: call before any mytm routines
void initialize_timing();


typedef struct myTMTask* myTMTaskPtr;

typedef bool (*mytm_proc)();


myTMTaskPtr myXTMSetup(int32 time, mytm_proc func);

void myTMRemove(myTMTaskPtr task);

// ZZZ: call this from time to time to collect leftover zombie threads and reclaim a little storage.
// Pass false for fairly quick operation.  Pass true to make sure that we wait for folks to finish.
void myTMCleanup();

// ZZZ: Use these for mutually exclusive operation with any emulated TMTasks
bool take_mytm_mutex();
bool release_mytm_mutex();


// ghs: exception-safe version of take_/release_mytm_mutex above // TODO: any reason code which still uses the old functions hasn't been updated?
class MyTMMutexTaker
{
public :
    MyTMMutexTaker() { m_release = take_mytm_mutex(); }
    ~MyTMMutexTaker() { if (m_release) release_mytm_mutex(); }
private:
    bool m_release;
};

#endif
