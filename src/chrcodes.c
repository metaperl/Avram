/* functions for converting between characters and lists

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
#include <avm/lists.h>
#include <avm/branches.h>
#include <avm/chrcodes.h>


/* a list representation for each character code */
list _avm_representations[256];
list _avm_standard_representations[256];

/* spare storage used in the avm_character_representation macro */
list _avm_temporary_character;

/* used as a counter of bits that have been packed into a character */
static int spoke = 0;

/* temporary storage for a byte being unpacked */
static int spool = 0;

/* representation of (nil,nil) */
static list shared_cell = NULL;

/* error messages */
static list memory_overflow = NULL;
static list counter_overflow = NULL;
static list invalid_text_format = NULL;
static list invalid_prompt = NULL;

/* Non-zero means static variables have been initialized. */
static int initialized = 0;

/* Non-zero indicates an invalid character representation detected by pack */
static int packaging_error;

/* used for quickly mapping lists packed into integers to their ascii
   codes */
#define maximum_package 5459
static int ascii_codes[1 + maximum_package];
static int standard_ascii_codes[1 + maximum_package];






static void
pack (operand, package, level)
     list operand;
     int *package;
     int level;

     /* This turns a list into a bit string, assuming the bit string
        is short enough to be packed into an integer.  The bits are
        written according to a postfix traversal, which is different
        from the way lists are encoded for file i/o. */
{

  if (level > 8)
    packaging_error = 1;
  else if (!operand)
    *package <<= 1;
  else
    {
      pack (operand->tail, package, level + 1);
      pack (operand->head, package, level + 1);
      *package = (*package << 1) + 1;
    }
}







int
avm_character_code (operand)
     list operand;

     /* This returns the character code if the operand represents a
        character, -1 otherwise; The way character codes are computed
        from lists is meant to be as fast as possible. First the list
        is turned into a bit string by a postfix traversal, and the
        bits are packed into an integer.  The integer is then used as
        an index into a pre-initialized array to obtain the character
        code. Once obtained, the character code is written into the
        list node, so that if the same list needs to be translated to
        a character code again, the previously stored value will be
        retrieved instead of being computed again. */
{
  int package;

  if (!operand)
    return (-1);
  if (operand->characteristic)
    return (operand->characterization);
  packaging_error = package = 0;
  pack (operand, &package, 0);
  if (packaging_error ? 1 : (package < 0 ? 1 : package > maximum_package))
    return (-1);
  else if ((operand->characteristic = (ascii_codes[package] >= 0)))
    return (operand->characterization = ascii_codes[package]);
  else
    return (-1);
}



int
avm_standard_character_code (operand)
     list operand;

     /* always uses the standard representation */
{
  int package;

  if (!operand)
    return (-1);
  packaging_error = package = 0;
  pack (operand, &package, 0);
  if (packaging_error ? 1 : (package < 0 ? 1 : package > maximum_package))
    return (-1);
  else
    return(standard_ascii_codes[package]);
}






list
avm_strung (string)
     char *string;

     /* This turns a null terminated character string into a list. */

{
  list front, back;

  if (!initialized)
    avm_initialize_chrcodes ();
  front = back = NULL;
  while (*string)
    avm_enqueue (&front, &back, avm_character_representation (*(string++)));
  return (front);
}






list
avm_standard_strung (string)
     char *string;

     /* This turns a null terminated character string into a list,
	using the standard character representation regardless of the version
	emulation. */
{
  list front, back;

  if (!initialized)
    avm_initialize_chrcodes ();
  front = back = NULL;
  while (*string)
    {
      /* printf("%s\n",string); */
      avm_enqueue (&front, &back, avm_standard_character_representation (*(string++)));
    }
  return (front);
}









list
avm_recoverable_strung (string, fault)
     char *string;
     int *fault;

     /* This turns a null terminated character string into a list. */

