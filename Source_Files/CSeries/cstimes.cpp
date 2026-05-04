/*
 csmisc.cpp - Miscellaneous routines, SDL implementation
 
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

#include "cstimes.hpp"

#include "cserr.hpp"
#include "thread_priority.hpp"

#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_error.h>



static const auto epoch = std::chrono::steady_clock::now();

// TODO: an option to play the game at, say, 110% speed might be a welcome gameplay feature for modern gamers (alternatively an "Overdrive" might be better implemented as a selective Physics model adjustment, e.g. so player, monsters, and projectiles move faster but platforms and effects still run at standard speed)
/* a knob to play the game in "slow motion" to debug timing sensitive features.
   this is not a preferences option because of the cheating potential, and
   because of the awesome breakage that will occur at very large values */
static constexpr int TIME_SKEW = 1;

/*
 *  Return tick counter
 */

uint64_t machine_tick_count()
{
	const auto now = std::chrono::steady_clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(now - epoch).count() / TIME_SKEW;
}

/*
 *  Delay a certain number of ticks
 */

void sleep_for_machine_ticks(uint32 ticks)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(ticks*TIME_SKEW));
}

/*
 *  Delay until a certain tick count
 */

void sleep_until_machine_tick_count(uint64_t ticks)
{
	std::this_thread::sleep_until(std::chrono::steady_clock::time_point(std::chrono::milliseconds(ticks*TIME_SKEW)));
}

/*
 *  Give up a small amount of processor time
 */
void yield()
{
	std::this_thread::yield();
}

/*
 *  Wait for mouse click or keypress
 */

