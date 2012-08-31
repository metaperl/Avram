
/* execution of interactive applications and filter mode transducers

   Copyright (C) 2003,2007 Dennis Furey

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

#include <avm/common.h>
#include <avm/chrcodes.h>
#include <avm/error.h>
#include <avm/apply.h>
#include <avm/formin.h>
#include <avm/formout.h>
#include <avm/rawio.h>
#include <avm/exmodes.h>

#ifndef HAVE_MEMMOVE
extern void 
*memmove(char *dest, const char *source, unsigned length)
#endif

/* points to a stack of pids */
typedef struct exp_pid_node *exp_pid_stack;

/* a stack of these is needed for re-entrancy */
struct exp_pid_node
{
  int pid;
  char *name;
  exp_pid_stack other_pids;
};

/* the stack of pids whose top is referenced globally by the avm_popen and avm_pclose functions */
static exp_pid_stack top = NULL;

/* Non-zero means static variables have been initialized. */
static int initialized = 0;

/* set to true if evaluation of avm_recoverable_interact is prohibited */
static int jailed = 0;

/* set to true if the avm_recoverable_interact function should show debugging output */
static int tracing = 0;

/* error messages as lists of lists of character representations */
static list misformatted_interaction = NULL;
static list failed_interaction = NULL;
static list failed_interaction1 = NULL;
static list failed_interaction2 = NULL;
static list failed_interaction3 = NULL;
static list failed_interaction4 = NULL;
static list failed_interaction5 = NULL;
static list failed_interaction6 = NULL;
static list invalid_interaction = NULL;
static list empty_command_line = NULL;
static list memory_overflow = NULL;
static list unable_to_open = NULL;
static list unable_to_close = NULL;
static list interaction_disabled = NULL;

#if HAVE_EXPECT
extern FILE *exp_popen ();
extern int exp_pid;
#endif

#ifndef HAVE_MEMMOVE
extern void *
memmove PARAMS((char *dest, const char *source, unsigned length));
#endif







void
avm_disable_interaction () 

{
  jailed = 1;
}







void
avm_trace_interaction ()

{
  tracing = 1;
}










static list
avm_popen (pipe, output_string, tracing, stepping, fault)
     FILE **pipe;
     list output_string;
     int tracing;
     int stepping;
     int *fault;

     /* creates a process with exp_popen and makes a note of its pid */
{
#if HAVE_EXPECT
  list line;
  char *child;
  list message;
  exp_pid_stack next_top;

  if (*pipe)
    return NULL;
  if (*fault = !(next_top = (exp_pid_stack) malloc (sizeof(*next_top))))
    return avm_copied (memory_overflow);
  memset (next_top, 0, sizeof(*next_top));
  next_top->other_pids = top;
  line = avm_recoverable_join (avm_copied (output_string), NULL);
  if (*fault = !line)
    {
      free (next_top);
      return avm_copied (memory_overflow);
    }
  message = NULL;
  child = avm_recoverable_prompt (line, &message, fault);
  avm_dispose (line);
  if (*fault)
    {
      if (child)
	free (child);
      free (next_top);
      return message;
    }
  if (child ? !strlen(child) : 1)
    {
      if (child)
	free (child);
      free (next_top);
      if (tracing ? 1 : stepping)
	printf("not opening empty command\n");
      return NULL;
    }
  if (tracing)
    printf("opening %s\n",child);
  if (stepping)
    printf ("%s\n",child);
  if (*fault = !(*pipe = exp_popen (child)))
    {
      if (tracing ? 1 : stepping)
#if HAVE_STRERROR
	printf("can't open %s; %s\n", child, xstrerror (errno));
#else
        printf("can't open %s\n",child);
#endif
      free (next_top);
      free (child);
    }
  else
    {
      top = next_top;
      top->name = child;
      top->pid = exp_pid;
    }
  return (*fault ? ((errno == ENOMEM) ? avm_copied (memory_overflow) : avm_copied (failed_interaction1)) : NULL);
#endif
  *fault = 1;
  return avm_copied (failed_interaction2);
}







