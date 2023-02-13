
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
#include <ctype.h>

/* Convenient macro to get the length of an array (number of elements) */
#define arrlen(a) (sizeof(a) / sizeof *(a))

// function to split words and store into an array of string ptrs 
static int split_word(int elements, char *input, char *ptrArray[]);

// function to string search and replace
static char *str_substitute(char *restrict *restrict haystack, char const *restrict needle, char const *restrict substitute); 

// function to word expansion
static void perform_expansion(pid_t backgroundProcessId, int exitStatusForeground, int elements, char *ptrArray[]);

// function to check for unwaited-for-background processes before taking user input 
static void manage_background_processes();

// SIGINT handler function
static void handle_SIGINT(int signo);

// function to print prompt
static void print_prompt(char *ps1_env);

// function to parse & tokenize user input 
static void parse_user_input(char *outFile, char *inFile, int *runInBackground, int elements, char *ptrArray[]);

// function to print the char * []
static void print_array(int elements, char* ptrArray[]);


int main(int argc, char *argv[]) {

  char *outFile = NULL;
  char *inFile = NULL; 

  int runInBackground = 0;
  int exitStatusForeground = 0;
  pid_t backgroundProcessId = -100;

  char *ps1_env = NULL;
  int elements = 0;

  FILE *fp = stdin;
  char *line = NULL;   // keep outside main infinite loop 
  size_t buff_size = 0;
  ssize_t bytes_read;  // later: maybe move into main infinite loop like example

  char *ptrArray[512];

  /* -------------------------------------------- */ 
  /*  SIGINT & SIGTSTP signal set-up (done) */
  /* --------------------------------------------- */
  struct sigaction SIGINT_action = {0}, ignore_action = {0};
  // Register handle_SIGINT as the signal handler
  SIGINT_action.sa_handler = handle_SIGINT;
  // Block all catchable signals while handle_SIGINT is running
  sigfillset(&SIGINT_action.sa_mask);
  // No flags set
  SIGINT_action.sa_flags = 0;

  // give SIGINT a signal handler at the start when user is prompted 
  sigaction(SIGINT, &SIGINT_action, NULL);

  // Set SIGTSTP's action part of ignore_action struct -- SIG_IGN as its signal handler
  ignore_action.sa_handler = SIG_IGN;
  sigaction(SIGTSTP, &ignore_action, NULL);


  /* ************ INPUT ********************* */
  /*  ---------------------------------------------------------
   *  Input - Managing background processes (Done) 
   *  check for un-waited-for-background processes in the same process group ID as smallsh
   *  ---------------------------------------------------------
   */
  manage_background_processes(); 

  // ******************************************




  // ------ Testing portion (above ^^^^ ) -------- Top of code starts below (vvvvv) --------

  /*  ----  Input - The prompt (DONE) ------ */
  print_prompt(ps1_env);

  /* ------ Input - Reading a line of input (Done) ------------------ */

  // read input from stdin
  bytes_read = getline(&line, &buff_size, fp);

  // signal interrupt or getline() fail...reset errno and  
  if (bytes_read == -1) {
    perror("Getline() failed.\n");
    clearerr(stdin);
    errno = 0;
    putchar('\n'); 
    // continue; /* reset command prompt */ 

  }
  else {
    printf("\nRead number of bytes from getline(): %zd\n", bytes_read);
    printf("Moving onto word_splitting...\n"); 
  }

  // reset SIGINT to be ignored throughout program EXCEPT @ getline() 
  sigaction(SIGINT, &ignore_action, NULL); 



  // split words
  elements = split_word(elements, line, ptrArray);


  printf("Size of elements after split_word and before perform_expansion: %d\n", elements); 


  // perform expansion 
  perform_expansion(backgroundProcessId, exitStatusForeground, elements, ptrArray);

  // print the ptrArray
  print_array(elements, ptrArray);  
  // parse tokenized input 

  printf("\nParsing user input:\n"); 
  parse_user_input(outFile, inFile, &runInBackground, elements, ptrArray);


exit: 

  free(line);
  return 0; 
}

static void print_array(int elements, char *ptrArray[]) {
  // print the ptrArray
  for (int j = 0; j < elements; j++) {
    if (ptrArray[j] == NULL) {
      printf("index %d is NULL\n", j); 
    }
    else {
      printf("%s\n", ptrArray[j]); 
    }
  }
}


