
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>




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
  }
  else {
    printf("\nRead number of bytes from getline(): %zd", bytes_read);
  }

  free(line);



  return 0; 
}
