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


#ifndef _PROFILE_TOOLS_H_
#define _PROFILE_TOOLS_H_

#include <stdlib.h>
#include <stdint.h>

namespace epee
{

#ifdef ENABLE_PROFILING
#define PROFILE_FUNC(immortal_ptr_str) static profile_tools::local_call_account lcl_acc(immortal_ptr_str); \
	profile_tools::call_frame cf(lcl_acc);

#define PROFILE_FUNC_SECOND(immortal_ptr_str) static profile_tools::local_call_account lcl_acc2(immortal_ptr_str); \
	profile_tools::call_frame cf2(lcl_acc2);

#define PROFILE_FUNC_THIRD(immortal_ptr_str) static profile_tools::local_call_account lcl_acc3(immortal_ptr_str); \
	profile_tools::call_frame cf3(lcl_acc3);

#define PROFILE_FUNC_ACC(acc) \
	profile_tools::call_frame cf(acc);


#else
#define PROFILE_FUNC(immortal_ptr_str)
#define PROFILE_FUNC_SECOND(immortal_ptr_str)
#define PROFILE_FUNC_THIRD(immortal_ptr_str)
#endif

#define START_WAY_POINTS() uint64_t _____way_point_time = epee::misc_utils::get_tick_count();
#define WAY_POINT(name) {uint64_t delta = epee::misc_utils::get_tick_count()-_____way_point_time; LOG_PRINT("Way point " << name << ": " << delta, LOG_LEVEL_2);_____way_point_time = misc_utils::get_tick_count();}
#define WAY_POINT2(name, avrg_obj) {uint64_t delta = epee::misc_utils::get_tick_count()-_____way_point_time; avrg_obj.push(delta); LOG_PRINT("Way point " << name << ": " << delta, LOG_LEVEL_2);_____way_point_time = misc_utils::get_tick_count();}


#define TIME_MEASURE_START(var_name)    uint64_t var_name = epee::misc_utils::get_tick_count();
#define TIME_MEASURE_FINISH(var_name)   var_name = epee::misc_utils::get_tick_count() - var_name;

namespace profile_tools
{
	struct local_call_account
	{
		local_call_account(const char* pstr);
		~local_call_account();

		size_t m_count_of_call;
		uint64_t m_summary_time_used;
		const char* m_pname;
	};
	
  struct call_frame_impl;
	struct call_frame
	{
		call_frame(local_call_account& cc);
		~call_frame();
		
	private:
		local_call_account& m_cc;
    call_frame_impl *m_pimpl;
	};
}
}


#endif //_PROFILE_TOOLS_H_