static void parse_user_input(char *outFile, char *inFile, int *runInBackground, int elements, char *ptrArray[]) {

  for (int counter = 0; counter < elements; counter++) {

    // iterate through wordlist til end of line or till # is encountered
    if (counter != elements-1 && strcmp(ptrArray[counter], "#") != 0) {
      continue; 
    }

    printf("parse_user_input has gone past the continue"); 

    //  # (comment token) found ... stop looking
    if (strcmp(ptrArray[counter], "#") == 0) {
      free(ptrArray[counter]); 
      ptrArray[counter] = NULL; 

      // "&" exists behind "#" (comment)  
      if (counter-1 >= 0) {
        if (strcmp(ptrArray[counter-1], "&") == 0) {
          free(ptrArray[counter-1]);
          ptrArray[counter-1] = NULL; 
          *runInBackground = 2;
        }
        // check for "<" before & - input redirection operator
        if (counter-3 >= 0) {

          if (strcmp(ptrArray[counter-3], "<") == 0) {
            inFile = ptrArray[counter-2];
            free(ptrArray[counter-3]);
            ptrArray[counter-3] = NULL;

            // if "> output_file" exists before the "<"
            if (counter-5 >= 0) {
              if (strcmp(ptrArray[counter-5], ">") == 0) {
                outFile = ptrArray[counter-4];
                free(ptrArray[counter-5]); 
                ptrArray[counter-5] = NULL;
              }
            }
            break;
          }

          // check for ">" before & - output redirection operator
          else if (strcmp(ptrArray[counter-3], ">") == 0) {
            outFile = ptrArray[counter-2];
            free(ptrArray[counter-3]);
            ptrArray[counter-3] = NULL; 

            if (counter-5 >= 0) {
              if (strcmp(ptrArray[counter-5], "<") == 0) {
                inFile = ptrArray[counter-4];
                free(ptrArray[counter-5]); 
                ptrArray[counter-5] = NULL;
              }
            }
            break; 
          }
        }
        break; 
      }
      break;
    }


    // wordlist pointer reached the end w/o encountering a comment (#)
    else if (counter == elements-1) {

      printf("counter == elements-1 reached\n"); 

      // "&" exists behind NULL   
      if (counter-1 >= 0) {
        if (strcmp(ptrArray[counter-1], "&") == 0) {
          free(ptrArray[counter-1]);
          ptrArray[counter-1] = NULL; 
          *runInBackground = 2;
        }
      }

      // check for "<" before & - input redirection operator
      if (counter-2 >= 0) {
        if (strcmp(ptrArray[counter-2], "<") == 0) {
          inFile = ptrArray[counter-1];
          free(ptrArray[counter-2]);
          ptrArray[counter-2] = NULL;

          // if "> output_file" exists before the "<"
          if (counter-4 >= 0) {
            if (strcmp(ptrArray[counter-4], ">") == 0) {
              outFile = ptrArray[counter-3];
              free(ptrArray[counter-4]); 
              ptrArray[counter-4] = NULL;
            }

            break;
          }
          break;
        }

        // check for ">" before & - output redirection operator
        else if (strcmp(ptrArray[counter-2], ">") == 0) {
          outFile = ptrArray[counter-1];
          free(ptrArray[counter-2]);
          ptrArray[counter-2] = NULL; 

          if (counter-4 >= 0) {
            if (strcmp(ptrArray[counter-4], "<") == 0) {
              inFile = ptrArray[counter-3];
              free(ptrArray[counter-4]); 
              ptrArray[counter-4] = NULL;
              break;
            }
          }
          break;
        }
        break; 
      }
      break; 
    }
  }

  print_array(elements, ptrArray);  
}



static void print_prompt(char *ps1_env) {
  // expand PS1 environment variable.. unset -> "" | fprintf(stderr, _) prints to stderr
  ps1_env = getenv("PS1");
  if (ps1_env == NULL) { ps1_env = ""; }
  fprintf(stderr, "%s",ps1_env);

}

