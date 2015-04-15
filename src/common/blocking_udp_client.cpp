// Copyright (c) 2015 The Pebblecoin developers
//
// blocking_udp_client.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "blocking_udp_client.h"

blocking_udp_client::blocking_udp_client()
    : socket_(io_service_),
      deadline_(io_service_)
{
  socket_.open(boost::asio::ip::udp::v4());
  
  deadline_.expires_at(boost::posix_time::pos_infin);
  check_deadline();
}

bool blocking_udp_client::receive_from(const boost::asio::mutable_buffer& buffer, udp::endpoint& endpoint,
                                       boost::posix_time::time_duration timeout)
{
  deadline_.expires_from_now(timeout);
  
  boost::system::error_code ec = boost::asio::error::would_block;
  std::size_t length = 0;
  
  socket_.async_receive_from(boost::asio::buffer(buffer), endpoint,
                             boost::bind(&blocking_udp_client::handle_receive, _1, _2, &ec, &length));
  
  do io_service_.run_one(); while (ec == boost::asio::error::would_block);
  
  return ec == boost::system::errc::success;
}

bool blocking_udp_client::send_to(const boost::asio::mutable_buffer& buffer, udp::endpoint& endpoint)
{
  return socket_.send_to(boost::asio::buffer(buffer), endpoint) > 0;
}

void blocking_udp_client::check_deadline()
{
  if (deadline_.expires_at() <= deadline_timer::traits_type::now())
  {
    socket_.cancel();
    deadline_.expires_at(boost::posix_time::pos_infin);
  }
  
  deadline_.async_wait(boost::bind(&blocking_udp_client::check_deadline, this));
}

void blocking_udp_client::handle_receive(const boost::system::error_code& ec, std::size_t length,
                                         boost::system::error_code* out_ec, std::size_t* out_length)
{
  *out_ec = ec;
  *out_length = length;
}
