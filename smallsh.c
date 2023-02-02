
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 


/* Convenient macro to get the length of an array (number of elements) */
#define arrlen(a) (sizeof(a) / sizeof *(a))


static int split_word(char *input, const char *delim, char *tokenArray[]);






//static int splitword(char *input, const char *delim, char *tokenArray[]) {

static int splitword(char *input, const char *delim) {
  
  char *ifs_env = getenv("IFS");
  char *token = NULL;

  if (ifs_env == NULL) {
    ifs_env = " \t\n"; 
  }

  //while (strtok(char *input, ifs_env, tokenArray) !== NULL) {
 
  token = strtok(input, ifs_env); 
  printf("First token = %s\n", token); 
  for (;;) {
    token = strtok(NULL, ifs_env);
    printf("Next Token = %s\n", token); 

    if (token == NULL) { break;} 

  }

  return 0; 
}








int main(void) {

  char *env_name = NULL;
  
  FILE *fp = stdin;
  char *line = NULL;
  size_t buff_size = 0;
  ssize_t bytes_read; 

  /* ************ INPUT *********************

   *  Managing background processes 
   *  --------------------------------
   */  
   
  // check for un-waited-for-background processes in the same process group ID as smallsh
  
  /*  The prompt 
   *  --------------------------------
  */ 

  // expand PS1 environment variable | fprintf() prints to desig stream
  env_name = getenv("PS1");
  fprintf(stderr, "The environment name is: %s",env_name);

  // read a line of input from stdin [getline(3)]
  bytes_read = getline(&line, &buff_size, fp);
  
  if (bytes_read == -1) {
     perror("Getline() failed."); 
     /* check for signal interruptions (signal handling) --> print newline and
      * spawn new command prompt (check for background processes) --> continue input line read
      */
  }
  else {
    printf("\nRead number of bytes from getline(): %zd", bytes_read);
  }

  free(line);



  return 0; 
}



void *str_substitute(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub) {
  
  char *str = *haystack;
  size_t haystack_len = strlen(str); 
  size_t const needle_len = strlen(needle), 
               sub_len = strlen(sub); 

  for (;;) {
    str = strstr(str, needle);
    if (!str) break;

    // realloc 
  }
}
