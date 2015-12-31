// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2009-2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stdarg.h>

#ifndef WIN32
// for posix_fallocate
#ifdef __linux_

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#define _POSIX_C_SOURCE 200112L
#include <sys/prctl.h>

#endif

#include <algorithm>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>

#else

#ifdef _MSC_VER
#pragma warning(disable:4786)
#pragma warning(disable:4804)
#pragma warning(disable:4805)
#pragma warning(disable:4717)
#endif

#include <io.h> /* for _commit */
#include <shlobj.h>
#endif

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options/detail/config_file.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/thread/once.hpp>

#include "include_base_utils.h"

#include "common/command_line.h"
#include "common/util.h"
#include "crypto/hash.h"
#include "cryptonote_config.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "daemon/daemon_options.h"

#include "interface/qtversion.h"
#include "sync.h"
#include "qt_options.h"
#include "util.h"

using namespace epee;

// Work around clang compilation problem in Boost 1.46:
// /usr/include/boost/program_options/detail/config_file.hpp:163:17: error: call to function 'to_internal' that is neither visible in the template definition nor found by argument-dependent lookup
// See also: http://stackoverflow.com/questions/10020179/compilation-fail-in-boost-librairies-program-options
//           http://clang.debian.net/status.php?version=3.0&key=CANNOT_FIND_FUNCTION
namespace boost {
    namespace program_options {
        std::string to_internal(const std::string&);
    }
}

using namespace std;
namespace po = boost::program_options;

//map<string, string> mapArgs;
//map<string, vector<string> > mapMultiArgs;
po::variables_map vmapArgs;
bool fDebug = false;
bool fPrintToConsole = false;
bool fPrintToDebugLog = true;
string strMiscWarning;
bool fLogTimestamps = false;
volatile bool fReopenDebugLog = false;

// LogPrintf() has been broken a couple of times now
// by well-meaning people adding mutexes in the most straightforward way.
// It breaks because it may be called by global destructors during shutdown.
// Since the order of destruction of static/global objects is undefined,
// defining a mutex as a global object doesn't work (the mutex gets
// destroyed, and then some later destructor calls OutputDebugStringF,
// maybe indirectly, and you get a core dump at shutdown trying to lock
// the mutex).

static boost::once_flag debugPrintInitFlag = BOOST_ONCE_INIT;
// We use boost::call_once() to make sure these are initialized in
// in a thread-safe manner the first time it is called:
static FILE* fileout = NULL;
static boost::mutex* mutexDebugLog = NULL;

static void DebugPrintInit()
{
    assert(fileout == NULL);
    assert(mutexDebugLog == NULL);

    boost::filesystem::path pathDebug = GetDataDir() / "debug.log";
    fileout = fopen(pathDebug.string().c_str(), "a");
    if (fileout) setbuf(fileout, NULL); // unbuffered

    mutexDebugLog = new boost::mutex();
}

bool LogAcceptCategory(const char* category)
{
    /*if (category != NULL)
    {
        if (!fDebug)
            return false;

        // Give each thread quick access to -debug settings.
        // This helps prevent issues debugging global destructors,
        // where mapMultiArgs might be deleted before another
        // global destructor calls LogPrint()
        static boost::thread_specific_ptr<set<string> > ptrCategory;
        if (ptrCategory.get() == NULL)
        {
            const vector<string>& categories = mapMultiArgs["-debug"];
            ptrCategory.reset(new set<string>(categories.begin(), categories.end()));
            // thread_specific_ptr automatically deletes the set when the thread ends.
        }
        const set<string>& setCategories = *ptrCategory.get();

        // if not debugging everything and not debugging specific category, LogPrint does nothing.
        if (setCategories.count(string("")) == 0 &&
            setCategories.count(string(category)) == 0)
            return false;
    }*/
    return true;
}