static list
avm_pclose (pipe, tracing, stepping, fault)
     FILE **pipe;
     int tracing;
     int stepping;
     int *fault;

     /* closes the most recently opened process and waits on its pid */
{
#if HAVE_EXPECT
  exp_pid_stack previous_top;
  list message;

  message = NULL;
  if (!*pipe)
    return message;
  if (!(fclose (*pipe)))
    {
      if (tracing)
	printf ("closing %s\n",top->name);
      wait (top->pid);
    }
  else
    {
      *fault = 1;
      message = avm_copied (unable_to_close);
      if (tracing)
	printf ("not closing %s\n",top->name);
    }
  *pipe = NULL;
  previous_top = top;
  top = top->other_pids;
  free (previous_top->name);
  free (previous_top);
  return message;
#endif
  *fault = 1;
  return avm_copied (unable_to_close);
}







static list
piped_out (pipe, output_strings, tracing, stepping, fault)
     list output_strings;
     FILE **pipe;
     int tracing;
     int stepping;
     int *fault;

     /* this sends strings out the pipe, opening if necessary with
	avm_popen and the first string */
{
  list line,message;
  int datum;

  message = NULL;
#if HAVE_EXPECT
  while (*fault ? 0 : (output_strings ? !*pipe : 0))
    {
      message = avm_popen (pipe, output_strings->head, tracing, stepping, fault);
      output_strings = output_strings->tail;
    }
  if (*fault)
    return message;
  if (!*pipe)
    return NULL;
  while (output_strings)
    {
      line = output_strings->head;
      while (line)
	{
	  if (*fault = ((datum = avm_standard_character_code (line->head)) < 0))
	    return avm_copied (misformatted_interaction);
	  else if (*fault = (putc (datum, *pipe) != datum))
	    {
	      avm_dispose (avm_pclose (pipe, tracing, stepping, fault));
	      return avm_copied (failed_interaction3);
	    }
	  if (tracing)
	    printf("<- %c %u\n",datum,datum);
	  line = line->tail;
	}
      output_strings = output_strings->tail;
      if (output_strings)
	{
	  if (*fault = (putc (10,*pipe) != 10))
	    {
	      avm_dispose (avm_pclose (pipe, tracing, stepping, fault));
	      return avm_copied (failed_interaction4);
	    }
	  if (tracing)
	    printf("<-   10\n");
 	}
    }
#endif
  return message;
}





static list
line_up(front_column, back_column, front_line, back_line, datum, delayed_cr, fault)
     list *front_column;
     list *back_column;
     list *front_line;
     list *back_line;
     int datum;
     int *delayed_cr;
     int *fault;

     /* thia enqueues a datum representing a single character into a
	list of lists of strings, interpreting line breaks and
	suppressing terminating carriage returns */
{
  if (*fault)
    return NULL;
  if (datum == 10)
    {
      avm_recoverable_enqueue (front_line, back_line, *front_column, fault);
      *front_column = *back_column = NULL;
    }
  else if (!(datum == 13 ? 1 : datum == EOF))
    {
      if (*delayed_cr)
	avm_recoverable_enqueue (front_column, back_column, avm_standard_character_representation (13), fault);
      if (!*fault)
	avm_recoverable_enqueue (front_column, back_column, avm_standard_character_representation (datum), fault);
    }
  *delayed_cr = (datum == 13);
  if (*fault)
    {
      avm_dispose (*front_line);
      avm_dispose (*front_column);
      *front_line = *back_line = NULL;
      *front_column = *back_column = NULL;
      return avm_copied (memory_overflow);
    }
  return NULL;
}






