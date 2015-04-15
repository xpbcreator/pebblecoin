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

#pragma once

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio.hpp>
#include <cstdlib>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <iostream>

using boost::asio::deadline_timer;
using boost::asio::ip::udp;

class blocking_udp_client
{
public:
  blocking_udp_client();
  
  bool receive_from(const boost::asio::mutable_buffer& buffer, udp::endpoint& endpoint,
                    boost::posix_time::time_duration timeout);
  bool send_to(const boost::asio::mutable_buffer& buffer, udp::endpoint& endpoint);

private:
  void check_deadline();
  static void handle_receive(const boost::system::error_code& ec, std::size_t length,
                             boost::system::error_code* out_ec, std::size_t* out_length);

private:
  boost::asio::io_service io_service_;
  udp::socket socket_;
  deadline_timer deadline_;
};
