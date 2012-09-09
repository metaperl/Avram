
/* contains the universal function and some supporting operations

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

#include <avm/common.h>
#include <avm/error.h>
#include <avm/vman.h>
#include <avm/chrcodes.h>
#include <avm/compare.h>
#include <avm/listfuns.h>
#include <avm/libfuns.h>
#include <avm/decons.h>
#include <avm/instruct.h>
#include <avm/portals.h>
#include <avm/ports.h>
#include <avm/profile.h>
#include <avm/formout.h>
#include <avm/exmodes.h>
#include <avm/remote.h>
#include <avm/apply.h>

/* Non-zero means static variables have been initialized. */
static int initialized = 0;

/* This is the virtual code for a function that transforms arbitary
   virtual code into that which employs only the combinators that are
   known to the avm_apply function. */
static list interpreter;

/* These are all error messages represented as lists of lists of
   character representations. */
static list unsupported_hook;
static list invalid_deconstruction;
static list invalid_recursion;
static list memory_overflow;
static list reset;

/* This is the current or emulated version number of avram represented
   as a list of character representations. */
static list avram_version_number;

/* this message, when embedded in a program using the note combinator,
   indicates that map and reduce combinators in the descendents should
   be executed remotely if possible */
static list parallel;
static list nondeterministic;

/* if a concurrent thread sets this, avm_apply will interrupt itself,
   and so will avm_harvest */
flag _avm_reset = 0;






static counter
exception (errors, additional_errors)
     counter errors;
     int additional_errors;

     /* This adds to an error counter and checks for overflow. */
{
  counter result;

  if (!additional_errors)
    return (errors);
  else if ((result = errors + additional_errors) < errors)
    avm_error ("counter overflow (code 4)");
  return (result);
}









static void
dispatch (errors, result, client, next)
     counter errors;
     list result;
     port client;
     instruction *next;

     /* This routine plugs a result into the appropriate port, joins
        results of constructions, and chooses conditional
        branches. The caller should regard the result parameter as
        having been disposed of. */
{
  port old_client;
  portal old;
  int done;

  if (done = !client)
    avm_internal_error (7);
  else
    do
      {
	if (errors != client->errors)
	  {
	    if (client->predicating)
	      {
		if (!*next)
		  avm_internal_error (8);
		else
		  {
		    avm_sever (client);
		    client = (*next)->client;
		    avm_retire (next);
		    avm_retire (next);
		  }
	      }
	    else if (client->parent)
	      {
		if (!((client = (old_client = client)->parent)->descendents->left))
		  avm_internal_error (9);
		else if (old_client == client->descendents->left)
		  avm_retire (next);
		else
		  avm_dispose (client->descendents->left->contents);
		avm_sever (client->descendents->left);
		avm_sever (client->descendents->right);
	      }
	    else if (*next)
	      {
		client = (*next)->client;
		avm_retire (next);
	      }
	  }
	if (errors != client->errors)
	  {
	    if (done = (*next ? 0 : !(client->parent)))
	      {
		client->contents = result;
		client->errors = errors;
	      }
	  }
	else if (done = client->predicating)
	  {
	    if (result)
	      avm_dispose (result);
	    else
	      avm_reschedule (next);
	    avm_retire (next);
	    avm_sever (client);
	  }
	else
	  {
	    client->contents = result;
	    if (client->impetus)
	      {
		if (client->impetus->interpretation)
		  avm_internal_error (10);
		client->impetus->interpretation = avm_copied (result);
	      }
	    if (!(done=!(client->parent ? (client == client->parent->descendents->right) : 0)))
	      {
		if (!((client=(old_client=client)->parent)->descendents->left)?errors!=client->descendents->left->errors:0)
		  avm_internal_error (11);
		if (!(result = avm_recoverable_join (client->descendents->left->contents, result)))
		  {
		    result = avm_copied (memory_overflow);
		    errors = exception (errors, 1);
		  }
		avm_sever (client->descendents->left);
		old = client->descendents;
		client->descendents = old->alters;
		avm_seal (old);
		avm_sever (old_client);
	      }
	  }
      }
    while (!done);
}