static void perform_expansion(pid_t backgroundProcessId, int exitStatusForeground, int elements, char *ptrArray[]) {




  char exit_str[10];
  char process_id[10];

  char temp[50];
  char *expan_env = NULL;
  expan_env = getenv("HOME");
  printf("\nexpan_env is %s\n", expan_env); 

  int i = 0;
  printf("size of ptrArray is %d\n", elements);

  // elements - 1 to disregard the NULL pointer @ the end 
  while (i < elements-1) {

    // ensure each token is >= size 2
    if (strlen(ptrArray[i]) >= 2) {

      // bullet point 1: “~/” at the beginning of a word shall be replaced with the value of the HOME environment variable
      if (strncmp(ptrArray[i], "~/", 2) == 0) {
        if (expan_env == NULL) {
          expan_env = ""; 
        }
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
      sprintf(exit_str, "%d", exitStatusForeground);
      str_substitute(&ptrArray[i], "$?", exit_str); 

      // bullet point 4: occurrence of “$!” within a word shall be replaced with the process ID of the most recent background process (see waiting)
      if (backgroundProcessId == -110) {
        str_substitute(&ptrArray[i], "$!", ""); 
      }
      else {
        sprintf(process_id, "%d", backgroundProcessId);
        str_substitute(&ptrArray[i], "$!", process_id);
      }

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
    perror("mallloc failed, exiting...\n");
    exit(1); 
  }

  strcpy(ptrArray[index], copy);
  free(copy);
  elements++;

  for (;;) {

    index++;
    copy = NULL;

    token = strtok(NULL, ifs_env);
    printf("Next Token =%s\n", token); 

    if (token == NULL) { 
      ptrArray[index] = (char*) malloc(sizeof(*ptrArray)); 
      ptrArray[index] = token;
      elements++; 
      printf("NULL TOKEN IS INSERTED\n"); 
      break;}

    copy = strdup(token);

    ptrArray[index] = (char*)malloc((strlen(token) + 1) * sizeof (*ptrArray));

    if (ptrArray[index] == NULL) {
      perror("mallloc failed, exiting...\n");
      exit(1); 
    }

    strcpy(ptrArray[index], copy);
    free(copy);
    elements++; 
  }


  printf("The strings are:\n");

  // print the ptrArray
  print_array(elements, ptrArray);  

  printf("Done splitting word and printing tokens.\n");

  printf("Number of elements from split_word is: %d\n", elements); 

  return elements;  

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


static void manage_background_processes() {

  // get process group id of calling process..always sucess 
  // pid_t currGroupId = getpgrp();
  pid_t childPID = 0;
  int childStatus = 0;

  // WNOHANG flag - waitpid() returns 0 when no child process terminates...
  // WUNTRACED constant - for stopped processes
  //      -1 --> fork() failed
  //      0 --> child process executes chunk 
  //      > 0 --> child process PID 

  while ((childPID = waitpid(0, &childStatus, WNOHANG | WUNTRACED)) > 0) {

    /*pid_t childPGID = getpgid(childPID);

      if (childPGID == -1) {
      perror("There was an error calling getpgid().\n"); 
      }
    // check if child process group id of current child process == parent process group id
    if (getpgid(childPID) == currGroupId) {

*/

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
        perror("kill() failed with the childPID.\n"); 
      }
      fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) childPID); 
    }

    // other state changes: ignore 
    // }
  }

  if (childPID < 0) {
    perror("Either no existing background processes or no more. waitpid() fails and returns...");  
  }
}

/* Our signal handler for SIGINT */
static void handle_SIGINT(int signo){
  char* message = "SIGINT handler() called.\n";
  // We are using write rather than printf
  write(STDOUT_FILENO, message, 39);
}

/*
   void exit(int argc, char *argv[]) {

   int exit_status = 0;

// get process group id of calling process..always sucess 
pid_t currGroupId = getpgrp();
pid_t childPID = 0;
int childStatus = 0;


// arg not provided
if (argc == 1) {
exit_status = atoi(getenv("$?"));
}

// too many args
else if (argc > 2) {
printf("Error, more than one argument provided...\n"); 
}

// arg provided is not an int 
else if (argc == 2) {
int i = 0;

// validate each input char is int
for (i = 0; argv[1][i] != '\0'; i++) {
if (isdigit(argv[1][i]) == 0) {
printf("Error, argument provided is not an integer"); 
}
}

// if success, set arg as exit status
exit_status = atoi(argv[1]); 
}

// print exit to stderr
fprintf(stderr, "\nexit\n");

// kill child processes in process group id w/ SIGINT.. don't wait using WNOHANG
while ((childPID = waitpid(-1, &childStatus, WNOHANG)) > 0) {

pid_t childPGID = getpgid(childPID);

if (childPGID == -1) {
perror("There was an error calling getpgid().\n"); 
}
// check if child process group id of current child process == parent process group id
if (getpgid(childPID) == currGroupId) {

int killStatus = 0;

// if SIGCONT kill() fails
if ((killStatus = kill(childPID, SIGINT)) == -1) {
perror("kill() failed with the childPID.\n"); 
}

}

}

exit(exit_status); 

}

*/
