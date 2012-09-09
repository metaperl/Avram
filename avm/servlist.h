#ifndef AVM_SERVLIST_H
#define AVM_SERVLIST_H 1

#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define AVM_MSG_RESET "R"
#define AVM_MSG_STATUS "S"
#define AVM_DEFAULT_TIMEOUT 10

typedef struct server_list_struct *server_list;

/* A server list stores information about the registered servers. */

struct server_list_struct
{
  int data_fd;         /* socket descriptor for the data path; set to blocking with a short timeout */
  int status_fd;       /* socket descriptor for the control path; set to non-blocking */
  char *host;          /* host name passed to getaddrinfo */
  char *data_port;     /* data path port number passed to getaddrinfo */
  char *status_port;   /* control path port number passed to getaddrinfo; usually the next consecutive port */
  flag opened;         /* socket descriptors are valid */
  flag connected;      /* socket descriptors are valid and the connection has been established */
  list cache;          /* the last function sent to the server */
  time_t state_change; /* the last time it changed among busy, idle, and dead, or had its status requested */
  flag queried;        /* a status requested hasn't been answered yet */
  time_t request_time; /* the last time a status request was sent */
  char *expected_crc;  /* cyclic redundancy check for the last job sent to this server */
  double load_metric;  /* returned by the remote server in status requests and used for load balancing */
  server_list peer;    /* points to next server in a linked list */
};

struct statpacket      /* sent by a server in response to a status request */
{
  double load_average;
  unsigned short protocol_version;
  char payload[128];                 /* in protocol version 0, the null terminated crc for the running job */
};

  extern int avm_offline ();
  extern void avm_count_servlist ();
  extern void avm_initialize_servlist ();
  extern int avm_connectable (server_list server);
  extern void avm_wait_for_event (time_t interval);
  extern void avm_watch_server (server_list server);
  extern void avm_flush_server (server_list server);
  extern void avm_release_server (server_list *server);
  extern int avm_writable (server_list server, int *fault);
  extern void avm_unregister_server (server_list *servers);
  extern int avm_readable (server_list *server, int *fault);
  extern int avm_registered_server (char *host, int port_number);
  extern server_list avm_revived_server (time_t interval, int *fault);
  extern server_list avm_acquired_server (time_t interval, int *fault);
  extern int avm_unresponsive (server_list *server, time_t interval, list *value, int *fault);

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_SERVLIST_H */