list
avm_recoverable_apply (operator, operand, fault)
     list operator;
     list operand;
     int *fault;

     /* This routine computes the universal function, setting the
        fault to true in the event of an exception. The function takes
        the form of a big case statement, and operates on a stack of
        instructions.  Each instruction has an operator and operand to
        be evaluated together, and a pointer to a place to store the
        result of the evaluation. The pointer will point to the result
        field of some instruction below it in the stack, except in the
        case of the bottom instruction, where it points to the result
        to be returned, and in the case of a predicate to a
        conditional combinator, which points to the bit bucket. The
        form macro takes the top operator and decides what operation
        it represents. If it can't be evaluated immediately, it is
        broken up into smaller operations that are pushed onto the
        stack. If the form of the operator is not recognized, it is
        translated to a simpler form by applying the interpreter to
        it, which is done by pushing the interpreter and the operator
        onto the stack. If an operator was previously translated in
        this way, the result of the translation might still be lying
        around and will be reused. */
{
  list message;
  port bit_bucket;
  score new_sheet;
  instruction current;
  struct avm_packet result;
  portal new_descendent;
  int success, overflow;
  flag balanceable;

  enum forms
  { INTERPRETIVE, CONSTANT, CONDITIONAL, COMPOSITION, CONSTRUCTION, GUARD,
    DECONSTRUCT, REFER, RECUR, HOOK, COMPARATOR, RACE, REVERSE, HAVE, MAP, SORT,
    CONCATENATE, DISTRIBUTE, TRANSPOSE, PROFILE, NOTE, WEIGHT, MEMBER, KNOWN, REDUCE,
    DECONSTRUCT_LEFT, DECONSTRUCT_RIGHT, IDENTITY, LIBRARY, VERSION_NUMBER, INTERACTIVE
  };

#define rex current->remotely_executable
#define gr current->granularity
#define ba current->non_deterministic
#define elevel current->datum.errors
#define program current->actor.contents
#define pointer program->tail
#define false_branch program->tail
#define failure exception(elevel,1)
#define predicate program->head->head
#define exception_handler program->tail
#define true_branch program->head->tail
#define right_side program->tail
#define next (current->dependents)
#define constant_value program->head->tail
#define destination current->client
#define left_compositor program->head->head
#define right_compositor program->head->tail
#define player program->tail->tail->tail->head->head
#define noted_operator program->tail->tail->tail->tail->head
#define notation program->tail->tail->tail->tail->tail
#define team program->tail->tail->tail->head->tail
#define left_side program->head->head
#define guarded_operator program->head->tail
#define recurrence program->head->head->tail
#define reference program->head->head->head
#define left_racer program->tail->tail->head
#define right_racer program->tail->tail->tail
#define library_name program->tail->head->head
#define function_name program->tail->head->tail
#define have_library_name program->tail->head->tail
#define have_function_name program->tail->tail->tail
#define robot program->tail->tail->head->tail
#define argument (current->datum.contents)
#define game current->sheet
#define left_deconstruction (argument?avm_copied(argument->head):avm_copied(invalid_deconstruction))
#define right_deconstruction (argument?avm_copied(argument->tail):avm_copied(invalid_deconstruction))
#define mapped_function program->tail->head->tail
#define reduced_function program->tail->head->head
#define vacuous_case program->tail->head->tail
#define sorting_predicate program->tail->head->head

#define form(o) o?(\
   o->interpretation?\
      KNOWN:\
      o->head?(\
         o->tail?(\
            o->head->head?(\
               o->head->tail?\
                  CONDITIONAL:\
                  CONSTRUCTION):\
               o->head->tail?\
                  GUARD:\
                  o->tail->head?(\
                     o->tail->tail?(\
                        o->tail->head->head?(\
                           o->tail->head->tail?(\
                              o->tail->tail->head?\
                                 INTERPRETIVE:\
                                 o->tail->tail->tail?\
                                    INTERPRETIVE:\
                                    LIBRARY):\
                              o->tail->tail->head?(\
                                 o->tail->tail->tail?\
                                    INTERPRETIVE:\
                                    o->tail->head->head->head?\
                                       INTERPRETIVE:\
                                       o->tail->head->head->tail?\
                                          INTERPRETIVE:\
                                          o->tail->tail->head->head?\
                                             INTERPRETIVE:\
                                             o->tail->tail->head->tail?\
                                                INTERACTIVE:\
                                                INTERPRETIVE):\
                                 o->tail->tail->tail?\
                                    INTERPRETIVE:\
                                    o->tail->head->tail?\
                                       INTERPRETIVE:\
                                       SORT):\
                           o->tail->head->tail?(\
                              o->tail->tail->head?\
                                 INTERPRETIVE:\
                                 o->tail->tail->tail?\
                                    HAVE:\
                                    INTERPRETIVE):\
                              o->tail->tail->head?(\
                                 o->tail->tail->tail?\
                                    RACE:\
                                    INTERPRETIVE):\
                                 o->tail->tail->tail?(\
                                    o->tail->tail->tail->head?(\
                                       o->tail->tail->tail->head->head?\
                                          PROFILE:\
                                          o->tail->tail->tail->head->tail?\
                                             HOOK:\
                                             VERSION_NUMBER):\
                                       o->tail->tail->tail->tail?(\
                                          o->tail->tail->tail->tail->head?\
                                             NOTE:\
                                             HOOK):\
                                          WEIGHT):\
                                    TRANSPOSE):\
		        o->tail->head->head?\
                           REDUCE:\
                           o->tail->head->tail?\
                              MAP:\
			      MEMBER):		\
                     o->tail->tail?(\
                        o->tail->tail->head?\
                           INTERPRETIVE:\
                           o->tail->tail->tail?\
                              INTERPRETIVE:\
                              REVERSE):\
                        CONCATENATE):\
            o->head->head?(\
               o->head->tail?\
                  COMPOSITION:\
                  o->head->head->head?(\
                     o->head->head->tail?\
                        INTERPRETIVE:\
                        REFER):\
                     o->head->head->tail?\
                        RECUR:\
                        DISTRIBUTE):\
               CONSTANT):\
         o->tail?\
            DECONSTRUCT:\
            COMPARATOR):\
   INTERPRETIVE

  if (!initialized)
    avm_initialize_apply ();
  if (!operator)
    avm_error ("empty operator");
  current = NULL;
  memset (&result, overflow = 0, sizeof (result));
  success = avm_scheduled (operator, 0, operand, &result, &current, avm_entries (NULL, &message, fault), 0, 0, 0);
  avm_dispose (operand);
  avm_dispose (operator);
  if (result.errors = *fault)
    result.contents = avm_copied (message);
  else if (result.errors = !(success ? success-- : 0))
    result.contents = avm_copied (memory_overflow);
  else
      while (current)
	{
	  (game->reductions)++;
	  switch (form (program))
	    {
	    case RACE:
	    case HOOK: dispatch (failure, avm_copied (unsupported_hook), destination, &next);
	      break;
	    case CONSTANT: dispatch (elevel, avm_copied (constant_value), destination, &next);
	      break;
	    case VERSION_NUMBER: dispatch (elevel, avm_copied (avram_version_number), destination, &next);
	      break;
	    case KNOWN:
	      overflow = ! avm_scheduled (program->interpretation, elevel, argument, destination, &next, game, rex, ba, gr);
	      break;
	    case REVERSE: message = avm_reversal (argument, fault);
	      dispatch (exception (elevel, *fault), message, destination, &next);
	      break;
	    case INTERACTIVE: message = avm_recoverable_interact (robot, fault);
	      dispatch (exception (elevel, *fault), message, destination, &next);
	      break;
	    case WEIGHT: message = avm_measurement (argument, fault);
	      dispatch (exception (elevel, *fault), message, destination, &next);
	      break;
	    case DISTRIBUTE: message = avm_distribution (argument, fault);
	      dispatch (exception (elevel, *fault), message, destination, &next);
	      break;
	    case DECONSTRUCT: message = avm_deconstruction (pointer, argument, fault);
	      dispatch (exception (elevel, *fault), message, destination, &next);
	      break;
	    case CONCATENATE: message = avm_concatenation (argument, fault);
	      dispatch (exception (elevel, *fault), message, destination, &next);
	      break;
	    case TRANSPOSE: message = avm_transposition (avm_copied (argument), fault);
	      dispatch (exception (elevel, *fault), message, destination, &next);
	      break;
	    case MEMBER: message = avm_membership (argument, fault);
	      dispatch (exception (elevel, *fault), message, destination, &next);
	      break;
	    case COMPARATOR: message = avm_comparison (argument, fault);
	      dispatch (exception (elevel, *fault), message, destination,&next);
	      break;
	    case HAVE: message = avm_have_library_call (have_library_name, have_function_name, fault);
	      dispatch (exception (elevel, *fault), message, destination, &next);
	      break;
	    case COMPOSITION: success = avm_scheduled (left_compositor, elevel, NULL, destination, &next, game, rex, ba, gr);
	      success += avm_scheduled (right_compositor, elevel, argument, &(next->datum), &next, game, rex, ba, gr);
	      overflow=(success < 2);
	      break;
	    case GUARD: success = avm_scheduled (exception_handler, failure, NULL, destination,&next, game, rex, ba, gr);
	      success += avm_scheduled(guarded_operator, elevel, argument, &(next->datum), &next, game, rex, ba, gr);
	      overflow=(success < 2);
	      break;
	    case REFER: if (!(overflow=!(message =avm_recoverable_join (avm_copied (reference), avm_copied (argument)))))
		overflow = !avm_scheduled (reference, elevel, message, destination, &next, game, rex, ba, gr);
	      avm_dispose (message);
	      break;
	    case MAP: if (current->remotely_executable)
		{
		  current->remotely_executable = 0;
		  if (avm_remotely_mapped (mapped_function, argument, &message, current->granularity, fault))
		    {
		      dispatch (exception (elevel, *fault), message, destination, &next);
		      break;
		    }
		}
	    case REDUCE: if (current->remotely_executable)
		{
		  current->remotely_executable = 0;
		  if (avm_remotely_reduced (reduced_function, vacuous_case, argument, &message, current->granularity, 
					    current->non_deterministic, fault))
		    {
		      dispatch (exception (elevel, *fault), message, destination, &next);
		      break;
		    }
		}
	    case SORT: if (current->remotely_executable)
		{
		  current->remotely_executable = 0;
		  if (avm_remotely_sorted (sorting_predicate, argument, &message, current->granularity, fault))
		    {
		      dispatch (exception (elevel, *fault), message, destination, &next);
		      break;
		    }
		}
	    case INTERPRETIVE: success = avm_scheduled (NULL, elevel, argument, destination, &next,game, rex, ba, gr);
	      if (success ? (program->internal) : 0)
		(next->actor.impetus = program)->facilitator = &(next->actor);
	      overflow = ((success += avm_scheduled (interpreter, 0, program, &(next->actor), &next, game, rex, ba, gr)) < 2);
	      break;
	    case PROFILE: new_sheet = avm_entries (team, &message, fault);
	      if (*fault)
		dispatch (failure, avm_copied (message), destination, &next);
	      else
		overflow = !avm_scheduled (player, elevel, argument, destination, &next, new_sheet, rex, ba, gr);
	      break;
	    case CONDITIONAL: if (!(overflow = !(bit_bucket = avm_newport (elevel, NULL, 1))))
		{
		  success = avm_scheduled (true_branch, elevel, argument, destination, &next, game, rex, ba, gr);
		  success += avm_scheduled (false_branch, elevel, argument, destination, &next, game, rex, ba, gr);
		  success += avm_scheduled (predicate, elevel, argument, bit_bucket, &next, game, rex, ba, gr);
		  if (overflow = (success < 3))
		    avm_sever (bit_bucket);
		}
	      break;
	    case LIBRARY: library_name->characterization = function_name->characterization = 0;
	      if (program->characterization > 255)
		{                                                                      /* avoid repeated lookups */
		  library_name->characterization = program->characterization >> 8;
		  function_name->characterization = program->characterization & 0xff;
		}
	      message = avm_library_call (library_name, function_name, argument, fault);
	      program->characterization = (library_name->characterization << 8) | function_name->characterization;
	      dispatch (exception (elevel, *fault), message, destination,&next);
	      break;
	    case RECUR: message = avm_deconstruction (recurrence, argument, fault);
	      if (*fault ? 1 : !(message ? message->head : 0))
		dispatch (failure, avm_copied (invalid_recursion),destination, &next);
	      else
		overflow = !avm_scheduled (message->head, elevel, message,destination, &next, game, rex, ba, gr);
	      avm_dispose (message);
	      break;
	    case NOTE: 
	      if (current->remotely_executable ? NULL : notation)
		{
		  message = avm_binary_comparison (notation->head, nondeterministic, fault);
		  balanceable = ! ! message;
		  message = (*fault ? message : message ? message : avm_binary_comparison (notation->head, parallel, fault));
		  if (*fault)
		    dispatch (exception (elevel, *fault), message, destination, &next);
		  else if (message)
		    {
		      current->remotely_executable = 1;
		      current->non_deterministic = balanceable;
		      current->granularity = avm_counter (notation->tail);
		      avm_dispose (message);
		    }
		}
	      if (!overflow)
		overflow = ! avm_scheduled (noted_operator, elevel, argument, destination, &next, game, rex, ba, gr);
	      break;
	    case CONSTRUCTION:
	      if (current->remotely_executable)
		{
		  current->remotely_executable = 0;
		  if (avm_remotely_constructed (left_side, right_side, argument, &message, fault))
		    {
		      dispatch (exception (elevel, *fault), message, destination, &next);
		      break;
		    }
		}
	      if (!(overflow = !(new_descendent = avm_new_portal (destination->descendents))))
		{
		  if (overflow = !(new_descendent->left = avm_newport (elevel, destination, 0)))
		    avm_seal (new_descendent);
		  else if (overflow = !(new_descendent->right = avm_newport (elevel, destination, 0)))
		    {
		      avm_sever (new_descendent->left);
		      avm_seal (new_descendent);
		    }
		  else
		    {
		      success = avm_scheduled (right_side, elevel, argument, new_descendent->right, &next, game, rex, ba, gr);
		      success += avm_scheduled (left_side, elevel, argument, new_descendent->left, &next, game, rex, ba, gr);
		      if (!(overflow = (success < 2)))
			destination->descendents = new_descendent;
		      else
			{
			  avm_sever (new_descendent->left);
			  avm_sever (new_descendent->right);
			  avm_seal (new_descendent);
			}
		    }
		}
	      break;
	    default: avm_internal_error (12);
	    }
	  if (_avm_reset)
	    dispatch (failure, avm_copied (reset), destination, &next);
	  else if (overflow ? overflow-- : 0)
	    {
	      while (success ? success-- : 0)
		avm_retire (&next);
	      dispatch (failure, avm_copied (memory_overflow), destination, &next);
	    }
	  avm_retire (&(current));
	}
  *fault = (*fault ? 1 : result.errors);
  return (result.contents);
}







