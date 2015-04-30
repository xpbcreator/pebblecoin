// Copyright (c) 2006-2013, Andrey N. Sabelnikov, www.sabelnikov.net
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// * Neither the name of the Andrey N. Sabelnikov nor the
// names of its contributors may be used to endorse or promote products
// derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER  BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "../include/misc_log_ex.h"
#include "../include/syncobj.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>

namespace epee
{
namespace log_space
{
  /************************************************************************/
  /* file_output_stream                                                   */
  /************************************************************************/
  file_output_stream::file_output_stream( std::string default_log_file_name, std::string log_path )
  {
    m_default_log_filename = default_log_file_name;
    m_max_logfile_size = 0;
    m_default_log_path = log_path;
    m_pdefault_file_stream = add_new_stream_and_open(default_log_file_name.c_str());
  }

  file_output_stream::~file_output_stream()
  {
    for(named_log_streams::iterator it = m_log_file_names.begin(); it!=m_log_file_names.end(); it++)
    {
      if ( it->second->is_open() )
      {
        it->second->flush();
        it->second->close();
      }
      delete it->second;
    }
  }
  
  std::ofstream*    file_output_stream::add_new_stream_and_open(const char* pstream_name)
  {
    //log_space::rotate_log_file((m_default_log_path + "\\" + pstream_name).c_str());

    std::ofstream* pstream = (m_log_file_names[pstream_name] = new std::ofstream);
    std::string target_path = m_default_log_path + "/" + pstream_name;
    pstream->open( target_path.c_str(), std::ios_base::out | std::ios::app /*ios_base::trunc */);
    if(pstream->fail())
      return NULL;
    return pstream;
  }

  bool file_output_stream::set_max_logfile_size(uint64_t max_size)
  {
    m_max_logfile_size = max_size;
    return true;
  }

  bool file_output_stream::set_log_rotate_cmd(const std::string& cmd)
  {
    m_log_rotate_cmd = cmd;
    return true;
  }

  bool file_output_stream::out_buffer( const char* buffer, int buffer_len, int log_level, int color, const char* plog_name )
  {
    std::ofstream*    m_target_file_stream = m_pdefault_file_stream;
    if(plog_name)
    { //find named stream
      named_log_streams::iterator it = m_log_file_names.find(plog_name);
      if(it == m_log_file_names.end())
        m_target_file_stream = add_new_stream_and_open(plog_name);
      else
        m_target_file_stream = it->second;
    }
    if(!m_target_file_stream || !m_target_file_stream->is_open())
      return false;//TODO: add assert here

    m_target_file_stream->write(buffer, buffer_len );
    m_target_file_stream->flush();

    if(m_max_logfile_size)
    {
      std::ofstream::pos_type pt =  m_target_file_stream->tellp();
      uint64_t current_sz = pt;
      if(current_sz > m_max_logfile_size)
      {
        std::cout << "current_sz= " << current_sz << " m_max_logfile_size= " << m_max_logfile_size << std::endl;
        std::string log_file_name;
        if(!plog_name)
          log_file_name = m_default_log_filename;
        else
          log_file_name = plog_name;

        m_target_file_stream->close();
        std::string new_log_file_name = log_file_name;

        time_t tm = 0;
        time(&tm);

        int err_count = 0;
        boost::system::error_code ec;
        do
        {
          new_log_file_name = string_tools::cut_off_extension(log_file_name);
          if(err_count)
            new_log_file_name += misc_utils::get_time_str_v2(tm) + "(" + boost::lexical_cast<std::string>(err_count) + ")" + ".log";
          else
            new_log_file_name += misc_utils::get_time_str_v2(tm) + ".log";

          err_count++;
        }while(boost::filesystem::exists(m_default_log_path + "/" + new_log_file_name, ec));

        std::string new_log_file_path = m_default_log_path + "/" + new_log_file_name;
        boost::filesystem::rename(m_default_log_path + "/" + log_file_name, new_log_file_path, ec);
        if(ec)
        {
          std::cout << "Filed to rename, ec = " << ec.message() << std::endl;
        }

        if(m_log_rotate_cmd.size())
        {

          std::string m_log_rotate_cmd_local_copy = m_log_rotate_cmd;
          //boost::replace_all(m_log_rotate_cmd, "[*SOURCE*]", new_log_file_path);
          boost::replace_all(m_log_rotate_cmd_local_copy, "[*TARGET*]", new_log_file_path);

          misc_utils::call_sys_cmd(m_log_rotate_cmd_local_copy);
        }

        m_target_file_stream->open( (m_default_log_path + "/" + log_file_name).c_str(), std::ios_base::out | std::ios::app /*ios_base::trunc */);
        if(m_target_file_stream->fail())
          return false;
      }
    }

    return  true;
  }
  
