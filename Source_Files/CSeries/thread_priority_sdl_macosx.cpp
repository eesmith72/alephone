/*
 *  thread_priority_sdl_macosx.cpp
 *  AlephOne-OSX
 *
 *  Created by woody on Sat Dec 01 2001.
 *
 */

#include "thread_priority_sdl.h"

#include <pthread.h>
#include <sched.h>

bool BoostThreadPriority(SDL_Thread* inThread)
{
    pthread_t theTargetThread = (pthread_t) SDL_GetThreadID(inThread);
    int theSchedulingPolicy;
    sched_param	theSchedulingParameters;
    
    if (pthread_getschedparam(theTargetThread, &theSchedulingPolicy, &theSchedulingParameters) != no_err)
    {
        return false;
    }
    theSchedulingParameters.sched_priority = sched_get_priority_max(theSchedulingPolicy);
    
    return pthread_setschedparam(theTargetThread, theSchedulingPolicy, &theSchedulingParameters) == no_err;
}