static list
match (pipe, pattern, tracing, stepping, fault)
     FILE **pipe;
     char *pattern;
     int tracing;
     int stepping;
     int *fault;

     /* this collects strings from the pipe until the pattern is
	matched */
{
  list message,front_column,back_column,front_line,back_line;
  char *shift_register;
  char *shift_register_port;
  int pattern_length,datum;
  int delayed_cr;

  message = NULL;
  front_line = back_line = NULL;
  front_column = back_column = NULL;
#if HAVE_EXPECT
  if (*fault = !(shift_register = strdup (pattern)))
    return avm_copied (memory_overflow);
  pattern_length = strlen (pattern);
  shift_register_port = shift_register;
  while (*shift_register_port)
    *(shift_register_port++) = '\0';
  shift_register_port--;
  delayed_cr = 0;
  while (*fault ? 0 : (*pipe ? (strcmp (pattern, shift_register) ? ((datum = getc (*pipe)) != EOF) : 0) : 0))
    {
      if (tracing)
	printf("-> %c %u\n",datum < 32 ? ' ' : (datum > 126 ? ' ' : datum),datum);
      if (stepping)
	printf ("%c",datum);
      memmove (shift_register, shift_register + 1, pattern_length);
      *shift_register_port = datum;
      message = line_up (&front_column, &back_column, &front_line, &back_line, datum, &delayed_cr, fault);
    }
  free (shift_register);
  if (*fault)
    avm_dispose (avm_pclose (pipe, tracing, stepping, fault));
  else if (datum == EOF)
    message = avm_pclose (pipe, tracing, stepping, fault);
  if (*fault)
    {
      avm_dispose (front_line);
      avm_dispose (front_column);
      return message;
    }
  if (front_column)
    avm_recoverable_enqueue (&front_line, &back_line, front_column, fault);
  if (*fault)
    return avm_copied (memory_overflow);
#endif
  return front_line;
}







static list
piped_in (pipe, prompt_strings, tracing, stepping, fault)
     FILE **pipe;
     list prompt_strings;
     int tracing;
     int stepping;
     int *fault;

     /* this collects strings from the pipe until the pattern encoded
	by the prompt is matched */
{
  char *pattern;
  char *pattern_port;
  list message;

  message = NULL;
#if HAVE_EXPECT
  pattern = avm_recoverable_prompt (prompt_strings, &message, fault);
  if (*fault)
    return (message);
  if (*fault = !pattern)
    return avm_copied (failed_interaction5);
  if (tracing)
    {
      printf("waiting for ");
      pattern_port = pattern;
      while (*pattern_port)
        if (*(pattern_port) == 4)
	  printf("EOF ",*(pattern_port++));
	else
	  printf("%u ",*(pattern_port++));
      if (!*pattern)
	printf("nothing");
      printf("\n");
    }
  message = match (pipe, pattern, tracing, stepping, fault);
  if (tracing)
    {
      if (!*pipe)
	printf ("received EOF\n");
      else if (!*fault)
	printf("matched\n");
    }
  free (pattern);
#endif
  return message;
}












static list
dripped_in (pipe, tracing, stepping, fault)
     FILE **pipe;
     int tracing;
     int stepping;
     int *fault;

     /* this reads a single character from the pipe */
{
  int datum;
  list message;

  message = NULL;
  if (*fault)
    return message;
#if HAVE_EXPECT
  datum = getc (*pipe);
  if (tracing)
    printf("-> %c %u\n",datum < 32 ? ' ' : (datum > 126 ? ' ' : datum),datum);
  if (stepping)
    printf("%c",datum);
  if (datum == EOF)
    {
      if (tracing)
	printf ("received EOF\n");
      message = avm_pclose (pipe, tracing, stepping, fault);
    }
  if (!*fault)
    return avm_standard_character_representation (datum);
#endif
  return message;
}








static void
step (pipe, interactor, interaction, step_mode, ask_to_overwrite_mode, quiet_mode, fault)
     FILE **pipe;
     list interactor;
     list *interaction;
     int step_mode;
     int ask_to_overwrite_mode;
     int quiet_mode;
     int *fault;

     /* this function performs one step of interaction between a
	program invoked with the --interactive command line option and
	the shell */
{
  list configuration,message;

  if (*fault = (*fault ? 1 : !*interaction))
    return;
  if (!((*interaction)->tail))
    {
      avm_output_as_directed ((*interaction)->head, ask_to_overwrite_mode, !quiet_mode);
      configuration = avm_copied (*interaction);
    }
  else
    {
      message = NULL;
      avm_dispose (piped_out (pipe, (*interaction)->tail->head, 0, step_mode, fault));
      if (*fault ? NULL : (*interaction)->tail->tail)
	message = piped_in (pipe, (*interaction)->tail->tail, 0, step_mode, fault);
      else if (!*fault)
	message = dripped_in (pipe, 0, step_mode, fault);
      if (*fault)
	{
	  avm_dispose (message);
	  avm_dispose (*interaction);
	  *interaction = NULL;
	  return;
	}
      if (*fault = !(configuration = avm_recoverable_join (avm_copied((*interaction)->head), message)))
	{
	  avm_dispose (avm_pclose (pipe, 0, step_mode, fault));
	  avm_dispose (*interaction);
	  *interaction = NULL;
	  return;
	}
    }
  avm_dispose (*interaction);
  *interaction = avm_recoverable_apply (avm_copied (interactor), configuration, fault);
  if (*fault)
    avm_dispose (avm_pclose (pipe, 0, step_mode, fault));
  return;
}






