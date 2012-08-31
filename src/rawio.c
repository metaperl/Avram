
/* functions for shifting data between lists in memory and raw format files

   Copyright (C) 2006 Dennis Furey

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

#include <netdb.h>
#include <stdint.h>
#include <errno.h>
#include <poll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <avm/common.h>
#include <avm/error.h>
#include <avm/lists.h>
#include <avm/branches.h>
#include <avm/chrcodes.h>
#include <avm/rawio.h>
#if HAVE_GCRYPT
#include <gcrypt.h>
#endif


/* This is number of characters between line breaks in raw
   output. Feel free to change it. */

#define file_width 79
#define WORD_SIZE 8       /* for binary transfers; at most sizeof char */
#define PACKET_SIZE 1024
#define OFFSET 0          /* can be set to 60 with a WORD_SIZE of 6 to match printable format */

/* non-zero means static variables are initialized */
static int initialized = 0;

#if HAVE_GCRYPT
static gcry_md_hd_t hash_context;
#endif


static int
sent_bit (repository, bit, filename, spool, spoke, column)
     FILE *repository;
     int bit;
     char *filename;
     int *spool;
     int *spoke;
     int *column;

     /* This puts a bit into a file, using a byte to spool them
        between calls. This has to be called a bunch of times at the
        end of the data in order to ensure that any unwritten bits get
        flushed. */
{
  int ioerror;

  ioerror = 0;
  (*spool) = ((*spool) << 1) + bit;
  if (((*spoke)++) == 5)
    {
      (*spool) += 60;
      if (ioerror = (putc (*spool, repository) != (*spool)))
	avm_non_fatal_io_error ("can't write to", filename, errno);
      else if (++(*column) == file_width)
	{
	  if (ioerror = (putc ('\n', repository) != '\n'))
	    avm_non_fatal_io_error ("can't write to", filename, errno);
	  *column = 0;
	}
      *spool = *spoke = 0;
    }
  return !ioerror;
}






static int
received_bit (object, filename, spoke, spool)
     FILE *object;
     char *filename;
     int *spoke;
     int *spool;

     /* This gets the next bit from the file, dealing with the hassle
        of unpacking them from characters. */
{
  int last_character;

  if (!((*spoke)--))
    {
      do
	{
	  (*spool) = getc (object);
	  if ((*spool) == '#')
	    {
	      do
		{
		  last_character = *spool;
		  (*spool) = getc (object);
		}
	      while ((*spool) == EOF ? 0 : (*spool) == '\n' ? (last_character == '\\') : 1);
	    }
	}
      while ((*spool) == EOF ? 0 : ((*spool) == '\n' ? 1 : 0));
      if ((*spool) == EOF ? 1 : (*spool) < 60 ? 1 : ((*spool) = (*spool) - 60) & 0xffc0)
	avm_fatal_io_error ("invalid raw file format in", filename, 0);
      (*spoke) = 5;
    }
  return (((*spool) >> (*spoke)) & 1);
}











void
avm_send_list (repository, operand, filename)
     FILE *repository;
     list operand;
     char *filename;

     /* This puts a list into a raw format file. */

{
  list front, queue, old;
  int spool, spoke, column;

  if (!initialized)
    avm_initialize_rawio ();	/* if the caller didn't do what it should */
  front = queue = NULL;
  spool = spoke = column = 0;
  avm_enqueue (&front, &queue, avm_copied (operand));
  while (front ? sent_bit (repository, front->head ? 1 : 0, filename, &spool, &spoke, &column) : 0)
    {
      if (front->head)
	{
	  avm_enqueue (&front, &queue, avm_copied (front->head->head));
	  avm_enqueue (&front, &queue, avm_copied (front->head->tail));
	}
      front = avm_copied ((old = front)->tail);
      avm_dispose (old);
    }
  while (spoke ? sent_bit (repository, 0, filename, &spool, &spoke, &column) : 0);
  if (front)
    avm_dispose (front);
  else if (putc ('\n', repository) != '\n')
    avm_non_fatal_io_error ("can't write to", filename, errno);
}









