
/* concrete representation of pending remote concurrent computations

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
#include <avm/chrcodes.h>
#include <avm/common.h>
#include <avm/error.h>
#include <avm/apply.h>
#include <avm/branches.h>
#include <avm/compare.h>
#include <avm/rawio.h>
#include <avm/jobs.h>
#include <avm/farms.h>
#include <avm/servlist.h>
#include <avm/lists.h>
#include <avm/listfuns.h>

/* the maximum number of farm nodes stored in the following list */
#define CACHE_SIZE 0xff

/* a list of recycled farm nodes available without using malloc */
static farm available_farm = NULL;

/* the number of farm nodes in the above list */
static int available_farms;

/* Non-zero means static variables have been initialized. */
static int initialized = 0;

/* the number of allocated and not reclaimed structs */
static counter extant_farms = 0;

/* error messages as lists of lists of character representations */
static list memory_overflow = NULL;
static list cache_miss = NULL;
static list reset = NULL;







void
avm_abnormally_terminate (maggie)
     farm *maggie;

     /* this gets rid of a farm, and tries to stop the remotely
	running jobs but doesn't care if it's unsuccessful */
{
  farm old;
  int sent;
  char *msg = AVM_MSG_RESET;

  if (*maggie ? (*maggie)->prev : NULL)
    (*maggie)->prev->next = NULL;
  while (*maggie)
    {
      *maggie = (old = *maggie)->next;
      avm_dispose (old->operand);
      if (old->runner ? (old->runner->connected) : 0)
	sent = send(old->runner->status_fd, msg, 1 + strlen (msg), MSG_NOSIGNAL);
      avm_release_server (&(old->runner));
      if (available_farms < CACHE_SIZE)
	{
	  old->next = available_farm;
	  available_farm = old;
	  available_farms++;
	}
      else
	{
	  extant_farms--;
	  free (old);
	}
    }
}








void
avm_plant(maggie, top, fault)
     farm *maggie;
     job top;
     int *fault;

     /* This plants one tree in the farm, using the list of its
	prerequisites' roots as the operand, and deletes the
	prerequisites from the tree. In case of a fault, the farm is
	cleared and the prerequisites are still deleted, but the rest
	of the tree is left. */
{
  list front, back;
  farm new_farm;
  job descendent;

  if (!top)
    return;
  front = back = NULL;
  descendent = top->prerequisites;
  while (*fault ? NULL : descendent)
    {
      avm_recoverable_enqueue (&front, &back, avm_copied (descendent->root), fault);
      descendent = descendent->corequisites;
    }
  avm_free_job (&(top->prerequisites));
  if (*fault)
    return;
  if (available_farm)
    {
      new_farm = available_farm;
      available_farm = available_farm->next;
      available_farms--;
    }
  else
    {
      if (*fault = !(new_farm = (farm) malloc (sizeof (*new_farm))))
	{
	  avm_abnormally_terminate (maggie);
	  avm_dispose (front);
	  return;
	}
      extant_farms++;
    }
  memset (new_farm, 0, sizeof (*new_farm));
  new_farm->site = top;
  new_farm->operand = front;
  if (!*maggie)
    new_farm->prev = new_farm->next = new_farm;
  else
    {
      new_farm->next = *maggie;
      new_farm->prev = (*maggie)->prev;
      (*maggie)->prev = new_farm;
      if (new_farm->prev)
	new_farm->prev->next = new_farm;
    }
  *maggie = new_farm;
}










static void
propagate(maggie, new_root, fault)
     farm *maggie;
     list new_root;
     int *fault;

     /* returns a result to the job addressed by the first item of the
        farm, clears it and checks whether the dependent job exists and
        is enabled, in which case it is planted */
{
  farm old;
  job top;

  if (*fault)
    return;
  if (!(maggie ? ((old = *maggie) ? old->site : NULL) : NULL))
    avm_internal_error (110);
  if (*maggie == (*maggie)->next)
    *maggie = NULL;
  else
    {
      *maggie = (*maggie)->next;
      if (*maggie)                      /* should always be true for a circularly linked list */
	(*maggie)->prev = old->prev;
      if (old->prev)
	old->prev->next = *maggie;
    }
  avm_dispose (old->operand);
  avm_dispose (old->site->root);
  old->site->root = new_root;
  old->site->running = 0;
  top = (!old->site->dependent ? NULL : --(old->site->dependent->dependence) ? old->site->dependent : NULL);
  if (available_farms < CACHE_SIZE)
    {
      old->next = available_farm;
      available_farm = old;
      available_farms++;
    }
  else
    {
      extant_farms--;
      free (old);
    }
  avm_plant(maggie, top, fault);
 }










