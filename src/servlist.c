
/* for keeping track of remote servers and their states

   Copyright (C) 2010 Dennis Furey

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#define _GNU_SOURCE

#include <netdb.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <avm/common.h>
#include <avm/error.h>
#include <avm/lists.h>
#include <avm/chrcodes.h>
#include <avm/servlist.h>

/* Non-zero means static variables have been initialized. */
static int initialized = 0;

/* diagnostic reported when when the server uses a newer protocol version than the client recognizes */
static list protocol_breach = NULL;

/* Three global static lists are maintained, for busy, dead, and idle servers. */
static int idle_servers = 0;
static int busy_servers = 0;
static int dead_servers = 0;
static int extant_servers = 0;

/* lists of available servers */
static server_list idle_front = NULL;
static server_list idle_back = NULL;
static server_list busy_front = NULL;
static server_list busy_back = NULL;
static server_list dead_front = NULL;
static server_list dead_back = NULL;


/*---------------------------------------------------- registration --------------------------------------------*/




int
avm_registered_server (host, port_number)
     char *host;
     int port_number;

     /* this adds to the list of available servers, and returns non-zero if successful */
{
  server_list server;
  int fault;

  if (!(server = (server_list) malloc (sizeof *server)))
    return 0;
  extant_servers++;
  memset (server, 0, sizeof *server);
  fault = (asprintf (&(server->data_port), "%d", port_number) < 0);
  fault = (fault ? 1 : (asprintf (&(server->status_port), "%d", port_number + 1) < 0));
  fault = (fault ? 1 : !(server->host = strdup (host)));
  if (fault = (fault ? 1 : ! avm_connectable (server)))
    {
      avm_unregister_server (&server);
      return 0;
    }
  else
    {
      busy_servers++;                       /* because releasing normally decrements it */
      avm_release_server (&server);
      return 1;
    }
}










void
avm_unregister_server (servers)
     server_list *servers;

     /* frees all servers and closes open connections */
{
  server_list old;

  while (*servers)
    {
      extant_servers--;
      *servers = (old = *servers)->peer;
      avm_flush_server (old);
      if (old->opened)
	{
	  close (old->data_fd);
	  close (old->status_fd);
	}
      avm_dispose (old->cache);
      free (old->expected_crc);
      free (old->status_port);
      free (old->data_port);
      free (old->host);
      free (old);
    }
}



/*--------------------------------------------------- observation ----------------------------------------------*/






void
avm_watch_server (server)
     server_list server;

     /* puts a server into the busy list, where it will be monitored by
	avm_wait_for_event */
{
  if (!server)
    return;
  busy_servers++;
  server->state_change = time (NULL);
  server->peer = NULL;
  busy_back = (busy_back ? (busy_back->peer = server) : (busy_front = server));
}










void
avm_wait_for_event (interval)
     time_t interval;

     /* sleeps until either an event happens on a watched server or
	it's time to check the one that has been running the longest,
	but can also just return if there isn't enough memory, since
	the only detrimental effect will be not waiting long enough */
{
  time_t running_time;
  struct pollfd *fds;
  struct pollfd *nextfd;
  server_list server;
  int numfds, fdnum;

  if (!(server = busy_front) ? 0 : (running_time = time (NULL) - busy_front->state_change) >= interval)
    return;
  fds = NULL;
  numfds = busy_servers;
  while ((numfds <= 0) ? 0 : !(fds = (struct pollfd *) malloc (numfds * sizeof *fds)))
    numfds = numfds >> 1;
  if (!fds)
    return;
  fdnum = 0;
  nextfd = fds;
  while (server ? (fdnum++ < numfds) : 0)
    {
      nextfd->fd = server->data_fd;
      (nextfd++)->events = POLLIN | POLLRDHUP | POLLHUP | POLLNVAL;
      server = server->peer;
    }
  poll (fds, fdnum, interval - running_time);
  free (fds);
  return;
}









/*---------------------------------------------------- mutation ------------------------------------------------*/









