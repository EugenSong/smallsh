
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>



/* Convenient macro to get the length of an array (number of elements) */
#define arrlen(a) (sizeof(a) / sizeof *(a))

// split words and store into an array of string ptrs
static int split_word(char *input, char *ptrArray[]);

// string search and replace function
char *str_substitute(char *restrict *restrict haystack, char const *restrict needle, char const *restrict substitute); 




int main(void) {

  char *env_name = NULL;

  FILE *fp = stdin;
  char *line = NULL;   // keep outside main infinite loop 
  size_t buff_size = 0;
  ssize_t bytes_read;  // later: maybe move into main infinite loop like example

  char *ptrArray[512]; 

  /* ************ INPUT *********************
   *  
   *  ---------------------------------------------------------
   *  Input - Managing background processes (NEED TO INCLUDE)
   *  ---------------------------------------------------------

   *  check for un-waited-for-background processes in the same process group ID as smallsh
   *  -----------------------------------------------------
   */

  char *expan_env = NULL;
  expan_env = getenv("HOME");
  printf("\nexpan_env is %s\n", expan_env); 








  // ------ Testing portion (above ^^^^ ) -------- Top of code starts below (vvvvv) --------

  /*  --------------------------------
   *  Input - The prompt (DONE)
   *  --------------------------------
   */

  // expand PS1 environment variable | fprintf() prints to desig stream
  env_name = getenv("PS1");
  fprintf(stderr, "%s",env_name);

  /*
   * ------------------------------------------------
   * Input - Reading a line of input (Almost done)
   * ...include more on reading interrupted by signal...clearerr(3)
   * ------------------------------------------------
   */

  // read a line of input from stdin [getline(3)]
  bytes_read = getline(&line, &buff_size, fp);

  if (bytes_read == -1) {
    perror("Getline() failed."); 
    /* check for signal interruptions (signal handling) --> print newline and
     * spawn new command prompt (check for background processes) --> continue input line read
     */
  }
  else {
    printf("\nRead number of bytes from getline(): %zd\n", bytes_read);
  }

  // split words
  split_word(line, ptrArray);

  free(line);



  return 0; 
}


static int split_word(char *input, char *ptrArray[]) {

  char *ifs_env = getenv("IFS");
  char *token = NULL;
  char *copy = NULL;
  int index = 0;


  // if unset --> "space - tab - new line"
  if (ifs_env == NULL) {
    ifs_env = " \t\n"; 
  }


  token = strtok(input, ifs_env); 
  printf("First token =%s\n", token);

  copy = strdup(token);
  //printf("Copied token =%s\n", copy); 

  // allocate memory in str pointer based on len of token
  ptrArray[index] = (char*)malloc((strlen(token) + 1) * sizeof (*ptrArray));

  if (ptrArray[index] == NULL) {
    perror("mallloc failed, exiting...");
    exit(1); 
  }

  strcpy(ptrArray[index], copy);
  free(copy);

  for (;;) {

    index++;
    copy = NULL;

    token = strtok(NULL, ifs_env);
    printf("Next Token =%s\n", token); 

    if (token == NULL) { break;}

    copy = strdup(token);
    ptrArray[index] = (char*)malloc((strlen(token) + 1) * sizeof (*ptrArray));

    if (ptrArray[index] == NULL) {
      perror("mallloc failed, exiting...");
      exit(1); 
    }

    strcpy(ptrArray[index], copy);
    free(copy);
  }

  printf("The strings are:\n");

  // print the ptrArray
  for (int j = 0; j < index; j++) {
    printf("%s\n", ptrArray[j]); 
  }

  printf("Done splitting word and printing tokens.\n"); 
  return 0; 

  // don't forget to free(ptrArray[index] after finishing !!
}




char *str_substitute(char *restrict *restrict haystack, char const *restrict needle, char const *restrict substitute) {
  
  char *str = *haystack;
  size_t haystack_len = strlen(str); 
  size_t const needle_len = strlen(needle), 
               sub_len = strlen(substitute); 

  for (;;) {

    // search the pointer to the array of char pointers for existing needle... exit if DNE
    str = strstr(str, needle);
    if (!str) break;
    ptrdiff_t offset = str - *haystack;

    // CASE 1: Replacement string is longer than needle size 
    // realloc - update size of haystack if replacement is longer or shorter than needle, need to resize the memory location 
    if (sub_len > needle_len) {
      str = realloc(*haystack, sizeof **haystack * (haystack_len + sub_len - needle_len + 1));
          if (str == NULL) {
            goto exit;  
          }

          // move haystack start pointer to str
          *haystack = str;
          str = *haystack + offset; 
  }
    memmove(str + sub_len, str + needle_len, haystack_len + 1 - offset - needle_len);  
    memcpy(str, substitute, sub_len);
    haystack_len = haystack_len + sub_len - needle_len; 
    str += sub_len; 
  }
  str = *haystack;

  // CASE 2: Replacement string is shorter than needle size 
  // shrink haystack size
  if (sub_len < needle_len) {
    str = realloc(*haystack, sizeof ** haystack * (haystack_len+1));
    if (str == NULL) {
      goto exit; 
    }
    *haystack = str; 
  }
  
exit: 
  return str; 
}