static list
farmed_out (maggie, server, fault)
     farm *maggie;
     server_list server;
     int *fault;

     /* assigns a job to a remote server */
{
  list equality, package;
  job site;
  int timeout, closed;

  if (*fault = (*fault ? 1 : !(*maggie) ? 1 : !(site = (*maggie)->site) ? 1 : !server))
    {
      avm_release_server (&server);
      return NULL;
    }
  equality = avm_binary_comparison (site->root, server->cache, fault);
  if (*fault)
    return equality;
  (*maggie)->cache_hit = ! ! equality;
  package = avm_recoverable_join (equality ? NULL : avm_copied (site->root), avm_copied ((*maggie)->operand));
  avm_dispose (equality);
  if (*fault = !package)
    return NULL;
  closed = timeout = 0;
  avm_recoverable_send_list (server->data_fd, package, &(server->expected_crc), &timeout, &closed, fault);
  avm_dispose (package);
  avm_dispose (server->cache);
  server->cache = NULL;
  if (*fault ? 1 : timeout)
    {
      close (server->data_fd);
      close (server->status_fd);
      if (timeout ? closed : 0)
	server->opened = server->connected = 0;
      avm_release_server (&server);
      return NULL;
    }
  server->cache = avm_copied (site->root);
  site->running = 1;
  avm_watch_server (server);
  (*maggie)->runner = server;
  (*maggie) = (*maggie)->next;
  return NULL;
}











static int
matching_crc (runner, fault)
     server_list *runner;
     int *fault;

     /* If a crc is read from the runner matching the expected one, a
        true result is returned and the runner is set to null, but the
        server associated with it is left open and connected in
        preparation for reading the data, after which it should be
        released by the caller.

        If no crc is readable because the port isn't really ready
	(even though it would have been polled before this function
	was called), then the function returns false but doesn't
	release the runner, because this could happen if the data were
	subsequently removed by the kernel despite a positive polling
	result due to a transmission error.

        If the crc is unreadable because the connection was closed, by
	the server, the runner is flushed, closed, and released, and a
	false result is returned.

        If the crc is readable but incorrect, the runner is released
        and flushed but not closed, and a false result is returned. */
{
  char echoed_crc[128];
  int received;

  if (*fault ? 1 : !*runner)
    avm_internal_error (122);
  memset (echoed_crc, 0, sizeof echoed_crc);
  received = recv ((*runner)->data_fd, echoed_crc, 1 + strlen ((*runner)->expected_crc), MSG_NOSIGNAL | MSG_DONTWAIT);
  if (*fault = ((received == -1) ? (errno == ENOMEM) : 0))
    {
      avm_release_server (runner);
      return 0;
    }
  if ((received == -1) ? ((errno == EAGAIN) ? 1 : (errno == EWOULDBLOCK)) : 0)
    return 0;
  if ((received != -1) ? 0 : (errno == EINVAL) ? 1 : (errno == EFAULT) ? 1 : (errno == EBADF) ? 1 : (errno == ENOTSOCK))
    avm_internal_error (117);
  if (strcmp (echoed_crc, (*runner)->expected_crc) == 0)
    {
      *runner = NULL;
      return 1;
    }
  if ((received == -1) ? ((errno == ECONNREFUSED) ? 1 : (errno == ENOTCONN)) : 0)
    {
      close ((*runner)->data_fd);
      close ((*runner)->status_fd);
      (*runner)->opened = (*runner)->connected = 0;
    }
  avm_release_server (runner);
  return 0;
}












static list
farmed_in (maggie, fault)
     farm *maggie;
     int *fault;

     /* This gets a result back from a remote server, which should be
        be the null terminated crc followed by a non-empty list
        representing a pair (data,exception) of type %xbX in Ursala
        notation. The crc is read first and then the list is read by
        avm_recoverable_received_list. If the right side is non-empty,
        the left is a diagnostic message. If the package sent to the
        server had an empty left side (and therefore a true cache_hit
        field in the farm, so as to call the server's cached function)
        and the diagnostic message of "cache miss" is returned, a
        retry is enabled rather than raising an exception locally.
        The return value of this function is used only for diagnostic
        messages, with valid results being propagated up the job
        tree. */
{
  int timeout, closed, remote_fault;
  list result, old_result, equality;
  server_list old_runner;
  char *ignored_crc;

  if (*fault ? 1 : (!*maggie) ? 1 : !(old_runner = (*maggie)->runner) ? 1 : !((*maggie)->site))
    avm_internal_error (121);
  if (! matching_crc (&((*maggie)->runner), fault))
    {
      (*maggie)->site->running = ! ! (*maggie)->runner;
      return NULL;
    }
  (*maggie)->site->running = closed = timeout = 0;
  result = avm_recoverable_received_list (old_runner->data_fd, &ignored_crc, &timeout, &closed, fault);
  free (ignored_crc);
  if (timeout ? closed : 0)
    {
      close (old_runner->data_fd);
      close (old_runner->status_fd);
      old_runner->opened = old_runner->connected = 0;
    }
  avm_release_server (&old_runner);
  if (*fault ? 1 : !result ? 1 : timeout ? 1 : closed)
    return result;
  result = avm_copied ((old_result = result)->head);
  remote_fault = !!(old_result->tail);
  avm_dispose (old_result);
  if (!remote_fault)
    {
      propagate (maggie, result, fault);
      return NULL;
    }
  equality = ((*maggie)->cache_hit ? NULL : avm_binary_comparison (result, cache_miss, fault));
  if (*fault)
    {
      avm_dispose (result);
      return equality;
    }
  if (*fault = !equality)
    return result;
  avm_dispose (equality);
  return NULL;
}