int LogPrintStr(const std::string &str)
{
    LOG_PRINT_L0(str);
    
    int ret = 0; // Returns total number of characters written
    if (fPrintToConsole)
    {
        // print to console
        ret = fwrite(str.data(), 1, str.size(), stdout);
    }
    else if (fPrintToDebugLog)
    {
        static bool fStartedNewLine = true;
        boost::call_once(&DebugPrintInit, debugPrintInitFlag);

        if (fileout == NULL)
            return ret;

        boost::mutex::scoped_lock scoped_lock(*mutexDebugLog);

        // reopen the log file, if requested
        if (fReopenDebugLog) {
            fReopenDebugLog = false;
            boost::filesystem::path pathDebug = GetDataDir() / "debug.log";
            if (freopen(pathDebug.string().c_str(),"a",fileout) != NULL)
                setbuf(fileout, NULL); // unbuffered
        }

        // Debug print useful for profiling
        if (fLogTimestamps && fStartedNewLine)
            ret += fprintf(fileout, "%s ", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", GetTime()).c_str());
        if (!str.empty() && str[str.size()-1] == '\n')
            fStartedNewLine = true;
        else
            fStartedNewLine = false;

        ret = fwrite(str.data(), 1, str.size(), fileout);
    }

    return ret;
}

string EncodeBase64(const unsigned char* pch, size_t len)
{
    static const char *pbase64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    string strRet="";
    strRet.reserve((len+2)/3*4);

    int mode=0, left=0;
    const unsigned char *pchEnd = pch+len;

    while (pch<pchEnd)
    {
        int enc = *(pch++);
        switch (mode)
        {
            case 0: // we have no bits
                strRet += pbase64[enc >> 2];
                left = (enc & 3) << 4;
                mode = 1;
                break;

            case 1: // we have two bits
                strRet += pbase64[left | (enc >> 4)];
                left = (enc & 15) << 2;
                mode = 2;
                break;

            case 2: // we have four bits
                strRet += pbase64[left | (enc >> 6)];
                strRet += pbase64[enc & 63];
                mode = 0;
                break;
        }
    }

    if (mode)
    {
        strRet += pbase64[left];
        strRet += '=';
        if (mode == 1)
            strRet += '=';
    }

    return strRet;
}

string EncodeBase64(const string& str)
{
    return EncodeBase64((const unsigned char*)str.c_str(), str.size());
}

vector<unsigned char> DecodeBase64(const char* p, bool* pfInvalid)
{
    static const int decode64_table[256] =
    {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1,
        -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28,
        29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
        49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
    };

    if (pfInvalid)
        *pfInvalid = false;

    vector<unsigned char> vchRet;
    vchRet.reserve(strlen(p)*3/4);

    int mode = 0;
    int left = 0;

    while (1)
    {
         int dec = decode64_table[(unsigned char)*p];
         if (dec == -1) break;
         p++;
         switch (mode)
         {
             case 0: // we have no bits and get 6
                 left = dec;
                 mode = 1;
                 break;

              case 1: // we have 6 bits and keep 4
                  vchRet.push_back((left<<2) | (dec>>4));
                  left = dec & 15;
                  mode = 2;
                  break;

             case 2: // we have 4 bits and get 6, we keep 2
                 vchRet.push_back((left<<4) | (dec>>2));
                 left = dec & 3;
                 mode = 3;
                 break;

             case 3: // we have 2 bits and get 6
                 vchRet.push_back((left<<6) | dec);
                 mode = 0;
                 break;
         }
    }

    if (pfInvalid)
        switch (mode)
        {
            case 0: // 4n base64 characters processed: ok
                break;

            case 1: // 4n+1 base64 character processed: impossible
                *pfInvalid = true;
                break;

            case 2: // 4n+2 base64 characters processed: require '=='
                if (left || p[0] != '=' || p[1] != '=' || decode64_table[(unsigned char)p[2]] != -1)
                    *pfInvalid = true;
                break;

            case 3: // 4n+3 base64 characters processed: require '='
                if (left || p[0] != '=' || decode64_table[(unsigned char)p[1]] != -1)
                    *pfInvalid = true;
                break;
        }

    return vchRet;
}

string DecodeBase64(const string& str)
{
    vector<unsigned char> vchRet = DecodeBase64(str.c_str());
    return string((const char*)&vchRet[0], vchRet.size());
}

bool ParseParameters(int argc, const char*const argv[], const po::options_description& desc_options)
{
    return command_line::handle_error_helper(desc_options, [&]()
    {
        po::store(po::parse_command_line(argc, argv, desc_options), vmapArgs);
        po::notify(vmapArgs);
        return true;
    });
}


static std::string FormatException(std::exception* pex, const char* pszThread)
{
#ifdef WIN32
    char pszModule[MAX_PATH] = "";
    GetModuleFileNameA(NULL, pszModule, sizeof(pszModule));
#else
    const char* pszModule = "pebblecoin";
#endif
    if (pex)
        return strprintf(
            "EXCEPTION: %s       \n%s       \n%s in %s       \n", typeid(*pex).name(), pex->what(), pszModule, pszThread);
    else
        return strprintf(
            "UNKNOWN EXCEPTION       \n%s in %s       \n", pszModule, pszThread);
}

void PrintExceptionContinue(std::exception* pex, const char* pszThread)
{
    std::string message = FormatException(pex, pszThread);
    LogPrintf("\n\n************************\n%s\n", message);
    fprintf(stderr, "\n\n************************\n%s\n", message.c_str());
    strMiscWarning = message;
}

#define MAX_NETWORK_TYPES 4
static boost::filesystem::path pathCached[MAX_NETWORK_TYPES+1]; //CChainParams::MAX_NETWORK_TYPES+1];
static CCriticalSection csPathCached;

const boost::filesystem::path &GetDataDir(bool fNetSpecific)
{
    namespace fs = boost::filesystem;

    LOCK(csPathCached);

    int nNet = MAX_NETWORK_TYPES; //CChainParams::MAX_NETWORK_TYPES;
    if (fNetSpecific) nNet = 0; //Params().NetworkID();

    fs::path &path = pathCached[nNet];

    // This can be called during exceptions by LogPrintf(), so we cache the
    // value so we don't have to do memory allocations after that.
    if (!path.empty())
        return path;

    if (ProvidedArg(command_line::arg_data_dir)) {
        path = fs::system_complete(command_line::get_data_dir(vmapArgs));
        if (!fs::is_directory(path)) {
            return path; // will throw an error with the path
        }
    } else {
        path = tools::get_default_data_dir();
    }
    // No net-specific paths yet
    //if (fNetSpecific)
    //    path /= "DirnameNotImplemented"; //Params().DataDir();

    fs::create_directories(path);

    return path;
}

void ClearDatadirCache()
{
    //std::fill(&pathCached[0], &pathCached[CChainParams::MAX_NETWORK_TYPES+1], boost::filesystem::path());
    std::fill(&pathCached[0], &pathCached[MAX_NETWORK_TYPES+1], boost::filesystem::path());
}

boost::filesystem::path GetConfigFile()
{
    boost::filesystem::path config_path(GetArg(daemon_opt::arg_config_file));
    if (!config_path.has_parent_path())
    {
        config_path = GetDataDir() / config_path;
    }
  
    return config_path;
}

bool ReadConfigFile(const boost::program_options::options_description& desc_options)
{
    return command_line::handle_error_helper(desc_options, [&]()
    {
        boost::filesystem::path config_path = GetConfigFile();
        boost::system::error_code ec;
        if (boost::filesystem::exists(config_path, ec))
        {
            po::store(po::parse_config_file<char>(config_path.string<std::string>().c_str(), desc_options), vmapArgs);
        }
        po::notify(vmapArgs);
      
        ClearDatadirCache();
      
        return true;
    });
}

boost::filesystem::path GetIrcConfigFile()
{
    boost::filesystem::path pathIrcConfigFile("irc.ini");
    if (!pathIrcConfigFile.is_complete())
    {
        pathIrcConfigFile = GetDataDir() / pathIrcConfigFile;
    }
    return pathIrcConfigFile;
}

boost::filesystem::path GetWalletFile()
{
    boost::filesystem::path data_dir_path = GetDataDir(true);
    boost::filesystem::path wallet_path = GetArg(qt_opt::arg_wallet_file);
    if (!wallet_path.has_parent_path())
    {
        wallet_path = data_dir_path / wallet_path;
    }
    
    return wallet_path;
}

// Ignores exceptions thrown by boost's create_directory if the requested directory exists.
//   Specifically handles case where path p exists, but it wasn't possible for the user to write to the parent directory.
bool TryCreateDirectory(const boost::filesystem::path& p)
{
    try
    {
        return boost::filesystem::create_directory(p);
    } catch (boost::filesystem::filesystem_error) {
        if (!boost::filesystem::exists(p) || !boost::filesystem::is_directory(p))
            throw;
    }

    // create_directory didn't create the directory, it had to have existed already
    return false;
}

//
// "Never go to sea with two chronometers; take one or three."
// Our three time sources are:
//  - System clock
//  - Median of other nodes clocks
//  - The user (asking the user to fix the system clock if the first two disagree)
//
static int64_t nMockTime = 0;  // For unit testing

int64_t GetTime()
{
    if (nMockTime) return nMockTime;

    return time(NULL);
}

static CCriticalSection cs_nTimeOffset;
static int64_t nTimeOffset = 0;

int64_t GetTimeOffset()
{
    LOCK(cs_nTimeOffset);
    return nTimeOffset;
}

int64_t GetAdjustedTime()
{
    return GetTime() + GetTimeOffset();
}

string FormatVersion(int nVersion)
{
    if (nVersion%100 == 0)
        return strprintf("%d.%d.%d", nVersion/1000000, (nVersion/10000)%100, (nVersion/100)%100);
    else
        return strprintf("%d.%d.%d.%d", nVersion/1000000, (nVersion/10000)%100, (nVersion/100)%100, nVersion%100);
}

string FormatFullVersion()
{
    return CLIENT_BUILD;
}

// Format the subversion field according to BIP 14 spec (https://en.bitcoin.it/wiki/BIP_0014)
std::string FormatSubVersion(const std::string& name, int nClientVersion, const std::vector<std::string>& comments)
{
    std::ostringstream ss;
    ss << "/";
    ss << name << ":" << FormatVersion(nClientVersion);
    if (!comments.empty())
        ss << "(" << boost::algorithm::join(comments, "; ") << ")";
    ss << "/";
    return ss.str();
}

void SetupEnvironment()
{
    #ifndef WIN32
    try
    {
	#if BOOST_FILESYSTEM_VERSION == 3
            boost::filesystem::path::codecvt(); // Raises runtime error if current locale is invalid
	#else				          // boost filesystem v2
            std::locale();                      // Raises runtime error if current locale is invalid
	#endif
    } catch(std::runtime_error &e)
    {
        setenv("LC_ALL", "C", 1); // Force C locale
    }
    #endif
  
#ifdef WIN32
  _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif
}

std::string DateTimeStrFormat(const char* pszFormat, int64_t nTime)
{
    // std::locale takes ownership of the pointer
    std::locale loc(std::locale::classic(), new boost::posix_time::time_facet(pszFormat));
    std::stringstream ss;
    ss.imbue(loc);
    ss << boost::posix_time::from_time_t(nTime);
    return ss.str();
}

std::string GetStrHash(const crypto::hash& h)
{
    return epee::string_tools::pod_to_hex(h);
}

crypto::hash GetCryptoHash(const std::string& s)
{
    crypto::hash h;
    epee::string_tools::hex_to_pod(s, h);
    return h;
}

crypto::hash GetCryptoHash(const cryptonote::transaction& tx)
{
    crypto::hash tx_hash;
    cryptonote::get_transaction_hash(tx, tx_hash);
    return tx_hash;
}

std::string GetStrHash(const cryptonote::transaction& tx)
{
    return GetStrHash(GetCryptoHash(tx));
}