void
avm_interact (interactor, step_mode, ask_to_overwrite_mode, quiet_mode)
     list interactor;
     int step_mode;
     int ask_to_overwrite_mode;
     int quiet_mode;

/* This function executes programs invoked with the --interact command
   line option.  The interactor function is initially applied to NULL
   and is expected to return one of four possible results.

1) a NULL result
2) a result of the form (file list,NULL)
3) a result of the form (state,(output strings,NULL))
4) a result of the form (state,(output strings,prompt strings))

Depending on the result returned, one of four possible things happens.

1) If it returns NULL, nothing more is done and the program
   terminates.
2) If it returns (file list,NULL), the file list is output using
   avm_output_as_directed, the interactor is applied to (file list,NULL),
   and the cycle continues.
3) If it returns (state,(output strings,NULL)), then the output
   strings are sent down a pipe, a single character c is read from the
   pipe, the interactor is applied to (state,c), and the cycle continues.
4) If it returns (state,(output strings,prompt strings)), then the
   output strings are sent down a pipe, input strings are received from
   the pipe until a squence of input strings matching the prompt strings
   is detected, the interactor is applied to (state,input strings) and
   the cycle continues.

In the third and fourth cases, above, where output has to be sent down
a pipe, the pipe is opened with exp_popen if it hasn't been opened
already on a previous cycle. The argument to exp_popen in such cases
is the first string in the list of output strings, and only the rest
of the strings get sent down the pipe.

If an EOF is read from the pipe at any time, the pipe is closed. If a
pipe is closed due to EOF in the third case, the interactor is applied
to (state,NULL) rather than to (state,c). If a pipe is closed due to
EOF in the fourth case, the interactor is applied to the output
strings truncated at the EOF regardless of the prompt strings. If the
interactor returns more output strings after the pipe has been closed,
a new pipe is opened using exp_popen with the first output string, as
before, and the rest of the output strings are sent down the pipe.

Empty lists of output strings are handled as follows. If a pipe needs
to be opened for reading but can't be opened because the list of
output strings is empty (and hence indicates no argument to
exp_popen), the pipe is not opened and the effect is the same as if
EOF had been read from it. If a pipe is already opened and the list of
output strings is empty, nothing is written to it but reading proceeds
normally.

An empty list of prompt strings is not interpreted as such because the
NULL value is taken to imply character oriented interaction per case
3. There is a danger of deadlock if the author of the interactor
misunderstands the use of an empty list of prompt strings to mean that
the interactor will be invoked again immediately without waiting for
input from the pipe.  This effect can be achieved instead by the use
of a list of prompt strings containing only the empty string.

The expect library puts a carriage return at the end of every line
that is read from the pipe in addition to separating the lines with
line feeds. The carriage returns are stripped in the case of line
oriented interaction (case 4) but retained in the case of character
oriented interaction (case 3). The prompt strings returned by the
interactor should not include trailing carriage returns for the sake
of matching the input read from the pipe, because they are
automatically added by the prompt function, above. Embedded carriage
returns (as opposed to trailing) are not stripped.

If the author of the interactor function wishes to execute a
non-interactive command (e.g., ls or pwd) and read all output from it
without further interaction, the interactor should use a list of
prompt strings containing only the single string containing only the
single character ascii 4 (for EOF) or any other character that is
certain not to occur in the output of the command. */