list
avm_apply (operator, operand)
     list operator;
     list operand;

     /* This routine computes the universal function, printing an
        error message and exiting in the event of an exception. This
        is the one that's used by avram. */
{
  int fault;
  list result;

  fault = 0;
  result = avm_recoverable_apply (operator, operand, &fault);
  if (fault)
    {
      avm_output (stderr, "standard error", NULL, result, 0);
      fprintf (stderr, "\n");
      exit (EXIT_FAILURE);
    }
  return (result);
}







void
avm_initialize_apply ()
{

#include "rewrite.c"

  /* compatible with an old undocumented character set */

char *olde_interpreter = "{jmxksShi_z{CkjmK_ZKyxno@soKx{y=NfkpoKmH{Iyl>EuoGwg\
IoH[zrfZrYKwzYIs^z{[C`_xZrKin{ZsSIKfckjslk`itkmv{jnKHhpZPxxo<exskkqFiv@kfc^zK\
KyGvrxP^zyx[CFityPqMiB<Hs`_Z>cfcswoyWOxxhyneuyPqNsn`skw]jXqDDs`ozgZ>criIZxvSJ\
hlogiwTF[PbLU=nrFVzORiVpdGhGz<LcfOZgZXogiFziY=XXjrakRmvizMOEXmt<_{iZpzhoz]DGt\
glegv`svkS@ohiGHyyIXXySOvJ^zeIyGpPtGxLkpNsMGX`]Hw\\g[Fb<D{^bkh^sqWslogqowrCIq\
aTY\\M_NgZFVzyQGkFs\\gZ_lnI?Debg<BK\\GM>z<D{f`[rBgXrkZ_[hSOSS`[_{MYLO<Hs\\>Ky\
RZmMZ?It@{sD]\\]`oN<kvlFsm>{@GyYW`[kLIaYC{FyTUN`LkqsogsXF>IxLkc{zaaP\\_ZDIuD`\
`pd<Lc\\<Oz>ivDstvflGyAUCyTGyZLy`qtng[byQy?kSSXGz>i{vxoQGOcLktLzLL\\<it\\s\\O\
zOEyY?NDsea>{gPf[ahAYMazl?IyNqJSfiCeSwNN>z=KxqOWjg\\FOtN>zgz<fk]?t\\s\\G[Jf\\\
OEyNfcpW\\F[kPuNaYOQ\\_Z]BWSPen=E{pBB>it\\svjAUMLHoAN>it@Et>IvktLz`{yFZ<itd\\\
evANg[oiyKn_gfblktQW_fMlMQoSiDBK]?y<oUj]<bL=_tivFZ=>Zgze>[RMxeKtfitpFO[n@yim[\
>XaLRuDGvyAX`QKyVkx><kvkv@X_O=N>sfiuFzGZ=>Z<e{zQQFLcleIx<egu=bqMwoSw<`g<FsoZE\
h?K[kY=OIT@Et@ExY_ZJ]gt=GtfcFitivi{xYHl<FZ>bkl<_{TNjGwxUPnhp\\BK_oMFgyoIXydWB\
RGZGZKcaoP<fZFsdFT=GvgZ?GyqR>Ffit<_zm>{X]Vg[=GS`Lb<>scle?X^yBghSqPHyGKvgZ{YQN\
]tFZ?Itg>it=GtFcqEaAj?GxLk\\Fsv^@\\oZLu[\\nFsqFd`^IWLPgqz>HR\\=?Z<NzkiVfv[fet\
>cnlHs^itGZdQyIDFc\\gZ=KzoD>IvqqI<\\O[bd\\]_fU?zOILP\\FZFs_wfpQa@qONknc\\<>c\
\\o[@s]arBB>s\\OzDIzt?>{t_gf^g[I@AYquOIW>_hitHsoNq@kqbg\\>gZNcnc\\F[E_yYYOHOZ\
DItLzg>N>z=nI=<@sl]byD@rli>ULMIt>cooH^[yQLHd@k^_uF[FZ<Gz_fvdHbK]?vkyD<g[hFLNZ\
qNqEEL^aR>o[F[QoFNzkff@<^ZFK^_uFZ@EunKD\\<Gzgz=>Z<=_xfNf[>@UjDxEL<euFZQVg^ivi\
wO@gnOzFK^_tizlw>@Et@EtitFZMbFkLQWiPNnk>_viTP\\oZ?It^@fU`EvOzFKfixfnhGZGZfs^c\
cNfGfy<mHnNe^_tONB>ItDs\\<h<iu`EvOz<NzL]>FivgZ<N{GZdE>fOZ>q><h`{^cHG{>{gEit>s\
bLc^it^<Nz<NzFs^GZf<\\xp\\<hU>[XvN>zN>z<<Lxbs\\GZ?Ix^itgZ?IwHc^ExF<<G[op\\Fs\
\\gZeT=?nWZ<et<oZ?It>c\\>k]PfeixcD\\BK\\Tz>Fc\\@KNc\\FZ<exev^Ybd\\@KGH<GZ>sh=\
>eUF[F[FZNcneoM>c]\\Bs\\euP>DdzNc`etdz>i><HH\\O{FZ`LdzFK^_tdzEHANLc^gtnOzFK^_\
vO{HL<ixfc^U>Z`{eKu`EtNhc`>s\\>sbLcbLc\\GZ\\>c\\oZbs\\GZ<ivWZFc\\gX<Lxbseit=?\
qFz=IfZ<Lx<LxetFKvn<LfRetFG@Et?LD\\\\]<Fy@MPfTp\\G>^l<<";

  if (initialized)
    return;
  initialized = 1;
  _avm_reset = 0;
  avm_initialize_lists ();
  avm_initialize_chrcodes ();
  avm_initialize_compare ();
  avm_initialize_listfuns ();
  avm_initialize_libfuns ();
  avm_initialize_decons ();
  avm_initialize_instruct ();
  avm_initialize_portals ();
  avm_initialize_ports ();
  avm_initialize_profile ();
  avm_initialize_formout ();
  avm_initialize_exmodes ();
  parallel = avm_strung ("par");
  nondeterministic = avm_strung ("npar");
  unsupported_hook = avm_join (avm_strung ("unsupported hook"), NULL);
  invalid_deconstruction = avm_join (avm_strung ("invalid deconstruction"), NULL);
  invalid_recursion = avm_join (avm_strung ("invalid recursion"), NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  reset = avm_join (avm_strung ("reset"), NULL);
  avram_version_number = avm_join (avm_strung (avm_version ()), NULL);
  interpreter = avm_scanned_list (avm_prior_to_version ("0.1.0") ? olde_interpreter : interpreter_code);
}





void
avm_count_apply ()
{

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (parallel);
  avm_dispose (nondeterministic);
  avm_dispose (unsupported_hook);
  avm_dispose (avram_version_number);
  avm_dispose (interpreter);
  avm_dispose (memory_overflow);
  avm_dispose (reset);
  avm_dispose (invalid_deconstruction);
  avm_dispose (invalid_recursion);
  parallel = NULL;
  nondeterministic = NULL;
  unsupported_hook = NULL;
  avram_version_number = NULL;
  interpreter = NULL;
  memory_overflow = NULL;
  reset = NULL;
  invalid_recursion = NULL;
  invalid_deconstruction = NULL;
}