{
  list front, back;

  if (!initialized)
    avm_initialize_chrcodes ();
  front = back = NULL;
  while (*fault ? 0 : *string)
    {
      avm_recoverable_enqueue (&front,&back,avm_character_representation (*(string++)),fault);
      if (! front)
	string = NULL;
    }
  return (front);
}





list
avm_recoverable_standard_strung (string, fault)
     char *string;
     int *fault;

     /* always uses the standard representation */

{
  list front, back;

  if (!initialized)
    avm_initialize_chrcodes ();
  front = back = NULL;
  while (*string)
    {
      avm_recoverable_enqueue (&front,&back,avm_standard_character_representation (*(string++)),fault);
      if (! front)
	string = NULL;
    }
  return (front);
}










static int
scanned_bit (string)
     char **string;

     /* This returns the next bit in a string each time it's called;
	the spool represents the current character, which isn't
	advanced to the next position in the string until all its bits
	are read, and the spoke keeps track of the bits remaining in
	the spool. */
{

  if (!(spoke--))
    {
      if (!**string)
	avm_internal_error (13);
      spool = *((*string)++) - 60;
      spoke = 5;
    }
  return ((spool >> spoke) & 1);
}







list
avm_scanned_list (string)
     char *string;

     /* This function uses the same algorithm as the one for reading
        trees from files, but reads from a string in memory rather
        than from a file. It's meant to be used for initializing lists
        that are hard coded into the program rather than being read
        from a file, such as the interpreter code in rewrite.c, since
        there's no easy way to initialize them otherwise.

        In an earlier version of the interpreter defined in rewrite.c,
        there was a weird case where cyclic lists were created as a
        result of the interpreter interpreting itself. It worked ok
        but couldn't be reclaimed. The interpreter code has been
        changed since then so that cyclic structures can't be created,
        but just in case anybody changes it back, this function makes
        a point of marking the lists it builds as internal, and the
        apply function uses this information to avoid creating a
        cyclic list.
     */
{
  list result;
  branch_queue old, front, back;

  if (!initialized)
    avm_initialize_chrcodes ();
  spoke = 0;
  front = back = NULL;
  avm_anticipate (&front, &back, &result);
  while (front)
    {
      if (*(front->above) = (scanned_bit (&string) ? avm_join (NULL, NULL) : NULL))
	{
	  (*(front->above))->internal = 1;	/* needed to prevent cyclic interpretations */
	  avm_anticipate (&front, &back, &((*(front->above))->head));
	  avm_anticipate (&front, &back, &((*(front->above))->tail));
	}
      front = (old = front)->following;
      avm_dispose_branch (old);
    }
  return (result);
}





list
avm_multiscanned (strings)
     char **strings;

     /* This does the same thing as scanned list except that it
        operates on null terminated array of strings instead of on a
        single string. Its purpose is for building hard coded lists at
        run time. Some compilers don't allow really large string
        constants so this gets around that limitation by letting them
        be arrays of strings. */
{
  list result;
  branch_queue old, front, back;
  char *string;
  int string_number;

  if (!initialized)
    avm_initialize_chrcodes ();
  spoke = 0;
  string = strings[string_number = 0];
  front = back = NULL;
  avm_anticipate (&front, &back, &result);
  while (front)
    {
      if (*string ? NULL : strings[++string_number])
	string = strings[string_number];
      if (*(front->above) = (scanned_bit (&string) ? avm_join (NULL, NULL) : NULL))
	{
	  avm_anticipate (&front, &back, &((*(front->above))->head));
	  avm_anticipate (&front, &back, &((*(front->above))->tail));
	}
      front = (old = front)->following;
      avm_dispose_branch (old);
    }
  return (result);
}







