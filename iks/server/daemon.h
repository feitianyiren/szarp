#ifndef __DAEMON_H__
#define __DAEMON_H__

#include <boost/asio.hpp>

bool daemonize( boost::asio::io_service& service );

#endif /* end of include guard: __DAEMON_H__ */