  int file_output_stream::get_type(){return LOGGER_FILE;}
  
  /************************************************************************/
  /* logger                                                               */
  /************************************************************************/
  std::string get_daytime_string2()
  {
    boost::posix_time::ptime p = boost::posix_time::microsec_clock::local_time();
    return misc_utils::get_time_str_v3(p);
  }
  std::string get_day_time_string()
  {
    return get_daytime_string2();
    //time_t tm = 0;
    //time(&tm);
    //return  misc_utils::get_time_str(tm);
  }
  std::string get_time_string()
  {
    return get_daytime_string2();
  }

  /************************************************************************/
  /* logger                                                               */
  /************************************************************************/
  logger::logger() : m_critical_sec("logger::m_critical_sec")
  {
    CRITICAL_REGION_BEGIN(m_critical_sec);
    init();
    CRITICAL_REGION_END();
  }
  logger::~logger()
  {
  }

  bool logger::set_max_logfile_size(uint64_t max_size)
  {
    CRITICAL_REGION_BEGIN(m_critical_sec);
    m_log_target.set_max_logfile_size(max_size);
    CRITICAL_REGION_END();
    return true;
  }

  bool logger::set_log_rotate_cmd(const std::string& cmd)
  {
    CRITICAL_REGION_BEGIN(m_critical_sec);
    m_log_target.set_log_rotate_cmd(cmd);
    CRITICAL_REGION_END();
    return true;
  }

  bool logger::take_away_journal(std::list<std::string>& journal)
  {
    CRITICAL_REGION_BEGIN(m_critical_sec);
    m_journal.swap(journal);
    CRITICAL_REGION_END();
    return true;
  }

  bool logger::do_log_message(const std::string& rlog_mes, int log_level, int color, bool add_to_journal, const char* plog_name)
  {
    CRITICAL_REGION_BEGIN(m_critical_sec);
    m_log_target.do_log_message(rlog_mes, log_level, color, plog_name);
    if(add_to_journal)
      m_journal.push_back(rlog_mes);

    return true;
    CRITICAL_REGION_END();
  }

  bool logger::add_logger( int type, const char* pdefault_file_name, const char* pdefault_log_folder , int log_level_limit)
  {
    CRITICAL_REGION_BEGIN(m_critical_sec);
    return m_log_target.add_logger( type, pdefault_file_name, pdefault_log_folder, log_level_limit);
    CRITICAL_REGION_END();
  }
  bool logger::add_logger( ibase_log_stream* pstream, int log_level_limit)
  {
    CRITICAL_REGION_BEGIN(m_critical_sec);
    return m_log_target.add_logger(pstream, log_level_limit);
    CRITICAL_REGION_END();
  }

  bool logger::remove_logger(int type)
  {
    CRITICAL_REGION_BEGIN(m_critical_sec);
    return m_log_target.remove_logger(type);
    CRITICAL_REGION_END();
  }


  bool logger::set_thread_prefix(const std::string& prefix)
  {
    CRITICAL_REGION_BEGIN(m_critical_sec);
    m_thr_prefix_strings[misc_utils::get_thread_string_id()] = prefix;
    CRITICAL_REGION_END();
    return true;
  }


  std::string logger::get_default_log_file()
  {
    return m_default_log_file;
  }

  std::string logger::get_default_log_folder()
  {
    return m_default_log_folder;
  }

  bool logger::init()
  {
    //

    m_process_name = string_tools::get_current_module_name();

    init_log_path_by_default();

    //init default set of loggers
    init_default_loggers();

    std::stringstream ss;
    ss  << get_time_string() << " Init logging. Level=" << get_set_log_detalisation_level()
      << " Log path=" << m_default_log_folder << std::endl;
    this->do_log_message(ss.str(), console_color_white, LOG_LEVEL_0);
    return true;
  }
  bool logger::init_default_loggers()
  {
    //TODO:
    return true;
  }