char *
avm_unstrung (string, message, fault)

     /* inverse of the strung function */

     list string;
     list *message;
     int *fault;
{
  char *result;
  counter total_length;
  int temporary;
  char *next_character_position;

  if (*fault)
    return NULL;
  *message = NULL;
  result = NULL;
  total_length = avm_recoverable_length (string);
  if (string ? (*fault = !total_length) : 0)
    *message = avm_copied (counter_overflow);
  else if (*fault = !(++total_length))
    *message = avm_copied (counter_overflow);
  else if (*fault = !(result = (char *) malloc (total_length)))
    *message = avm_copied (memory_overflow);
  if (*fault)
    return NULL;
  next_character_position = result;
  while (string ? !*fault : 0)
    {
      if (!total_length--)
	avm_internal_error (105);
      temporary = avm_character_code (string->head);
      if (*fault = (temporary <= 0))
	*message = avm_copied (invalid_text_format);
      else
	*next_character_position++ = temporary;
      string = string->tail;
    }
  if (*fault)
    {
      free (result);
      return NULL;
    }
  if (!total_length)
    avm_internal_error (44);
  (*next_character_position) = '\0';
  return result;
}





char *
avm_standard_unstrung (string, message, fault)
     list string;
     list *message;
     int *fault;

     /* always uses the standard representation */
{
  char *result;
  counter total_length;
  int temporary;
  char *next_character_position;

  if (*fault)
    return NULL;
  *message = NULL;
  result = NULL;
  total_length = avm_recoverable_length (string);
  if (string ? (*fault = !total_length) : 0)
    *message = avm_copied (counter_overflow);
  else if (*fault = !(++total_length))
    *message = avm_copied (counter_overflow);
  else if (*fault = !(result = (char *) malloc (total_length)))
    *message = avm_copied (memory_overflow);
  if (*fault)
    return NULL;
  next_character_position = result;
  while (string ? !*fault : 0)
    {
      if (!total_length--)
	avm_internal_error (71);
      temporary = avm_standard_character_code (string->head);
      if (*fault = (temporary <= 0))
	*message = avm_copied (invalid_text_format);
      else
	*next_character_position++ = temporary;
      string = string->tail;
    }
  if (*fault)
    {
      free (result);
      return NULL;
    }
  if (!total_length)
    avm_internal_error (40);
  (*next_character_position) = '\0';
  return result;
}



char *
avm_prompt (prompt_strings)

     /* This takes a list of character strings represented as lists
        and returns a string of characters with 13 10 used as a
        separator. */

     list prompt_strings;
{
  list line;
  counter total_length;
  char *result;
  char *next_character_position;
  int temporary;

  total_length = avm_length (prompt_strings);
  if (!total_length)
    return (NULL);
  total_length = (total_length - 1) << 1;
  if (total_length < 0)
    avm_error ("counter overflow (code 5)");
  total_length = total_length + avm_area (prompt_strings);
  if (total_length < 0)
    avm_error ("counter overflow (code 6)");
  total_length++;
  if (!total_length)
    avm_error ("counter overflow (code 7)");
  if (!(result = (char *) malloc (total_length)))
    avm_error ("memory overflow (code 4)");
  next_character_position = result;
  while (prompt_strings)
    {
      line = prompt_strings->head;
      while (line)
	{
	  if (!total_length--)
	    avm_internal_error (14);
          temporary = avm_character_code (line->head);
          *next_character_position = temporary;
	  if (temporary < 0)
	    avm_error ("invalid text format (code 2)");
	  else if (!(*(next_character_position++)))
	    avm_error ("null character in prompt");
	  line = line->tail;
	}
      if (prompt_strings->tail)
	{
	  if (total_length < 2)
	    avm_internal_error (15);
	  (*(next_character_position++)) = 13;
	  (*(next_character_position++)) = 10;
	  total_length = total_length - 2;
	}
      else
	{
	  if (!total_length--)
	    avm_internal_error (16);
	  (*next_character_position) = '\0';
	}
      prompt_strings = prompt_strings->tail;
    }
  return (result);
}