static farm
dependent_list (maggie)
     farm maggie;

     /* returns a singly linked list of the next generation of
	dependent jobs of the given farm */
{
  farm front_dependent, back_dependent, bottom, new_farm;
  job dependent;

  if (!(maggie ? maggie->prev : NULL))
    return NULL;
  front_dependent = back_dependent = NULL;
  (bottom = maggie)->prev->next = NULL;
  while (bottom)
    if (!(bottom->site) ? 1 : !(bottom->site->dependent) ? 1 : bottom->site->dependent->deficit)
      bottom = bottom->next;                                                              /* skip those already visited */
    else
      {
	if (available_farms)
	  {
	    new_farm = available_farm;
	    available_farm = available_farm->next;
	    available_farms--;
	  }
	else if (new_farm = (farm) malloc (sizeof *new_farm))
	  extant_farms++;
	else
	  new_farm = bottom = NULL;
	if (new_farm)
	  {
	    memset (new_farm, 0, sizeof *new_farm);
	    back_dependent = (back_dependent ? (back_dependent->next = new_farm) : (front_dependent = new_farm));
	    back_dependent->site = bottom->site->dependent;
	    back_dependent->site->deficit = 1;                                           /* mark this dependent visited */
	    bottom = bottom->next;
	  }
      }
  bottom = front_dependent;
  while (bottom)                                                                                     /* clear the marks */
    {
      if (bottom->site)
	bottom->site->deficit = 0;
      bottom = bottom->next;
    }
  maggie->prev->next = maggie;
  return front_dependent;
}









static farm
rebalanced (parents)
     farm parents;

     /* This gets called as the last alternative to going to sleep
        when all the jobs in the farm are running, in case there might
        be a chance to make better use of some idle servers by
        reorganizing the tree of jobs. The parameter should be a list
        of the dependent jobs of the running jobs represented as a
        singly linked acyclic farm pointing to them. The result
        returned is a singly linked farm pointing to the subset of
        given farm whose sites are now ready to be planted as a result
        of having their running prerequisites exchanged for finished
        prerequistes of other jobs in the list. This transformation is
        valid only when the function being computed by the jobs is
        commutative, as indicated by the balanceable parameter to the
        harvest function. */
{
  job finished_front, finished_back, running_front, running_back, back;
  farm result_front, result_back, parent;

  if (!((parent = parents) ? parents->next : NULL))
    {
      avm_abnormally_terminate (&parents);
      return NULL;
    }
  finished_front = finished_back = running_front = running_back = NULL;
  while (parent ? parent->site : NULL)                                    /* flatten and bipartition the prerequisites */
    {
      while (parent->site->prerequisites)
	{
	  parent->site->prerequisites->dependent = NULL;
	  if (parent->site->prerequisites->running ? 1 : ! ! parent->site->prerequisites->prerequisites)
	    {
	      avm_queue_job (&running_front, &running_back, &(parent->site->prerequisites), NULL);
	      (parent->site->dependence)--;
	    }
	  else
	    avm_queue_job (&finished_front, &finished_back, &(parent->site->prerequisites), NULL);
	  parent->site->deficit++;
	}
      if (parent->site->dependence)           /* unfinished prerequisites should have agreed with the dependence field */
	avm_internal_error (124);
      parent = parent->next;
    }
  parent = parents;
  while (parent ? parent->site : NULL)                     /* re-attach prerequisites using up the finished list first */
    {
      back = NULL;
      while (parent->site->deficit)
	{
	  if (finished_front)         /* stick it under the parent but don't bump the dependence because it's finished */
	    {
	      finished_front->dependent = parent->site;
	      avm_queue_job (&(parent->site->prerequisites), &back, &finished_front, NULL);
	    }
	  else if (!(running_front))              /* there should be at least as many prerequisites as we started with */
	    avm_internal_error (125);
	  else
	    avm_queue_job (&(parent->site->prerequisites), &back, &running_front, parent->site);
	  parent->site->deficit--;
	}
      parent = parent->next;
    }
  if (running_front ? running_back : NULL)                     /* there should have been no more than we started with */
    avm_internal_error (126);
  result_front = result_back = NULL;
  while (!parents ? 0 : !(parents->site) ? 0 : !(parents->site->dependence))   /* those with no running prerequisites */
    {
      result_back = (result_back ? (result_back->next = parents) : (result_front = parents));
      parents = parents->next;
      result_back->next = NULL;
    }
  avm_abnormally_terminate (&parents);             /* not really terminated since they aren't running yet, just freed */
  return result_front;
}