bool wait_for_click_or_keypress(uint32 ticks)
{
	auto start = machine_tick_count();
	SDL_Event event;
	while (machine_tick_count() - start < ticks) {
		SDL_WaitEventTimeout(&event, ticks);
		switch (event.type) {
			case SDL_MOUSEBUTTONDOWN:
			case SDL_KEYDOWN:
			case SDL_CONTROLLERBUTTONDOWN:
				return true;
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// moved here from mytm.cpp

/*
 The point of this file is to let us (networking code, in particular) use timing services
 with the same source-level API wrapper that Bungie used to access the Mac's Time Manager.
 
 Created by woody on Mon Oct 15 2001.
 
 - 3 December 2001 (Woody Zenfell): changed dependence on SDL_Threadx's SetRelativeThreadPriority
 to simply BoostThreadPriority(), a simpler function with a simpler interface.
 
 - 14 January 2003 (Woody Zenfell): TMTasks lock each other out while running (better models
 Time Manager behavior, so makes code safer).  Also removed missedDeadline stuff.
 
 The implementation is built on SDL_thread, and approximates the Time Manager behavior.
 Obviously, it's not a perfect emulation.  :)
 In particular, though TMTasks now lock one another out (as they should), TMTasks do not
 (cannot?) effectively lock out the main thread (as they would in Mac OS 9)... but, their
 threads ought to be higher-priority than the main thread, which means that as long as they
 don't block (which they shouldn't anyway), the main thread will not run while they do.
 
 I probably would have made life easier for myself by using SDL_timer instead, but frankly
 the documentation does not inspire me to trust it.  I'll do things on my own.
 */


#ifdef DEBUG
struct myTMTask_profile
{
    uint64_t mStartTime;
    uint64_t mFinishTime;
    uint32_t mNumCallsThisReset;
    uint32_t mNumCallsTotal;
    int32_t  mDriftMin;
    int32_t  mDriftMax;
    uint32_t mNumLateCalls;
    uint32_t mNumWarmResets;
    uint32_t mNumResuscitations;
};
#endif


// Housekeeping structure used in setup, teardown, and execution
struct myTMTask
{
    SDL_Thread*      mThread;
    uint32_t         mPeriod;
    mytm_proc        mFunction;
    std::atomic_bool mKeepRunning; // set true by myTMSetup; set false by thread or by myTMRemove.
#ifdef DEBUG
    myTMTask_profile mProfilingData;
#endif
};


// Only one TMTask should be scheduled at any given time, so they take this mutex.
static SDL_mutex* sTMTaskMutex = nullptr;


void initialize_timing()
{
    // XXX should provide a way to destroy the mutex too - currently we rely on process exit to do that.
    if (sTMTaskMutex) { log_anomaly("multiple invocations of initialize_timing()"); }
    
    // TODO: any reason we don't throw?
    sTMTaskMutex = SDL_CreateMutex();
    if (!sTMTaskMutex) log_warning("unable to create mytm mutex lock");
}


// The logging system is not (currently) thread-safe, so these logging calls are potentially a Bad Idea
// but if something's going wrong already, maybe it wouldn't hurt to take a small risk to shed some light.
bool take_mytm_mutex()
{
    bool success = (SDL_LockMutex(sTMTaskMutex) != -1);
    if (!success)
    {
        log_anomaly_f("take_mytm_mutex(): SDL_LockMutex() failed: %s", SDL_GetError());
        SDL_ClearError();
    }
    return success;
}



bool release_mytm_mutex()
{
    bool success = (SDL_UnlockMutex(sTMTaskMutex) != -1);
    if (!success)
    {
        log_anomaly_f("release_mytm_mutex(): SDL_UnlockMutex() failed: %s", SDL_GetError());
        SDL_ClearError();
    }
    return success;
}


// Function that threads execute - does housekeeping and calls user callback. Tries to be drift-free.
static int thread_loop(void* inData)
{
    myTMTask* theTMTask = (myTMTask*)inData;
    
    uint64_t theLastRunTime = machine_tick_count();
    uint64_t theCurrentRunTime;
    int32_t  theDrift = 0;
    
#ifdef DEBUG
    theTMTask->mProfilingData.mStartTime = theLastRunTime;
#endif
    
    while (theTMTask->mKeepRunning)
    {
        // Delay, unless we're at least a period behind schedule
        // Originally, I didn't compute theDelay explicitly as a signed quantity, which
        // made for some VERY long waits if we were running late...
        int32 theDelay = theTMTask->mPeriod - theDrift;
        if(theDelay > 0)
            sleep_for_machine_ticks(theDelay);
#ifdef DEBUG
        else // We missed a deadline!
            theTMTask->mProfilingData.mNumLateCalls++;
#endif
        
        theCurrentRunTime = machine_tick_count();
        theDrift += theCurrentRunTime - theLastRunTime - theTMTask->mPeriod;
        theLastRunTime = theCurrentRunTime;
        
#ifdef DEBUG
        if (theDrift < theTMTask->mProfilingData.mDriftMin)
            theTMTask->mProfilingData.mDriftMin = theDrift;
        if (theDrift > theTMTask->mProfilingData.mDriftMax)
            theTMTask->mProfilingData.mDriftMax = theDrift;
#endif
        
        // Since we've been delayed for a while, double-check that we still want to run.
        if (!theTMTask->mKeepRunning) break;
        
        // NOTE: since we could be preempted between checking for termination and actually calling the
        // callback, there is a VERY small chance that mFunction could be called (at most once) after
        // myTMRemoveTask() completes.  This is a BUG, but to avoid expensive synchronization (making
        // myTMRemoveTask() block until this thread finishes, protecting mKeepRunning with a mutex, etc.)
        // we take our chances.  This bug could only bite anyway (in the current IPring) while making the
        // transition from a normal player to the gatherer (in drop_upring_player()) as a result of the
        // gatherer becoming netdead - not terribly likely to begin with!
        
        // Call the function.  If it doesn't want to be rescheduled, stop ourselves.
#ifdef DEBUG
        theTMTask->mProfilingData.mNumCallsThisReset++;
        theTMTask->mProfilingData.mNumCallsTotal++;
#endif
        
        // Lock out other tmtasks while we run ours
        if (take_mytm_mutex())
        {
            bool runAgain = theTMTask->mFunction();
            release_mytm_mutex();
            if (!runAgain) break;
        }
    }
    
#ifdef DEBUG
    theTMTask->mProfilingData.mFinishTime = machine_tick_count();
#endif
    
    return 0;
}


static std::vector<myTMTaskPtr> sOutstandingTasks;

// Set up a periodic callout, with what tries to be a fairly drift-free period.
myTMTaskPtr myXTMSetup(int32 time, mytm_proc func)
{
    myTMTaskPtr theTask = new myTMTask;
    
    theTask->mPeriod      = time;
    theTask->mFunction    = func;
    theTask->mKeepRunning = true;

#ifdef DEBUG
    memset(&theTask->mProfilingData, 0, sizeof(myTMTask_profile));
#endif
    
    theTask->mThread = SDL_CreateThread(thread_loop, "myXTMSetup_taskThread", theTask);

    // Set thread priority a little higher
    BoostThreadPriority(theTask->mThread);
    
    sOutstandingTasks.push_back(theTask);
    
    return theTask;
}


// Stop an existing callout from executing.
void myTMRemove(myTMTaskPtr task)
{
    if (task) task->mKeepRunning = false;
}


#ifdef DEBUG

// ZZZ addition (to myTM interface): dump profiling data
#define DUMPIT_ZU(structure,field_name) log_dump_f("" #field_name ":\t%u", (unsigned)(structure).field_name)
#define DUMPIT_ZS(structure,field_name) log_dump_f("" #field_name ":\t%d", (int)(structure).field_name)

void myTMDumpProfile(myTMTask* inTask)
{
    if (inTask)
    {
        log_dump_f("PROFILE FOR SDL TMTASK %p (function %p)", inTask, inTask->mFunction);
        DUMPIT_ZU((*inTask), mPeriod);
        DUMPIT_ZU(inTask->mProfilingData, mStartTime);
        DUMPIT_ZU(inTask->mProfilingData, mFinishTime);
        DUMPIT_ZU(inTask->mProfilingData, mNumCallsThisReset);
        DUMPIT_ZU(inTask->mProfilingData, mNumCallsTotal);
        DUMPIT_ZS(inTask->mProfilingData, mDriftMin);
        DUMPIT_ZS(inTask->mProfilingData, mDriftMax);
        DUMPIT_ZU(inTask->mProfilingData, mNumLateCalls);
        DUMPIT_ZU(inTask->mProfilingData, mNumWarmResets);
        DUMPIT_ZU(inTask->mProfilingData, mNumResuscitations);
    }
}

#endif//DEBUG


// ZZZ addition: clean up outstanding timer task blocks and threads
// This could be slightly more efficient maybe by using a list, condensing calls to erase(), etc...
// but why bother?  It's only used occasionally at non-time-critical moments, and we're only dealing with
// a small handful of (small) elements anyway.
void myTMCleanup()
{
    auto i = sOutstandingTasks.begin();
    while (i != sOutstandingTasks.end())
    {
        if (!(*i)->mKeepRunning)
        {
            myTMTaskPtr theDeadTask = *i;
            auto next_i = sOutstandingTasks.erase(i);
            i = next_i;
            
#ifdef DEBUG
            myTMDumpProfile(theDeadTask);
#endif

            SDL_WaitThread(theDeadTask->mThread, NULL);
            delete theDeadTask;
        }
        else
        {
            ++i; // skip task
        }
    }
}