void
avm_flush_server (server)
     server_list server;

     /* this gets rid of any data waiting to be read from a server,
        and will also close the connection if there's a problem
        reading from it */
{
  struct statpacket reply[10];
  struct pollfd fds;
  int flushing;

  if (!server ? 1 : !(server->opened) ? 1 : !(server->connected))
    return;
  server->queried = 0;
  fds.events = POLLIN;
  fds.fd = server->status_fd;
  if (flushing = (poll (&fds, 1, 0) > 0))
    {
      while (flushing > 0)
	flushing = recv ((server)->status_fd, reply, sizeof reply, MSG_NOSIGNAL | MSG_DONTWAIT);
      if (!flushing ? 1 : (flushing == -1) ? ((errno == ENOTCONN) ? 1 : (errno == ECONNREFUSED)) : 0)
	server->connected = 0;
    }
  fds.fd = server->data_fd;
  if (flushing = (poll (&fds, 1, 0) > 0))
    {
      while (flushing > 0)
	flushing = recv (server->data_fd, reply, sizeof reply, MSG_NOSIGNAL | MSG_DONTWAIT);
      if (!flushing ? 1 : (flushing == -1) ? ((errno == ENOTCONN) ? 1 : (errno == ECONNREFUSED)) : 0)
	server->connected = 0;
    }
  if (server->opened = server->connected)
    return;
  close (server->data_fd);
  close (server->status_fd);
}










void
avm_release_server (server)
     server_list *server;

     /* returns a server that was most likely in the busy list either
	to the idle or dead server list depending on its connection
	status; also tries to flush leftover messages but doesn't
	examine them */
{
  int sent, flushed;
  struct statpacket reply;

  if (!(*server))
    return;
  busy_servers--;
  avm_flush_server (*server);
  (*server)->queried = 0;
  (*server)->peer = NULL;
  (*server)->state_change = time (NULL);
  free ((*server)->expected_crc);
  (*server)->expected_crc = NULL;
  avm_dispose ((*server)->cache);
  if ((*server)->opened)
    {
      idle_servers++;
      idle_back = (idle_back ? (idle_back->peer = *server) : (idle_front = *server));
    }
  else
    {
      dead_servers++;
      (*server)->connected = 0;
      dead_back = (dead_back ? (dead_back->peer = *server) : (dead_front = *server));
    }
  *server = NULL;
}








server_list
avm_revived_server (interval, fault)
     time_t interval;
     int *fault;

   /* returns the oldest dead server in the list that hasn't been
      tried for at least the given interval and is possible to reopen,
      based on the assumption that the dead list ordered from oldest
      to newest. */
{
  server_list unwritable_front, unwritable_back, result;
  int found;
  time_t now;

  now = time (NULL);
  found = 0;
  unwritable_front = unwritable_back = result = NULL;
  while (*fault ? 0 : found ? 0 : !dead_front ? 0 : (dead_front->state_change < now - interval))
    if (!(found = avm_writable (dead_front, fault)))
      {
	dead_front->state_change = now = time (NULL);
	unwritable_back = (unwritable_back ? (unwritable_back->peer = dead_front) : (unwritable_front = dead_front));
	dead_front = ((dead_front == dead_back) ? (dead_back = NULL) : dead_front->peer);
	unwritable_back->peer = NULL;
      }
  if (found)
    {
      dead_servers--;
      result = dead_front;
      result->state_change = time (NULL);
      dead_front = ((dead_front == dead_back) ? (dead_back = NULL) : dead_front->peer);
      result->peer = NULL;
    }
  if (dead_front)
    dead_back->peer = unwritable_front;
  else
    dead_front = unwritable_front;
  dead_back = (unwritable_back ? unwritable_back : dead_back);
  return result;
}










