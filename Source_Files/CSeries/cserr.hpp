/*
 cserr.hpp
 
 Copyright (C) 2026- the "Aleph One" developers.
 
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

#ifndef cserr_hpp
#define cserr_hpp

// TODO: create 'csbase.hpp' which is a mini-umbrella header that consolidates cstypes.h, cserr.hpp, cslog.hpp; other CSeries files should then #include "csbase.hpp", and "cseries.h" then exports the lot to the rest of AO

// TODO: consolidate strERRORS, strNETWORK_ERRORS and their enums here? (if so, where to put their strings?) we should always use predefined error codes when raising exceptions/returning errors; however, errors raised in low-level systems should not require string_resources to generate their error messages (string_resources errors are intended for high-level user reporting of user-addressable problems like missing scenario files)

// TODO: it might be nice to have bug reporting a bit better integrated with the AO repo, e.g. AO can generate a basic error description, display it in a dialog/export it to file, with a button to open AO's bug reporter in the user's web browser so they can open a new ticket and copy-paste that error info into it

// TODO: while the assert macros are only available in DEBUG, how should non-asserts be treated?

// TODO: in release, it might be an idea if assert macros just log on failure; that way there's a log report of problems for external troubleshooting if the failed issue (which is presumbably an AO implementation bug, or a data file corruption caused by one) causes breakage further down the line



// TODO: relocate the low-level logging stuff into cslog.hpp/cpp (if practical)

// enables/disables the log_LEVEL[_f] macros
#define DEBUG


// cserr starts to consolidate AO's chaos of logging/reporting/debugging/asserting/aborting mechanisms. A new general reporting framework can be extracted later.


// TODO: find exit(...) calls in code and generally replace with something more appropriate (in the rare case where `exit` is essentially, e.g. if a `malloc` or other memory call fails, then those are best wrapped in inline functions that do both, or else just assume they will never fail in practice and do as everyone else does nowadays and not bother checking them)



#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#include <cstdarg>
#include <exception>
#include <string>





typedef uint32_t aoerr; // (string_key_t aliases this later so we can define human-readable error messages using standard string resources)


// -----------------------------------------------------------------------------------------
// AOException -- Not as clumsy or as random as the old vhalt, but an elegant weapon for a more civilized age.
//
// While any function could catch and handle exceptions, for now it's best to let AOException propagate to main.cpp,
// which can catch and report the problem, then shut down the application.


// To throw an AOException:
//
//   throw_ao_exception(format_string, error_code, ...)
//
// First argument MUST be a C string with printf-style formatting syntax.
// Second argument MUST be uint32 error code (int32 is fine, but hard to read if -ve).
// Any additional arguments must match %-codes in format string.
// Stick to ASCII. All-caps is fine, possibly recommended as it's typically reporting corrupt data files or internal bugs.
// The formatted string is limited to 511 ASCII chars or less.



// TODO: does `throw EXCEPTION` capture func/file/stack info? if so, don't need to capture it in these macros; if not, all these macros need to capture it as ivars

#define throw_ao_exception(format, err, ...) \
{ \
    std::string tmp; \
    tmp.resize(AO_EXCEPTION_STRING_MAX); \
    snprintf(tmp.data(), tmp.size(), ("ERROR %04x: " #format), (err), __VA_ARGS__); \
    throw AOException((aoerr)(err), tmp); \
}


#define throw_bug_report(format, ...) \
{ \
    throw_ao_exception("%s in %s: " format, STRING_KEY(strDEBUG, db_found_a_bug), __func__, __FILE__, __VA_ARGS__); \
}


#define TODO(message) \
{ \
    throw_ao_exception("%s in %s: TODO: %s", STRING_KEY(strDEBUG, db_todo), __func__, __FILE__, (message)); \
}


// TODO: temporarily disabled for simple implementation below to get AO to build; straighten out later
/*
class AOException : public std::runtime_error
{
public:
    enum
    {
        Bob,
        InternalBug,
        CorruptData,
    };
    
    const strings_t descriptions = {
        "They’re Everywhere!!!",
        "Looks like an AO bug. Please report it.",
        "Looks like a corrupt scenario file. Please report it.",
    };
    
    AOException(aoerr code, const std::string& what = "") : std::runtime_error(what), m_code(code) {}
  
    
    AOException(const std::string& what) : std::runtime_error(what), m_code(5) {}
    
    //AOException(const AOException& exc) : std::runtime_error(exc.what()), m_code(exc.code()) {}
    
    AOException(aoerr code, const std::string& arg) : std::runtime_error(arg), m_code(code) {}

    
    aoerr code() const { return m_code; }
    
private:
    aoerr m_code;
};
*/

class AOException : public std::runtime_error
{
public:
    AOException(aoerr code, const std::string& arg) : std::runtime_error(arg) { }
};



#define strDEBUG (66)
enum {
    db_hello_bob,
    db_found_a_bug,
    db_todo,
    db_insecure_lua, // TODO: we need a proper Warning level for messages like this one
    // whatever else you want: stats, quotes, easter eggs, etc; remember to update  as well
};


#define errAOERROR               (101)

#define AO_ERROR(code)           ((((uint32_t)errAOERROR) << 16) | ((uint16_t)(code)))

#define AO_EXCEPTION_STRING_MAX  (512)


// -----------------------------------------------------------------------------------------
// asserts, debug logging, on-screen reporting
//
// This is low-level debugging tools so sticking with previous printf-style assert/warn macros.
//
// Main difference now is warn prints to to screen and assert throws AOException. (These may change in future.)
//