{
  FILE *pipe;
  list interaction,configuration,message;
  int fault;

#if HAVE_EXPECT
  pipe = NULL;
  fault = 0;
  if (!initialized)
    avm_initialize_exmodes ();
  if (!interactor)
    avm_error ("invalid interaction");
  if (jailed)
    avm_error ("interaction disabled");
  interaction = avm_apply (avm_copied (interactor), NULL);
  while (fault ? 0 : interaction)
    {
      step (&pipe, interactor, &interaction, step_mode, ask_to_overwrite_mode, quiet_mode, &fault);
      if (fault ? 0 : step_mode)
	{
	  fflush (stdout);
	  getchar ();
	}
    }
  avm_dispose (avm_pclose (&pipe, 0, step_mode, &fault));
  if (fault)
    avm_error ("failed interaction (code 0)");
  return;
#endif /* HAVE_EXPECT */
    avm_error ("I need avram linked with libexpect.");
}









static list
transition (pipe, interactor, interaction, transcript_front, transcript_back, tracing, fault)
     FILE **pipe;
     list interactor;
     list *interaction;
     list *transcript_front;
     list *transcript_back;
     int tracing;
     int *fault;

     /* This function performs one step of interaction between a
	transducer and an external application. Interactor is the
	state transition function, interaction is the current
	configuration, and transcript front and back are a queue of
	text in both directions. */
{
  list message;
  list configuration;

  *fault = (*fault ? 1 : !((*interaction) ? (*interaction)->tail : NULL));
  message = (*fault ? NULL : piped_out (pipe, (*interaction)->tail->head, tracing, 0, fault));
  if (*fault ? NULL : (*interaction)->tail->tail)
    message = piped_in (pipe, (*interaction)->tail->tail, tracing, 0, fault);
  else if (!*fault)
    message = dripped_in (pipe, tracing, 0, fault);
  if (*fault)
    return (message ? message : avm_copied (invalid_interaction));
  avm_recoverable_enqueue (transcript_front, transcript_back, avm_copied ((*interaction)->tail->head), fault);
  if (!*fault)
    avm_recoverable_enqueue (transcript_front, transcript_back, avm_copied (message), fault);
  if (*fault)
    {
      avm_dispose (*interaction);
      *interaction = NULL;
      avm_dispose (message);
      return avm_copied (memory_overflow);
    }
  configuration = avm_recoverable_join (avm_copied ((*interaction)->head), message);
  avm_dispose (*interaction);
  message = *interaction = NULL;
  if (*fault = !configuration)
    return avm_copied (memory_overflow);
  *interaction = avm_recoverable_apply (avm_copied (interactor), configuration, fault);
  if (*fault)
    {
      message = *interaction;
      *interaction = NULL;
    }
  return message;
}





list
avm_recoverable_interact (interactor, fault)
     list interactor;
     int *fault;

     /* This function implements the interact combinator. It is
	similar to avm_interact but always closes the pipe and
	performs no file i/o, and will return an error rather than
	exiting.  Otherwise it returns a transcript of the
	interaction as a list of lists of strings represented as
	lists of character encodings. */
{
  FILE *pipe;
  list interaction,transcript_front,transcript_back,configuration,message;

#if HAVE_EXPECT
  pipe = NULL;
  transcript_front = transcript_back = NULL;
  if (!initialized)
    avm_initialize_exmodes ();
  if (*fault = (*fault ? 1 : !interactor))
    return avm_copied (invalid_interaction);
  if (*fault = jailed)
    return avm_copied (interaction_disabled);
  interaction = avm_recoverable_apply (avm_copied (interactor), NULL, fault);
  if (*fault)
    return interaction;
  while (*fault ? NULL : interaction)
    message = transition (&pipe, interactor, &interaction, &transcript_front, &transcript_back, tracing, fault);
  avm_dispose (interaction);
  if (*fault)
    {
      avm_dispose (avm_pclose (&pipe, tracing, 0, fault));
      avm_dispose (transcript_front);
      return message;
    }
  message = avm_pclose (&pipe, tracing, 0, fault);
  if (*fault)
    {
      avm_dispose (transcript_front);
      return message;
    }
  return transcript_front;
#endif /* HAVE_EXPECT */
  *fault = 1;
  return avm_copied (failed_interaction6);
}