server_list
avm_acquired_server (interval, fault)
     time_t interval;
     int *fault;

   /* This tries to select the least loaded server from the list of
      idle servers based on the load metric and removes it from the
      list if it finds one. If it can't do that, it tries to revive a
      dead server. If it discovers any unconnectable servers along the
      way, it moves them to the dead list. */
{
  server_list server, result, writable_front, writable_back;
  double min_load;

  result = writable_front = writable_back = NULL;
  while (*fault ? 0 : idle_front)
    if (avm_writable (idle_front, fault))
      {
	if (!result ? 1 : (idle_front->load_metric < min_load))         /* scan the idle list for the minimum load */
	  {
	    result = idle_front;
	    min_load = result->load_metric;
	  }
	writable_back = (writable_back ? (writable_back->peer = idle_front) : (writable_front = idle_front));
	idle_front = ((idle_back == idle_front) ? (idle_back = NULL) : idle_front->peer);
	writable_back->peer = NULL;
      }
    else if (!(idle_front->opened))
      {                                                          /* enqueue unconnectable servers in the dead list */
	idle_front->state_change = time (NULL);
	dead_back = (dead_back ? (dead_back->peer = idle_front) : (dead_front = idle_front));
	idle_front = ((idle_back == idle_front) ? (idle_back = NULL) : (idle_front->peer));
	dead_back->peer = NULL;
      }
  if (idle_front)                                            /* put the surviving  servers back into the idle list */
    idle_back->peer = writable_front;
  else
    idle_front = writable_front;
  idle_back = (writable_back ? writable_back : idle_back);
  if (*fault ? 1 : !result)
    return (*fault ? NULL : avm_revived_server (interval, fault));      /* if there's no result, try the dead list */
  idle_servers--;
  if (result == idle_front)
    idle_front = ((idle_front == idle_back) ? (idle_back = NULL) : idle_front->peer);
  else
    {
      server = idle_front;
      while (server->peer != result)
	server = server->peer;
      if (!(server->peer = result->peer))
	idle_back = server;
    }
  result->peer = NULL;
  return result;
}











/*---------------------------------------------------- interrogation -------------------------------------------*/





int
avm_offline ()

/* this function returns a non-zero value if there are no remote
   servers available */

{
  int online;

  if (!initialized)
    avm_initialize_remote ();
  return (idle_servers ? 0 : busy_servers ? 0 : ! dead_servers);
}









int
avm_readable (server, fault)
     server_list *server;
     int *fault;

     /* tests whether there are data to receive from a given busy
        server, and may also move it to the dead server list if a
        hangup is detected */ 
{
  struct pollfd fds;

  fds.fd = (*server)->data_fd;
  fds.events = POLLIN | POLLRDHUP | POLLHUP | POLLNVAL;
  if (*fault)
    return 0;
  if (poll (&fds, 1, 0) < 0)
    {
      if (*fault = (errno == ENOMEM))
	return 0;
      avm_internal_error (120);
    }
  if (fds.revents & POLLNVAL)
    avm_internal_error (111);
  if (fds.revents & POLLIN)
    return 1;
  if (fds.revents & (POLLRDHUP | POLLHUP))
    {
      close ((*server)->data_fd);
      close ((*server)->status_fd);
      (*server)->opened = (*server)->connected = 0;
      avm_release_server (server);
    }
  return 0;
}









int
avm_writable (server, fault)
     server_list server;
     int *fault;

     /* This returns true iff a server socket is writable, which is tested
        here instead of when the socket is created (in connectable) because
        it's done asynchronously. If so, the connected field is set. */
{
  struct pollfd fds;
  int connection_error, connection_error_size;

  if (*fault ? 1 : !server)
    return 0;
  if (server->opened ? server->connected : 0)
    return 1;
  if (!(server->opened ? 1 : avm_connectable (server)))
    return 0;
  fds.fd = server->data_fd;
  fds.events = POLLOUT | POLLRDHUP | POLLHUP | POLLNVAL;
  if (poll (&fds, 1, 0) < 0)
    {
      if (*fault = (errno == ENOMEM))
	return 0;
      avm_internal_error (112);
    }
  if (fds.revents & POLLNVAL)
    avm_internal_error (113);
  if (fds.revents & POLLOUT)
    {
      connection_error_size = sizeof connection_error;
      if (getsockopt (server->data_fd, SOL_SOCKET, SO_ERROR, &connection_error, &connection_error_size) == -1)
	avm_internal_error (114);
      if (server->connected = ! connection_error)
	return 1;
    }
  close (server->status_fd);
  close (server->data_fd);
  return (server->opened = 0);
}