void
avm_recoverable_send_list (sockfd, operand, crc, timeout, closed, fault)
     int sockfd;
     list operand;
     char **crc;
     int *timeout;
     int *closed;
     int *fault;

     /* differs from avm_send_list by using a socket descriptor rather
        than a file, using binary rather than printable characters,
        and setting the fault parameter rather than aborting or
        writing an error message in the event of a fault; also
        computes a 32 bit cyclic redundancy check, which should be
        freed by the caller */

{
  char packet_buffer[PACKET_SIZE];
  char *cursor;
  list front, back, old;
  int words, bits, next_bit, sent, last_packet_size;

#if HAVE_GCRYPT
  if (!initialized)
    avm_initialize_rawio ();
  gcry_md_reset (hash_context);
  front = back = NULL;
  *closed = *timeout = words = bits = (int) (*(cursor = packet_buffer) = 0);
  avm_recoverable_enqueue (&front, &back, avm_copied (operand), fault);
  while (*fault ? NULL : *timeout ? NULL: front)
    {
      if (next_bit = (front->head ? 1 : 0))
	{
	  avm_recoverable_enqueue (&front, &back, avm_copied (front->head->head), fault);
	  avm_recoverable_enqueue (&front, &back, avm_copied (front->head->tail), fault);
	}
      if (!*fault)
	{
	  front = avm_copied ((old = front)->tail);
	  avm_dispose (old);
	  if (bits++ == WORD_SIZE)
	    {
	      bits = 1;
	      (*(cursor++)) += OFFSET;
	      if (words++ == PACKET_SIZE)
		{
		  words = sent = 0;
		  gcry_md_write (hash_context, packet_buffer, PACKET_SIZE);
		  while ((*timeout = (*timeout ? 1 : (sent < 0))) ? 0 : ((words += sent) < PACKET_SIZE))
		    {
		      sent = send (sockfd, &(packet_buffer[words]), PACKET_SIZE - words, MSG_NOSIGNAL | MSG_MORE);
		      *timeout = !sent;
		    }
		  *closed = (!*timeout ? 0 : !sent ? 1 : (errno == ENOTCONN) ? 1 : (errno == ECONNREFUSED));
		  words = (int) ((*(cursor = packet_buffer)) = 0);
		}
	    }
	  (*cursor) = ((*cursor) << 1) | next_bit;
	}
    }
  avm_dispose (front);
  (*cursor) = ((*cursor) << (WORD_SIZE - bits)) + OFFSET;
  if (last_packet_size = words + (bits ? 1 : 0))
    gcry_md_write (hash_context, packet_buffer, last_packet_size);
  if (!(*crc = strdup (gcry_md_read (hash_context, GCRY_MD_CRC32_RFC1510))))
    *fault = 1;
  words = sent = 0;
  if (*fault ? 0 : !(*timeout))
    while ((*timeout = (*timeout ? 1 : (sent < 0))) ? 0 : ((words += sent) < last_packet_size))
      *timeout = !(sent = send (sockfd, &(packet_buffer[words]), last_packet_size - words, MSG_NOSIGNAL));
  *closed = (*closed ? 1 : !*timeout ? 0 : (sent == 0) ? 1 : (errno == ENOTCONN) ? 1 : (errno == ECONNREFUSED));
  return;
#endif
  avm_error ("I need avram built with libgcrypt.");
}