void
avm_harvest (maggie, balanceable, fault)
     farm maggie;
     flag balanceable;
     int *fault;

     /* This sends out each item from the farm to a remote machine,
	unless it's the last one or a constant funciton, which is
	evaluated locally. Any time a job in the farm is finished, new
	jobs inferred from its dependents might be started. If this
	function is called with a list of farm nodes initialized to
	point to the leaf nodes in a job tree, then by the time it's
	finished, the whole tree will be evaluated up to the root. */
{
  list value;
  job site;
  int progress;                  /* set in the event of a propagation or status check */
  int longest_running_job;       /* the first remote job started since the most recent progress event */
  server_list server;
  farm collection, old;

  site = NULL;
  longest_running_job = 0;
  while (*fault ? 0 : !maggie ? 0 : (*fault = !(site = maggie->site)) ? 0 : ! _avm_reset)
    {
      if (maggie->runner)
	{
	  if (progress = avm_readable (&(maggie->runner), fault))
	    value = farmed_in (&maggie, fault);
	  else
	    progress = avm_unresponsive (&(maggie->runner), WAIT, &value, fault);
	}
      else if (progress = !(site->root))
	propagate (&maggie, avm_copied (maggie->operand), fault);
      else if (progress = !(maggie->operand))
	propagate (&maggie, avm_copied (site->root), fault);
      else if (progress = (site->root->head ? 0 : !(site->root->tail)))
	propagate (&maggie, value = (list) avm_flattened (maggie->operand, fault), fault);
      else if (progress = (site->root->tail ? 0 : !(site->root->head->head)))
	propagate (&maggie, avm_copied (site->root->head->tail), fault);
      else if (site->dependent ? (server = avm_acquired_server (RETRY, fault)) : NULL)
	{
	  longest_running_job = (longest_running_job ? longest_running_job : (int) maggie);
	  value = farmed_out (&maggie, server, fault);
	}
      else if (progress = !*fault)
	{
	  value = avm_recoverable_apply (avm_copied (site->root), avm_copied (maggie->operand), fault);
	  if (!*fault)
	    propagate (&maggie, value, fault);
	}
      if (*fault ? 0 : ((longest_running_job = (progress ? 0 : longest_running_job)) == (int) maggie))
	{
	  if (!(old = collection = (balanceable ? rebalanced (dependent_list (maggie)) : NULL)))
	    avm_wait_for_event (WAIT);
	  else
	    while (*fault ? 0 : collection)
	      {
		avm_plant (&maggie, collection->site, fault);
		collection = collection->next;
	      }
	  avm_abnormally_terminate (&old);
	}
      value = (*fault ? value : NULL);
    }
  if ((*fault = (*fault ? 1 : _avm_reset)) ? site : NULL)
    {
      while (site->dependent)
	site = site->dependent;
      avm_dispose (site->root);
      site->root = (value ? value : _avm_reset ? avm_copied (reset) : avm_copied (memory_overflow));
    }
  avm_abnormally_terminate (&maggie);
}











void
avm_initialize_farms ()

     /* This initializes static data structures. */
{

  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_branches ();
  avm_initialize_apply ();
  avm_initialize_compare ();
  reset = avm_join (avm_strung ("reset"), NULL);
  cache_miss = avm_join (avm_strung ("cache miss"), NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
}











void
avm_count_farms ()

{
  server_list old,servers;

  if (!initialized)
    return;
  avm_dispose (memory_overflow);
  avm_dispose (cache_miss);
  avm_dispose (reset);
  memory_overflow = NULL;
  cache_miss = NULL;
  reset = NULL;
  initialized = 0;
  if (extant_farms)
    avm_reclamation_failure ("farms", extant_farms);
}