void
avm_line_map (operator)
     list operator;

     /* applies a function to each line of standard input and sends
        the result to standard output, which should be a character
        string */
{
  int datum;
  list front, back, result;

  if (!initialized)
    avm_initialize_exmodes ();
  if ((datum = getc (stdin)) != EOF)
    {
      front = back = NULL;
      while (datum != '\n' ? datum != EOF : 0)
	{
	  avm_enqueue (&front, &back, avm_character_representation (datum));
	  datum = getc (stdin);
	}
      avm_put_bytes (result = avm_apply (avm_copied (operator), front));
      avm_dispose (result);
      while (datum == '\n')
	{
	  if (putc ('\n', stdout) != '\n')
	    avm_fatal_io_error ("can't write to", "standard output", errno);
	  front = back = NULL;
	  while ((datum = getc (stdin)) != '\n' ? datum != EOF : 0)
	    avm_enqueue (&front, &back, avm_character_representation (datum));
	  avm_put_bytes (result = avm_apply (avm_copied (operator), front));
	  avm_dispose (result);
	}
    }
  avm_dispose (operator);
}






void
avm_byte_transduce (operator)
     list operator;

     /* This uses a function as a transducer, taking each byte of standard
        input as input, and treating each output as a character string
        to go to standard output. */
{
  int datum;
  int end_of_file = 0;
  int ioerror = 0;
  list state_and_output, state, operand;

  if (!initialized)
    avm_initialize_exmodes ();
  state_and_output = avm_apply (avm_copied (operator), NULL);
  while (state_and_output ? !ioerror : 0)
    {
      avm_put_bytes (state_and_output->tail);
      state = avm_copied (state_and_output->head);
      avm_dispose (state_and_output);
      end_of_file = end_of_file ? 1 : ((datum = getc (stdin)) == EOF);
      operand = avm_join (state, end_of_file ? NULL : avm_character_representation (datum));
      state_and_output = avm_apply (avm_copied (operator), operand);
    }
  avm_dispose (operator);
}










void
avm_initialize_exmodes ()
     /* This initializes static data structures. */
{
  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_formin ();
  avm_initialize_formout ();
  avm_initialize_rawio ();
  avm_initialize_apply ();
  misformatted_interaction = avm_join (avm_strung ("misformatted interaction"), NULL);
  interaction_disabled = avm_join (avm_strung ("interaction disabled"), NULL);
  invalid_interaction = avm_join (avm_strung ("invalid interaction"), NULL);
  empty_command_line = avm_join (avm_strung ("empty command line"), NULL);
  failed_interaction = avm_join (avm_strung ("failed interaction"), NULL);
  failed_interaction1 = avm_join (avm_strung ("failed interaction (code 1)"), NULL);
  failed_interaction2 = avm_join (avm_strung ("failed interaction (code 2)"), NULL);
  failed_interaction3 = avm_join (avm_strung ("failed interaction (code 3)"), NULL);
  failed_interaction4 = avm_join (avm_strung ("failed interaction (code 4)"), NULL);
  failed_interaction5 = avm_join (avm_strung ("failed interaction (code 5)"), NULL);
  failed_interaction6 = avm_join (avm_strung ("I need avram linked with libexpect."), NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  unable_to_close = avm_join (avm_strung ("unable to close"), NULL);
  unable_to_open = avm_join (avm_strung ("unable to open"), NULL);
}





void
avm_count_exmodes ()

{
  if (!initialized)
    return;
  avm_dispose (interaction_disabled);
  avm_dispose (misformatted_interaction);
  avm_dispose (invalid_interaction);
  avm_dispose (empty_command_line);
  avm_dispose (failed_interaction1);
  avm_dispose (failed_interaction2);
  avm_dispose (failed_interaction3);
  avm_dispose (failed_interaction4);
  avm_dispose (failed_interaction5);
  avm_dispose (failed_interaction6);
  avm_dispose (failed_interaction);
  avm_dispose (memory_overflow);
  avm_dispose (unable_to_open);
  avm_dispose (unable_to_close);
  misformatted_interaction = NULL;
  interaction_disabled = NULL;
  invalid_interaction = NULL;
  empty_command_line = NULL;
  failed_interaction = NULL;
  memory_overflow = NULL;
  unable_to_close = NULL;
  unable_to_open = NULL;
  initialized = 0;
}