list
avm_recoverable_received_list (sockfd, crc, timeout, closed, fault)
     int sockfd;
     char **crc;
     int *timeout;
     int *closed;
     int *fault;

     /* like received_list but using sockets and not aborting; also
        checks for the absence of spurious trailing input and computes
        a 32 bit crc, which should be freed by the caller */
{
  struct pollfd fds;
  list result;
  char packet_buffer[PACKET_SIZE];
  char *cursor;
  branch_queue front,back;
  int bits,words,received,last_packet_size;

#if HAVE_GCRYPT
  if (!initialized)
    avm_initialize_rawio ();
  gcry_md_reset (hash_context);
  front = back = NULL;
  words = bits = 0;
  cursor = packet_buffer;
  avm_recoverable_anticipate (&front, &back, &result, fault);
  *closed = *timeout = (*fault ? 0 : ((received = recv (sockfd, packet_buffer, PACKET_SIZE, MSG_NOSIGNAL)) <= 0));
  gcry_md_write (hash_context, packet_buffer, received);
  (*cursor) -= OFFSET;
  while (*fault ? NULL : *timeout ? NULL : front)
    {
      if (bits++ == WORD_SIZE)
	{
	  bits = 1;
	  cursor++;
	  if (++words == received)
	    {
              cursor = packet_buffer;
	      *timeout = ((received = recv (sockfd, packet_buffer, PACKET_SIZE, MSG_NOSIGNAL)) <= 0);
	      gcry_md_write (hash_context, packet_buffer, received);
	      *closed = (!*timeout ? 0 : (received == 0) ? 1 : (errno == ENOTCONN) ? 1 : (errno == ECONNREFUSED));
	      words = 0;
	    }
	  (*cursor) -= OFFSET;
	}
      avm_recoverable_enqueue_branch(&front, &back, ((*cursor) >> (WORD_SIZE - bits)) & 1, fault);
    }
  avm_dispose_branch_queue (front);
  if (!(*crc = strdup (gcry_md_read (hash_context, GCRY_MD_CRC32_RFC1510))))
    *fault = 1;
  fds.fd = sockfd;
  fds.events = POLLIN;
  *fault = (*fault ? 1 : (*timeout = (*timeout ? 1 : ((words + (bits ? 1 : 0)) != received))));
  if (*fault = (*fault ? 1 : (poll (&fds, 1, 0) < 0) ? 1 : (fds.revents & POLLIN)))
    {
      avm_dispose (result);
      while ((poll (&fds, 1, 0) == 0) ? (fds.revents & POLLIN) : 0)
	received = recv (sockfd, packet_buffer, PACKET_SIZE, MSG_NOSIGNAL);  /* flush spurious trailing data */
      return NULL;
    }
  return result;
#endif
  avm_error ("I need avram built with libgcrypt.");
}









list
avm_received_list (object, filename)
     FILE *object;
     char *filename;

     /* This reads a list from a file that better be in raw format. */
{
  list result;
  int spoke, spool;
  branch_queue old, front, back;

  if (!initialized)
    avm_initialize_rawio ();	/* if the caller didn't do what it should */
  spoke = spool = 0;
  front = back = NULL;
  avm_anticipate (&front, &back, &result);
  while (front)
    {
      if (*(front->above) = (received_bit (object, filename, &spoke, &spool) ? avm_join (NULL, NULL) : NULL))
	{
	  avm_anticipate (&front, &back, &((*(front->above))->head));
	  avm_anticipate (&front, &back, &((*(front->above))->tail));
	}
      front = (old = front)->following;
      avm_dispose_branch (old);
    }
  return result;
}






void
avm_initialize_rawio ()
     /* This is called at the beginning before any of the others, if
        the calling program is conforming to the specs. */
{
  int fault;

  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_branches ();
  avm_initialize_chrcodes ();
#if HAVE_GCRYPT
  fault = ! gcry_check_version (NULL);
  fault = (fault ? 1 : (gcry_control (GCRYCTL_DISABLE_SECMEM) != GPG_ERR_NO_ERROR));
  fault = (fault ? 1 : (gcry_control (GCRYCTL_SET_VERBOSITY, 0) != GPG_ERR_NO_ERROR));
  fault = (fault ? 1 : (gcry_control (GCRYCTL_INITIALIZATION_FINISHED) != GPG_ERR_NO_ERROR));
  fault = (fault ? 1 : (gcry_md_open (&hash_context, GCRY_MD_CRC32_RFC1510, 0) != GPG_ERR_NO_ERROR));
  if (fault = (fault ? 1 : ! gcry_md_is_enabled (hash_context, GCRY_MD_CRC32_RFC1510)))
    avm_error ("unable to initialize libgcrypt");
#endif
}





void
avm_count_rawio ()

     /* This is just a hook if you want to put something here; client
        programs are supposed to call this at the end of a run. */
{
  if (!initialized)
    return;
  initialized = 0;
#if HAVE_GCRYPT
  gcry_md_close (hash_context);
#endif
}
