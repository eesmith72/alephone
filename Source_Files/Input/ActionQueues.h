/*
 ActionQueues.h -- Encapsulates a set of action_queues, so we can have
                   multiple sets and explicitly choose one.
 
 Created by woody on Wed Feb 20 2002.
 
 Copyright (C) 1991-2002 and beyond by Woody Zenfell, III
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

#ifndef	ACTIONQUEUES_H
#define	ACTIONQUEUES_H

#include "cseries.h"


struct ActionQueue
{
    uint32_t read_index, write_index;
    uint32_t* buffer;
};


class ActionQueues
{
public:
    ActionQueues(unsigned int inNumPlayers, unsigned int inQueueSize, bool inZombiesControllable);
    
    void		reset();
    void		resetQueue(int inPlayerIndex);

    void		enqueueActionFlags(int inPlayerIndex, const uint32* inFlags, int inFlagsCount);
    uint32		dequeueActionFlags(int inPlayerIndex);
    uint32		peekActionFlags(int inPlayerIndex, size_t inElementsFromHead);
    unsigned int	countActionFlags(int inPlayerIndex);
    unsigned int	totalCapacity(int inPlayerIndex) { return mQueueSize - 1; }
    unsigned int	availableCapacity(int inPlayerIndex) { return totalCapacity(inPlayerIndex) - countActionFlags(inPlayerIndex); }
    bool		zombiesControllable();
    void		setZombiesControllable(bool inZombiesControllable);
    
    ~ActionQueues();
    
protected:

    uint32_t     mNumPlayers;
    uint32_t     mQueueSize;
    ActionQueue* mQueueHeaders;
    uint32_t*    mFlagsBuffer;
    bool         mZombiesControllable;

// Hide these until they have valid implementation
private:
    ActionQueues(ActionQueues&);
    ActionQueues& operator =(ActionQueues&);
};

class ModifiableActionQueues : public ActionQueues {
public:
	ModifiableActionQueues(unsigned int inNumPlayers, unsigned int inQueueSize, bool inZombiesControllable) : ActionQueues(inNumPlayers, inQueueSize, inZombiesControllable) { }

	// modifies action flags at the head of the queue
	void modifyActionFlags(int inPlayerIndex, uint32 inFlags, uint32 inFlagsMask);
};

#endif // ACTIONQUEUES_H
