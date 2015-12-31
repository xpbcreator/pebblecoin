// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2009-2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_H
#define BITCOIN_UTIL_H

#include <stdarg.h>
#include <stdint.h>

#include <string>
#include <exception>
#include <vector>
#include <utility>

#include <boost/filesystem/path.hpp>

#if defined(HAVE_CONFIG_H)
#include "bitcoin-config.h"
#endif

#include "common/types.h"
#include "common/command_line.h"

#include "tinyformat.h"

namespace crypto
{
  POD_CLASS hash;
}

namespace cryptonote
{
  class transaction;
}

#if defined(WIN32)
#elif defined(__MACH__)
#include <sys/syslimits.h>
#define MAX_PATH PATH_MAX
#else
#include <linux/limits.h>
#define MAX_PATH PATH_MAX
#endif

extern boost::program_options::variables_map vmapArgs;
extern bool fDebug;
extern bool fPrintToConsole;
extern bool fPrintToDebugLog;
extern std::string strMiscWarning;
extern bool fLogTimestamps;
extern volatile bool fReopenDebugLog;

void SetupEnvironment();

/* Return true if log accepts specified category */
bool LogAcceptCategory(const char* category);
/* Send a string to the log output */
int LogPrintStr(const std::string &str);

#define strprintf tfm::format
#define LogPrintf(...) LogPrint(NULL, __VA_ARGS__)

/* When we switch to C++11, this can be switched to variadic templates instead
 * of this macro-based construction (see tinyformat.h).
 */
#define MAKE_ERROR_AND_LOG_FUNC(n)                                        \
    /*   Print to debug.log if -debug=category switch is given OR category is NULL. */ \
    template<TINYFORMAT_ARGTYPES(n)>                                          \
    static inline int LogPrint(const char* category, const char* format, TINYFORMAT_VARARGS(n))  \
    {                                                                         \
        if(!LogAcceptCategory(category)) return 0;                            \
        return LogPrintStr(tfm::format(format, TINYFORMAT_PASSARGS(n))); \
    }                                                                         \
    /*   Log error and return false */                                        \
    template<TINYFORMAT_ARGTYPES(n)>                                          \
    static inline bool error(const char* format, TINYFORMAT_VARARGS(n))                     \
    {                                                                         \
        LogPrintStr("ERROR: " + tfm::format(format, TINYFORMAT_PASSARGS(n)) + "\n"); \
        return false;                                                         \
    }

TINYFORMAT_FOREACH_ARGNUM(MAKE_ERROR_AND_LOG_FUNC)

/* Zero-arg versions of logging and error, these are not covered by
 * TINYFORMAT_FOREACH_ARGNUM
 */
static inline int LogPrint(const char* category, const char* format)
{
    if(!LogAcceptCategory(category)) return 0;
    return LogPrintStr(format);
}
static inline bool error(const char* format)
{
    LogPrintStr(std::string("ERROR: ") + format + "\n");
    return false;
}


void PrintExceptionContinue(std::exception* pex, const char* pszThread);
std::vector<unsigned char> DecodeBase64(const char* p, bool* pfInvalid = NULL);
std::string DecodeBase64(const std::string& str);
std::string EncodeBase64(const unsigned char* pch, size_t len);
std::string EncodeBase64(const std::string& str);
bool ParseParameters(int argc, const char*const argv[], const boost::program_options::options_description& desc_options);
bool TryCreateDirectory(const boost::filesystem::path& p);
const boost::filesystem::path &GetDataDir(bool fNetSpecific = true);
boost::filesystem::path GetConfigFile();
boost::filesystem::path GetIrcConfigFile();
boost::filesystem::path GetWalletFile();
bool ReadConfigFile(const boost::program_options::options_description& desc_options);
int64_t GetTime();
int64_t GetAdjustedTime();
int64_t GetTimeOffset();
std::string FormatFullVersion();
std::string FormatSubVersion(const std::string& name, int nClientVersion, const std::vector<std::string>& comments);



inline int atoi(const std::string& str)
{
    return atoi(str.c_str());
}

std::string DateTimeStrFormat(const char* pszFormat, int64_t nTime);

/**
 * Return argument or default value
 *
 * @param arg Argument to get (e.g. command_line::arg_help)
 * @return command-line argument or default value
 */
template<typename T, bool required>
T GetArg(const command_line::arg_descriptor<T, required>& arg)
{
    return command_line::get_arg(vmapArgs, arg);
}

/**
 * Return whether the argument has been set
 *
 * @param arg Argument to check
 * @return whether it was set
 */
template<typename T, bool required>
bool HasArg(const command_line::arg_descriptor<T, required>& arg)
{
    return command_line::has_arg(vmapArgs, arg);
}

/**
 * Return whether the argument has been provided by the user
 *
 * @param arg Argument to check
 * @return whether it was set by the user
 */
template<typename T, bool required>
bool ProvidedArg(const command_line::arg_descriptor<T, required>& arg)
{
    return command_line::has_arg(vmapArgs, arg) && !command_line::has_defaulted_arg(vmapArgs, arg);
}

/**
 * Set an argument if it doesn't already have a value
 *
 * @param arg Argument to set (e.g. command_line::arg_help)
 * @param value Value (e.g. "1")
 * @return true if argument gets set, false if it already had a value
 */
template<typename T, bool required>
bool SoftSetArg(const command_line::arg_descriptor<T, required>& arg, const T& value)
{
    if (command_line::has_arg(vmapArgs, arg))
        return false;
  
    vmapArgs.insert(std::make_pair(arg.name, boost::program_options::variable_value(value, false)));
    boost::program_options::notify(vmapArgs);
  
    return true;
}

std::string GetStrHash(const crypto::hash& h);
crypto::hash GetCryptoHash(const std::string& s);
crypto::hash GetCryptoHash(const cryptonote::transaction& tx);
std::string GetStrHash(const cryptonote::transaction& tx);

#endif