  bool logger::init_log_path_by_default()
  {
    //load process name
    m_default_log_folder = string_tools::get_current_module_folder();

    m_default_log_file =  m_process_name;
    std::string::size_type a = m_default_log_file.rfind('.');
    if ( a != std::string::npos )
      m_default_log_file.erase( a, m_default_log_file.size());
    m_default_log_file += ".log";

    return true;
  }

  /************************************************************************/
  /* log_singletone                                                       */
  /************************************************************************/
  int log_singletone::get_log_detalisation_level()
  {
    get_or_create_instance();//to initialize logger, if it not initialized
    return get_set_log_detalisation_level();
  }

  bool log_singletone::is_filter_error(int error_code)
  {
    return false;
  }

  bool log_singletone::do_log_message(const std::string& rlog_mes, int log_level, int color, bool keep_in_journal, const char* plog_name)
  {
    logger* plogger = get_or_create_instance();
    bool res = false;
    if(plogger)
      res = plogger->do_log_message(rlog_mes, log_level, color, keep_in_journal, plog_name);
    else
    { //globally uninitialized, create new logger for each call of do_log_message() and then delete it
      plogger = new logger();
      //TODO: some extra initialization
      res = plogger->do_log_message(rlog_mes, log_level, color, keep_in_journal, plog_name);
      delete plogger;
      plogger = NULL;
    }
    return res;
  }

  bool log_singletone::take_away_journal(std::list<std::string>& journal)
  {
    logger* plogger = get_or_create_instance();
    bool res = false;
    if(plogger)
      res = plogger->take_away_journal(journal);

    return res;
  }

  bool log_singletone::set_max_logfile_size(uint64_t file_size)
  {
    logger* plogger = get_or_create_instance();
    if(!plogger) return false;
    return plogger->set_max_logfile_size(file_size);
  }


  bool log_singletone::set_log_rotate_cmd(const std::string& cmd)
  {
    logger* plogger = get_or_create_instance();
    if(!plogger) return false;
    return plogger->set_log_rotate_cmd(cmd);
  }

  bool log_singletone::add_logger( int type, const char* pdefault_file_name, const char* pdefault_log_folder, int log_level_limit)
  {
    logger* plogger = get_or_create_instance();
    if(!plogger) return false;
    return plogger->add_logger(type, pdefault_file_name, pdefault_log_folder, log_level_limit);
  }

  std::string log_singletone::get_default_log_file()
  {
    logger* plogger = get_or_create_instance();
    if(plogger)
      return plogger->get_default_log_file();

    return "";
  }

  std::string log_singletone::get_default_log_folder()
  {
    logger* plogger = get_or_create_instance();
    if(plogger)
      return plogger->get_default_log_folder();

    return "";
  }

  bool log_singletone::add_logger( ibase_log_stream* pstream, int log_level_limit)
  {
    logger* plogger = get_or_create_instance();
    if(!plogger) return false;
    return plogger->add_logger(pstream, log_level_limit);
  }

  bool log_singletone::remove_logger( int type )
  {
    logger* plogger = get_or_create_instance();
    if(!plogger) return false;
    return plogger->remove_logger(type);
  }
PUSH_WARNINGS
DISABLE_GCC_WARNING(maybe-uninitialized)
  int log_singletone::get_set_log_detalisation_level(bool is_need_set, int log_level_to_set)
  {
    static int log_detalisation_level = LOG_LEVEL_1;
    if(is_need_set)
      log_detalisation_level = log_level_to_set;
    return log_detalisation_level;
  }
POP_WARNINGS
  int log_singletone::get_set_time_level(bool is_need_set, int time_log_level)
  {
    static int val_time_log_level = LOG_LEVEL_0;
    if(is_need_set)
      val_time_log_level = time_log_level;

    return val_time_log_level;
  }

  int log_singletone::get_set_process_level(bool is_need_set, int process_log_level)
  {
    static int val_process_log_level = LOG_LEVEL_0;
    if(is_need_set)
      val_process_log_level = process_log_level;

    return val_process_log_level;
  }

  /*static int  get_set_tid_level(bool is_need_set = false, int tid_log_level = LOG_LEVEL_0)
  {
    static int val_tid_log_level = LOG_LEVEL_0;
    if(is_need_set)
      val_tid_log_level = tid_log_level;

    return val_tid_log_level;
  }*/