int
avm_connectable (server)
     server_list server;

     /* This initializes socket descriptors and initiates the
	connection asynchronously. The data socket is given a timeout
	of AVM_DEFAULT_TIMEOUT seconds and the status socket is made
	non-blocking. If successful, the opened field is set but not
	the connected field, indicating that the server is in a state
	of waiting for the connection to be established. The connected
	field will be set subsequently when the server is successfully
	polled by the writable function. */
{
  struct addrinfo hints, *servinfo, *statinfo, *info;
  struct timeval timeout;
  int fault;

  if (!server)
    return 0;
  statinfo = servinfo = NULL;
  memset (&hints, 0, sizeof hints);
  memset (&timeout, 0, sizeof timeout);
  timeout.tv_sec = AVM_DEFAULT_TIMEOUT;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  fault = (getaddrinfo (server->host, server->data_port, &hints, &servinfo) != 0);
  info = servinfo;
  while (fault ? 0 : info ? ((server->data_fd = socket (info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) : 0)
    info = info->ai_next;
  fault = (fault ? 1 : (fcntl (server->data_fd, F_SETFL, O_NONBLOCK) == -1));
  if (!(fault = (fault ? 1 : !info)))
    fault = ((connect (server->data_fd, info->ai_addr, info->ai_addrlen) == -1) ? (errno != EINPROGRESS) : 0);
  fault = (fault ? 1 : (fcntl (server->data_fd, F_SETFL, O_SYNC) == -1));
  fault = (fault ? 1 : (setsockopt (server->data_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout) == -1));
  fault = (fault ? 1 : (setsockopt (server->data_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) == -1));
  fault = (fault ? 1 : (getaddrinfo (server->host, server->status_port, &hints, &statinfo) != 0));
  info = statinfo;
  while (fault ? 0 : info ? ((server->status_fd = socket (info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) : 0)
    info = info->ai_next;
  fault = (fault ? 1 : (fcntl (server->status_fd, F_SETFL, O_NONBLOCK)));
  if (!(fault = (fault ? 1 : !info)))
    fault = ((connect (server->status_fd, info->ai_addr, info->ai_addrlen) == -1) ? (errno != EINPROGRESS) : 0);
  if (servinfo)
    freeaddrinfo (servinfo);
  if (statinfo)
    freeaddrinfo (statinfo);
  if (server->opened = !fault)
    return 1;
  close (server->data_fd);
  close (server->status_fd);
  return 0;
}












static int
status_requestable (server, fault)
     server_list *server;
     int *fault;

     /* This makes a status request and returns true if the request is
        successfully made. It doesn't wait for a reply.  A status
        request consists of the null terminated AVM_MSG_STATUS string,
        followed by the null terminated crc of the relevant job. */
{
  int sent;
  char *status_code = AVM_MSG_STATUS;

  if (*fault ? 1 : !*server)
    avm_internal_error (118);
  sent = send ((*server)->status_fd, status_code, 1 + strlen (status_code), MSG_NOSIGNAL | MSG_MORE);
  if (*fault = ((sent == -1) ? (errno == ENOMEM) : 0))
    return 0;
  if (sent == 1 + strlen (status_code))
    (*server)->queried = 1;
  else
    (*server)->queried = ((sent == -1) ? ((errno == EAGAIN) ? 1 : (errno == EWOULDBLOCK)) : 0);
  if ((*server)->queried)
    {
      sent = send((*server)->status_fd, (*server)->expected_crc, 1 + strlen ((*server)->expected_crc), MSG_NOSIGNAL);
      if (*fault = ((sent == -1) ? (errno == ENOMEM) : 0))
	return 0;
      if (sent != 1 + strlen (status_code))
	(*server)->queried = ((sent == -1) ? ((errno == EAGAIN) ? 1 : (errno == EWOULDBLOCK)) : 0);
      if ((*server)->queried)
	{
	  (*server)->request_time = time (NULL);              /* a new status request has been sent or is being sent */
	  return 1;
	}
    }
  if (!(!sent ? 1 : (sent == -1) ? ((errno == EPIPE) ? 1 : (errno == ENOBUFS) ? 1 : (errno == ECONNRESET)) : 0))
    avm_internal_error (119);
  (*server)->opened = (*server)->connected = 0;             /* a request couldn't be sent because the server is down */
  close ((*server)->status_fd);
  close ((*server)->data_fd);
  avm_release_server (server);
  return 0;
}











static int
affirmative_response (server, value, fault)
     server_list *server;
     list *value;
     int *fault;

     /* This receives and interprets a status report expected from a
        server. This shouldn't block because status ports are set to
        non-blocking operation when opened, and the status request
        should have been sent a sufficient time ago when this function
        is called, so if the reply isn't aready here, the server is
        assumed to be down.

        In protocol version 0 (which is the only one at this writing),
        the reply to a status request is expected to be a statpacket
        containing the server's load metric, the protocol version (0),
        and the crc for the submitted job. A server can choose to send
        the result from the job to the data port instead of answering
        a status request on the status port if it's finished already,
        or such a thing could happen due to a race, so this condition
        is checked if the status request acknowledgement isn't
        received. */
{
  int received, result;
  struct statpacket reply;

  if (*fault ? 1 : !*server)
    avm_internal_error (116);
  *value = NULL;
  memset (&reply, 0, sizeof reply);
  received = recv ((*server)->status_fd, &reply, sizeof reply, MSG_NOSIGNAL);
  if (*fault = ((received == -1) ? (errno == ENOMEM) : 0))
    return 0;
  if (received > 0)                                                                          /* the server is up */
    {
      (*server)->load_metric = reply.load_average;
      if (*fault = reply.protocol_version)
	*value = avm_copied (protocol_breach);
      else 
	result = (strcmp ((*server)->expected_crc, reply.payload) == 0);
      avm_release_server (server);
      return (*fault ? 0 : result);                        /* but God knows if it's working on the submitted job */
    }
  else if (avm_readable (server, fault))
    return 1;
  if (!(!received ? 1 : (received == -1) ? ((errno == ENOTCONN) ? 1 : (errno == ECONNREFUSED)) : 0))
    avm_internal_error (117);
  (*server)->opened = (*server)->connected = 0;                                            /* the server is down */
  close ((*server)->status_fd);
  close ((*server)->data_fd);
  avm_release_server (server);
  return 0;
}









int
avm_unresponsive (server, interval, value, fault)
     server_list *server;
     time_t interval;
     list *value;
     int *fault;

     /* This requests a status update from a busy server or follows up
	a previous request and returns true and releases the server if
	there is any problem, such as a closed connection or a missing
	or wrong response. If the server is responsive, this returns
	0, as a correct response is considered a non-event insofar as
	it means that the client should just keep waiting. */
{
  *value = NULL;
  if (*fault)
    return 0;
  if (!*server ? 1 : !((*server)->connected) ? 1 : !((*server)->expected_crc))
    avm_internal_error (115);
  if (! (*server)->queried)
    return (! status_requestable (server, fault));
  if (time (NULL) - (*server)->request_time < interval)
    return 0;
  (*server)->queried = 0;
  return (! affirmative_response (server, value, fault));
}









/*---------------------------------------------------- initializaiton ------------------------------------------*/








void
avm_initialize_servlist ()

     /* This initializes static data structures. */
{

  if (initialized)
    return;
  initialized = 1;
  protocol_breach = avm_join (avm_strung ("incompatible client/server protocol"), NULL);
  avm_initialize_lists ();
  avm_initialize_compare ();
}








void
avm_count_servlist ()

{
  server_list old,servers;

  if (!initialized)
    return;
  avm_unregister_server (&idle_front);
  avm_unregister_server (&busy_front);
  avm_unregister_server (&dead_front);
  idle_back = busy_back = dead_back = NULL;
  avm_dispose (protocol_breach);
  protocol_breach = NULL;
  initialized = 0;
  if (extant_servers)
    avm_reclamation_failure ("server lists", extant_servers);
}
