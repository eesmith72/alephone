

#include "app_state.hpp"

#include "vbl.h" // is_vbl_reading_user_inputs



static app_state_t current_state = app_state_t::startup;

static app_state_t next_state = app_state_t::shutdown; // will be set to startup_screen or main_menu by shell's initialize_application


static int64_t time_of_next_transition = 0; // set_next_app_state() sets this to a future time (current machine ticks + ticks until change) at which main event loop should transition from current to next state

static int64_t timeout_duration = 0; // we need to hang onto this value so that losing and regaining window focus on main menu doesn't instantly jump into demo film

// state

void suspend_app_state_timeout()
{
    time_of_next_transition = INFINITE_TIME_DELAY;
}


void restart_app_state_timeout()
{
    time_of_next_transition = machine_tick_count() + timeout_duration;
}


void force_app_state_timeout()
{
    time_of_next_transition = 0;
}


app_state_t advance_app_state()
{
    current_state = next_state;
    return current_state;
}


// Sets the new next app state which the event loop will transition to in N ticks (0 = immediately on next loop). Called at end of initialize_application and in main_event_loop.cpp's transition_to_next_app_state.
void set_next_app_state(app_state_t next_state, uint32_t machine_ticks_until_next_state)
{
    ::next_state = next_state;
    timeout_duration = machine_ticks_until_next_state;
    restart_app_state_timeout();
    
    if (machine_ticks_until_next_state > 0)
        printf("state %02i at %05llu; will transition to %02i at %05lld\n", current_state, machine_tick_count(), next_state, time_of_next_transition);
    else
        printf("state %02i at %05llu; will transition to %02i now\n", current_state, machine_tick_count(), next_state);
}




static int16_t next_level_number = NONE; // TODO: may want to store current level number

void set_next_level_number(int16_t level_number)
{
    assert_fail(current_state == app_state_t::change_level, "");
    next_level_number = level_number;
}


int16_t get_next_level_number()
{
    return next_level_number;
}


void initialize_app_state()
{
    current_state = app_state_t::startup;
    next_state = app_state_t::startup_screen;
    next_level_number = 0;    
}


app_state_t get_app_state()
{
    return current_state;
}


void set_app_state(app_state_t state) // TODO: get rid of this
{
    current_state = state;
}

bool app_state_has_timed_out()
{
    return machine_tick_count() >= time_of_next_transition;
}


void set_app_focus_lost()
{
    if (current_state == app_state_t::main_menu)
    {
        suspend_app_state_timeout(); // suspend switching to demo film mode
    }
    // TODO: what about other states?
}


void set_app_focus_gained()
{
    if (current_state == app_state_t::main_menu)
    {
        restart_app_state_timeout();
    }
}



user_type_t user_type = user_type_t::solo_player;


user_type_t get_user_type()
{
    return user_type;
}

void set_user_type(user_type_t type)
{
    user_type = type;
}