  bool log_singletone::get_set_need_thread_id(bool is_need_set, bool is_need_val)
  {
    static bool is_need = false;
    if(is_need_set)
      is_need = is_need_val;

    return is_need;
  }

  bool log_singletone::get_set_need_proc_name(bool is_need_set, bool is_need_val)
  {
    static bool is_need = true;
    if(is_need_set)
      is_need = is_need_val;

    return is_need;
  }
  uint64_t log_singletone::get_set_err_count(bool is_need_set, uint64_t err_val)
  {
    static uint64_t err_count = 0;
    if(is_need_set)
      err_count = err_val;

    return err_count;
  }


#ifdef _MSC_VER
  void log_singletone::SetThreadName( DWORD dwThreadID, const char* threadName)
  {
#define MS_VC_EXCEPTION 0x406D1388

#pragma pack(push,8)
    typedef struct tagTHREADNAME_INFO
    {
      DWORD dwType; // Must be 0x1000.
      LPCSTR szName; // Pointer to name (in user addr space).
      DWORD dwThreadID; // Thread ID (-1=caller thread).
      DWORD dwFlags; // Reserved for future use, must be zero.
    } THREADNAME_INFO;
#pragma pack(pop)



    Sleep(10);
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = (char*)threadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;

    __try
    {
      RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }
  }
#endif

  bool log_singletone::set_thread_log_prefix(const std::string& prefix)
  {
#ifdef _MSC_VER
    SetThreadName(-1, prefix.c_str());
#endif


    logger* plogger = get_or_create_instance();
    if(!plogger) return false;
    return plogger->set_thread_prefix(prefix);
  }

  std::string log_singletone::get_prefix_entry()
  {
    std::stringstream str_prefix;
    //write time entry
    if ( get_set_time_level() <= get_set_log_detalisation_level() )
      str_prefix << get_day_time_string() << " ";

    //write process info
    logger* plogger = get_or_create_instance();
    //bool res = false;
    if(!plogger)
    { //globally uninitialized, create new logger for each call of get_prefix_entry() and then delete it
      plogger = new logger();
    }

    //if ( get_set_need_proc_name() && get_set_process_level() <= get_set_log_detalisation_level()  )
    //    str_prefix << "[" << plogger->m_process_name << " (id=" << GetCurrentProcessId() << ")] ";
//#ifdef _MSC_VER_EX
    if ( get_set_need_thread_id() /*&& get_set_tid_level() <= get_set_log_detalisation_level()*/ )
      str_prefix << "tid:" << misc_utils::get_thread_string_id() << " ";
//#endif

    if(plogger->m_thr_prefix_strings.size())
    {
      CRITICAL_REGION_LOCAL(plogger->m_critical_sec);
      std::string thr_str = misc_utils::get_thread_string_id();
      std::map<std::string, std::string>::iterator it = plogger->m_thr_prefix_strings.find(thr_str);
      if(it!=plogger->m_thr_prefix_strings.end())
      {
        str_prefix << it->second;
      }
    }


    if(get_set_is_uninitialized())
      delete plogger;

    return str_prefix.str();
  }

  log_singletone::log_singletone() {}

  bool log_singletone::init()
  {
    return true;/*do nothing here*/
  }
  bool log_singletone::un_init()
  {
    //delete object
    logger* plogger = get_set_instance_internal();
    if(plogger) delete plogger;
    //set uninitialized
    get_set_is_uninitialized(true, true);
    get_set_instance_internal(true, NULL);
    return true;
  }

  logger* log_singletone::get_or_create_instance()
  {
    logger* plogger = get_set_instance_internal();
    if(!plogger)
      if(!get_set_is_uninitialized())
        get_set_instance_internal(true, plogger = new logger);

    return plogger;
  }

  logger* log_singletone::get_set_instance_internal(bool is_need_set, logger* pnew_logger_val)
  {
    static logger* val_plogger = NULL;

    if(is_need_set)
      val_plogger = pnew_logger_val;

    return val_plogger;
  }

  bool log_singletone::get_set_is_uninitialized(bool is_need_set, bool is_uninitialized)
  {
    static bool val_is_uninitialized = false;

    if(is_need_set)
      val_is_uninitialized = is_uninitialized;

    return val_is_uninitialized;
  }
  //static int get_set_error_filter(bool is_need_set = false)
}
}