#define DEBUG_MESSAGE_MAX_SIZE  (512)

// stderr logging

#define stderr_print(message)        (fprintf(stderr, "%s in %s: %s"       , __func__, __FILE__, (message)))
#define stderr_print_f(format, ...)  (fprintf(stderr, ("%s in %s: " format), __func__, __FILE__, __VA_ARGS__))


// on-screen reporting

void screen_print(const std::string& s); // implemented in screen_shared.h as it uses high-level drawing // TODO: use callback hooks

#define screen_print_f(format, ...) \
{ \
    char ao__tmp__[DEBUG_MESSAGE_MAX_SIZE]; \
    snprintf(ao__tmp__, sizeof(ao__tmp__), (format), __VA_ARGS__); \
    screen_print(ao__tmp__); \
}



#ifdef DEBUG

// assertions

// TODO: why is assert_warn[_f] using screen_print instead of stderr?

#define assert_warn(assertion, message) \
{ \
    if (!(assertion)) \
    { \
        static char ao__tmp__[DEBUG_MESSAGE_MAX_SIZE]; \
        snprintf(ao__tmp__, sizeof(ao__tmp__), ("%s in %s: failed assertion (%s): %s"), __func__, __FILE__, #assertion, (message)); \
        screen_print(ao__tmp__); \
    } \
}

#define assert_warn_f(assertion, format, ...) \
{ \
    if (!(assertion)) \
    { \
        static char ao__tmp__[DEBUG_MESSAGE_MAX_SIZE]; \
        snprintf(ao__tmp__, sizeof(ao__tmp__), ("%s in %s: failed assertion (%s): " format), __func__, __FILE__, #assertion, __VA_ARGS__); \
        screen_print(ao__tmp__); \
    } \
}

#define assert_fail(assertion, message) \
{ \
    if (!(assertion)) { throw_bug_report(("%s in %s: failed assertion (%s): %s"), __func__, __FILE__, #assertion, (message)); } \
}

#define assert_fail_f(assertion, format, ...) \
{ \
    if (!(assertion)) { throw_bug_report(("%s in %s: failed assertion(%s): " format), #assertion, __VA_ARGS__); } \
}

#else // !DEBUG

// reporting (mostly debugging, except screen_print which is general notification also used for user messages)

#define screen_print(message)              ((void)0)
#define screen_print_f(format, ...)        ((void)0)
#define stderr_print(message)              ((void)0)
#define stderr_print_f(format, ...)        ((void)0)

// assertions

#define assert_warn(what)                  ((void)0)
#define assert_warn_f(what, message, ...)  ((void)0)
#define assert_fail(what)                  ((void)0)
#define assert_fail_f(what, message, ...)  ((void)0)

#endif // !DEBUG


// -----------------------------------------------------------------------------------------
// logging macros; these replace the Logging.h macros (logError, etc)
//
// These are relatively low level, using C strings and printf, so are for logging problems that a user can't deal with in the app, e.g. AO bugs, corrupted data, problems detected during startup and shutdown (esp. before/after the high-level reporting systems are available). C strings may be assumed to be UTF8-encoded, though this cannot be guaranteed[1].
//
// User-resolvable issues, e.g. missing scenario files, should be reported using the high-level reporting `alert_user` API (in csalerts.hpp), which has access to string_resources and can display on-screen dialogs (full unicode, i10ns)
//
// [1] AO's overhauled string handling *should* always work with UTF-encoded std::strings, but it's possible old MacRoman-encoded data might sneak through in places. While low-bit MacRoman chars are just ASCII, any high-bit MacRoman chars are NOT UTF8-compatible so those will appear as corrupted UTF8; in which case the resulting logs may need to be opened as MacRoman instead of UTF8 to be nominally readable[2].
//
// [2] (While it'd be nice to include a sanitize_utf_string that replaces invalid UTF8 char sequences with "[BADCHAR: 0xXX...]", that might require reporting to run on a background thread instead of the performance-sensitive graphics main thread; which creates extra complexity - something 'low-level' reporting doesn't want.)


#define log_fatal(message)          (stderr_print_f("FATAL:   %s"     , (message)  ))
#define log_fatal_f(format, ...)    (stderr_print_f("FATAL:   " format, __VA_ARGS__))
#define log_error(message)          (stderr_print_f("ERROR:   %s"     , message    ))
#define log_error_f(format, ...)    (stderr_print_f("ERROR:   " format, __VA_ARGS__))
#define log_warning(message)        (stderr_print_f("WARN:    %s"     , (message)  ))
#define log_warning_f(format, ...)  (stderr_print_f("WARN:    " format, __VA_ARGS__))
#define log_anomaly(message)        (stderr_print_f("ANOMALY: %s"     , (message)  )) /* gaseous */
#define log_anomaly_f(format, ...)  (stderr_print_f("ANOMALY: " format, __VA_ARGS__)) /* I think we call those "bugs", Freeman */
#define log_note(message)           (stderr_print_f("NOTE:    %s"     , (message)  ))
#define log_note_f(format, ...)     (stderr_print_f("NOTE:    " format, __VA_ARGS__))
#define log_trace(message)          (stderr_print_f("TRACE:   %s"     , (message)  ))
#define log_trace_f(format, ...)    (stderr_print_f("TRACE:   " format, __VA_ARGS__))
#define log_dump(message)           (stderr_print_f("DUMP:    %s"     , (message)  ))
#define log_dump_f(format, ...)     (stderr_print_f("DUMP:    " format, __VA_ARGS__))

#define log_context(message)        {}
#define log_context_f(format, ...)  {}




#endif /* cserr_hpp */