char *
avm_recoverable_prompt (prompt_strings,message,fault)

     list prompt_strings;
     list *message;
     int *fault;
{
  list line;
  counter total_length;
  char *result;
  char *next_character_position;
  int temporary;

  *message = NULL;
  if (*fault)
    return NULL;
  total_length = avm_length (prompt_strings);
  if (!total_length)
    return NULL;
  total_length = (total_length - 1) << 1;
  if (*fault = (total_length < 0))
    {
      *message = avm_copied (counter_overflow);
      return NULL;
    }
  total_length = total_length + avm_area (prompt_strings);
  if (*fault = (total_length < 0))
    {
      *message = avm_copied (counter_overflow);
      return NULL;
    }
  total_length++;
  if (*fault = (!total_length))
    {
      *message = avm_copied (counter_overflow);
      return (NULL);
    }
  if (*fault = (!(result = (char *) malloc (total_length))))
    {
      *message = avm_copied (memory_overflow);
      return (NULL);
    }
  next_character_position = result;
  while (*fault ? 0 : !!prompt_strings)
    {
      line = prompt_strings->head;
      while (*fault ? 0 : !!line)
	{
	  if (!total_length--)
	    avm_internal_error (45);
          temporary = avm_standard_character_code (line->head);
          *next_character_position = temporary;
	  if (*fault = (temporary < 0))
	    {
	      *message = avm_copied (invalid_prompt);
	      free (result);
	      return (NULL);
	    }
	  else if (*fault = !(*(next_character_position++)))
	    {
	      *message = avm_copied (invalid_prompt);
	      free (result);
	      return (NULL);
	    }
	  line = line->tail;
	}
      if (prompt_strings->tail)
	{
	  if (total_length < 2)
	    avm_internal_error (46);
	  (*(next_character_position++)) = 13;
	  (*(next_character_position++)) = 10;
	  total_length = total_length - 2;
	}
      else
	{
	  if (!total_length--)
	    avm_internal_error (47);
	  (*next_character_position) = '\0';
	}
      prompt_strings = prompt_strings->tail;
    }
  return result;
}







static list
unpacker (package)
     unsigned int *package;

     /* This is used for building the array of lists used as a lookup
	table by the avm_character_representation macro. */

{
  list left, right;

  if (!(*package & 1))
    {
      *package >>= 1;
      return (NULL);
    }
  *package >>= 1;
  left = unpacker (package);
  return ((!!(right = unpacker (package)) ? 1 : !!left) ? avm_join (left, right) : avm_copied (shared_cell));
}






