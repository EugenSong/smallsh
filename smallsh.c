
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>

/* Convenient macro to get the length of an array (number of elements) */
#define arrlen(a) (sizeof(a) / sizeof *(a))

// function to split words and store into an array of string ptrs 
static int split_word(int elements, char *input, char *ptrArray[]);

// function to string search and replace
static char *str_substitute(char *restrict *restrict haystack, char const *restrict needle, char const *restrict substitute); 

// function to word expansion
static void perform_expansion(int elements, char *ptrArray[]);


int main(int argc, char *argv[]) {

  char *env_name = NULL;
  int elements = 0;

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


  char* line2 = NULL;
  size_t n = 0;
  getline(&line2, &n, stdin);
  {
    char *ret = str_substitute(&line2, argv[1], argv[2]); 
    if (!ret) exit(1); 
    line2 = ret;
  }
  printf("%s", line2); 

  free(line2); 






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
  split_word(elements, line, ptrArray);



  // parsing needs to happen before expansion




  // perform expansion 






  free(line);



  return 0; 
}

static void perform_expansion(int elements, char *ptrArray[]) {

  char temp[50];
  char *expan_env = NULL;
  expan_env = getenv("HOME");
  printf("\nexpan_env is %s\n", expan_env); 

  int i = 0;
  printf("size of ptrArray is %d\n", elements);

  while (i < elements) {

    // ensure each token is >= size 2
    if (strlen(ptrArray[i]) >= 2) {

      // bullet point 1: “~/” at the beginning of a word shall be replaced with the value of the HOME environment variable
      if (strncmp(ptrArray[i], "~/", 2) == 0) {
        str_substitute(&ptrArray[i], "~", expan_env);
      }

      // pre- bullet point 2: get the smallsh pid and transform into a str to use in str_substitute() 
      pid_t pid = getpid();
      if (sprintf(temp, "%d", (int)pid) < 0) {
        perror("sprintf failed... $$ replacement failed"); 
      }

      // bullet point 2: occurrence of “$$” within a word shall be replaced with the proccess ID of the smallsh process
      str_substitute(&ptrArray[i], "$$", temp); 

      // bullet point 3: occurrence of “$?” within a word shall be replaced with the exit status of the last foreground command (see waiting)


      // bullet point 4: occurrence of “$!” within a word shall be replaced with the process ID of the most recent background process (see waiting)


    }

    i++; 
  }

}


static int split_word(int elements, char *input, char *ptrArray[]) {

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
    elements++; 
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




static char *str_substitute(char *restrict *restrict haystack, char const *restrict needle, char const *restrict substitute) {

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

    // important to move str ptr so loop knows to quit
    str += sub_len; 
  }
  str = *haystack;

  // CASE 2: Replacement string is shorter than needle size --> shrink haystack size after memmove / memcpy 
  if (sub_len < needle_len) {
    str = realloc(*haystack, sizeof **haystack * (haystack_len+1));
    if (str == NULL) {
      goto exit; 
    }
    *haystack = str; 
  }

exit: 
  return str; 
}


// check for unwaited-for-background processes
void manage_background_processes() {

  // get process group id of calling process..always sucess 
  pid_t currGroupId = getpgrp();
  pid_t childPID = 0;
  int childStatus = 0;

  // WNOHANG flag - waitpid() returns 0 when no child process terminates...
  // WUNTRACED constant - for stopped processes
  //      -1 --> fork() failed
  //      0 --> child process executes chunk 
  //      > 0 --> child process PID 

  while ((childPID = waitpid(-1, &childStatus, WNOHANG | WUNTRACED)) > 0) {

    pid_t childPGID = getpgid(childPID);

    if (childPGID == -1) {
      perror("There was an error calling getpgid()"); 
    }
    // check if child process group id of current child process == parent process group id
    if (getpgid(childPID) == currGroupId) {

      // if exited normally 
      if (WIFEXITED(childStatus)) {
        int exitStatus = WEXITSTATUS(childStatus);
        fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) childPID, exitStatus);
      }

      // if signaled
      else if (WIFSIGNALED(childStatus)) {
        int signalNumber = WTERMSIG(childStatus);
        fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) childPID, signalNumber);
      }

      // if stopped 
      else if (WIFSTOPPED(childStatus)) {
        int killStatus = 0;

        // if SIGCONT kill() fails
        if ((killStatus = kill(childPID, SIGCONT)) == -1) {
          perror("kill() failed with the childPID"); 
        }
        fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) childPID); 
      }

      // other state changes: ignore 
    }
  }


}