void
avm_initialize_chrcodes ()

     /* This initializes local data structures and is a good example
	of backward compatibility management. */
{
  unsigned int package, ascii_code;

  char *old_character_set = "\
04d50135053504b504750275004d014d054d04cd02cd01cd012d052d032d04ad\
046d026d011d051d031d009d049d029d019d045d025d043d023d013d00130053\
0153055304d302d30ad306d301d311d309d305d303d301330533153313330b33\
073300b304b314b30cb302b312b30ab306b301b311b309b305b303b300730473\
14730c73027312730a7306730173117309730573037300f310f308f304f302f3\
01f3004b014b054b154b0d4b034b134b0b4b074b00cb04cb14cb0ccb02cb12cb\
0acb06cb01cb11cb09cb05cb03cb012b052b152b0d2b032b132b0b2b072b04ab\
14ab0cab12ab11ab046b146b0c6b126b066b116b10eb011b051b151b0d1b031b\
131b0b1b071b049b149b0c9b129b069b019b119b045b145b0c5b125b115b10db\
043b143b0c3b123b113b10bb107b00470147054715470d47034713470b470747\
00c704c714c70cc702c712c70ac706c701c711c709c705c703c7012705271527\
0d27032713270b27072704a714a70ca712a706a701a711a70067046714670c67\
12670667116710e70117051715170d17031713170b170717049714970c971297\
1197045714570c571257115710d7043714370c371237113710b71077010f050f\
150f0d0f030f130f0b0f070f048f148f0c8f128f044f144f0c4f124f114f10cf\
042f142f0c2f122f112f10af106f041f141f0c1f121f111f031f109f105f103f";

  char *character_set = "\
04d50135053504b50475027504f5004d014d054d154d134d0b4d074d04cd14cd\
02cd12cd0acd11cd09cd012d052d152d0d2d032d132d0b2d072d04ad14ad0cad\
12ad11ad09ad046d146d026d126d0a6d116d096d10ed08ed04ed011d051d151d\
131d0b1d009d049d149d0c9d029d129d0a9d119d099d059d045d145d025d125d\
0a5d115d095d10dd08dd043d143d023d123d0a3d113d093d10bd08bd107d087d\
001300530153055315530b53075304d302d312d30ad306d301d311d309d305d3\
03d301330533153313330b3300b304b314b30cb302b312b30ab311b309b305b3\
04731473027312730a731173097310f308f301f3004b054b154b034b134b0b4b\
04cb14cb02cb12cb0acb11cb09cb012b052b152b0d2b132b0b2b04ab14ab0cab\
12ab11ab046b126b116b10eb011b051b151b0d1b131b0b1b049b149b129b119b\
045b145b0c5b125b115b10db043b123b113b10bb107b00470147054715470d47\
034713470b47074704c714c702c712c70ac711c709c70127052715270d270b27\
04a714a712a711a7046714671267116710e70117051715170d1713170b170497\
149712971197045714570c571257115710d7043714370c371237113710b71077\
010f050f150f0d0f030f130f0b0f070f048f148f128f118f044f144f124f114f\
10cf042f142f0c2f122f112f10af106f041f141f0c1f121f111f109f105f103f";

  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_branches ();
  shared_cell = avm_join (NULL, NULL);
  for (package = 0; package <= maximum_package; package++)
    ascii_codes[package] = -1;
  if (avm_prior_to_version ("0.1.0"))
    {
      for (ascii_code = 0; ascii_code <= 255; ascii_code++)
	{
	  sscanf (&(old_character_set[ascii_code << 2]), "%4x", &package);
	  ascii_codes[package] = ascii_code;
	  _avm_representations[ascii_code] = unpacker (&package);
	  _avm_representations[ascii_code]->characteristic = 1;
	  _avm_representations[ascii_code]->characterization = ascii_code;
	}
    }
  else
    {
      for (ascii_code = 0; ascii_code <= 255; ascii_code++)
	{
	  sscanf (&(character_set[ascii_code << 2]), "%4x", &package);
	  ascii_codes[package] = ascii_code;
	  _avm_representations[ascii_code] = unpacker (&package);
	  _avm_representations[ascii_code]->characteristic = 1;
	  _avm_representations[ascii_code]->characterization = ascii_code;
	}
    }
  for (ascii_code = 0; ascii_code <= 255; ascii_code++)
    {
      sscanf (&(character_set[ascii_code << 2]), "%4x", &package);
      standard_ascii_codes[package] = ascii_code;
      _avm_standard_representations[ascii_code] = unpacker (&package);
      _avm_standard_representations[ascii_code]->characteristic = 1;
      _avm_standard_representations[ascii_code]->characterization = ascii_code;
    }
  invalid_prompt = avm_join (avm_strung ("invalid prompt"), NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  counter_overflow = avm_join (avm_strung ("counter overflow"), NULL);
  invalid_text_format = avm_join (avm_strung ("invalid text format"), NULL);
}






void
avm_count_chrcodes ()

     /* This uninitializes local storage so that memory leaks can be
	detected. */

{

  int ascii_code;

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (shared_cell);
  avm_dispose (memory_overflow);
  avm_dispose (invalid_prompt);
  avm_dispose (counter_overflow);
  avm_dispose (invalid_text_format);
  counter_overflow = NULL;
  memory_overflow = NULL;
  invalid_text_format = NULL;
  invalid_prompt = NULL;
  shared_cell = NULL;
  for (ascii_code = 0; ascii_code < 256; ascii_code++)
    {
      avm_dispose (_avm_representations[ascii_code]);
      avm_dispose (_avm_standard_representations[ascii_code]);
    }
}
