
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
#include <sys/stat.h>
#include <fcntl.h>



/* Convenient macro to get the length of an array (number of elements) */
#define arrlen(a) (sizeof(a) / sizeof *(a))

// function to split words and store into an array of string ptrs 
static int split_word(char *ifs_env, int elements, char *input, char *ptrArray[]);

// function to string search and replace
static char *str_substitute(char *restrict *restrict haystack, char const *restrict needle, char const *restrict substitute); 

// function to word expansion
static void perform_expansion(char *home_env, pid_t backgroundProcessId, int exitStatusForeground, int elements, char *ptrArray[]);

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

// function when built-in exit() called
static int exit_called(int elements, int *exitStatusForeground, char* ptrArray[]);

// function when built-in cd() called 
static int cd_called(char *home_env, int *exitStatusForeground, int elements, char *ptrArray[]);


int main(int argc, char *argv[]) {

  char *outFile = NULL;
  char *inFile = NULL; 

  int runInBackground = 0;
  int exitStatusForeground = 0;
  pid_t backgroundProcessId = -100;


  char *line = NULL;   // keep outside main infinite loop 
  size_t buff_size = 0;


  pid_t mainChildPid = 0;
  int mainChildStatus = 0;

  // PS1 environment var
  char *ps1_env = getenv("PS1");
  if (ps1_env == NULL) { ps1_env = ""; }

  // HOME environment var
  char *home_env = getenv("HOME");
  if (home_env == NULL) { home_env = ""; }

  char *ifs_env = getenv("IFS");
  if (ifs_env == NULL) {ifs_env = " \t\n"; }




  /* -------------------------------------------- */ 
  /*  SIGINT & SIGTSTP signal set-up (done) */
  /* --------------------------------------------- */
  struct sigaction SIGINT_action = {0}, ignore_action = {0}, old_int_action = {0}, old_tstp_action = {0};
  // Register handle_SIGINT as the signal handler
  SIGINT_action.sa_handler = handle_SIGINT;
  // Block all catchable signals while handle_SIGINT is running
  sigfillset(&SIGINT_action.sa_mask);
  // No flags set
  SIGINT_action.sa_flags = 0;

  // give SIGINT a signal handler at the start when user is prompted 
  sigaction(SIGINT, &SIGINT_action, &old_int_action);

  // Set SIGTSTP's action part of ignore_action struct -- SIG_IGN as its signal handler
  ignore_action.sa_handler = SIG_IGN;
  sigaction(SIGTSTP, &ignore_action, &old_tstp_action);


  if (feof(stdin) == 0) {
    fflush(stdin); 
  }

  for (;;) {
 int elements = 0;
    char *ptrArray[512] = {NULL}; 

    ssize_t bytes_read;  // later: maybe move into main infinite loop like example
    manage_background_processes(); 

    /*  ----  Input - The prompt (DONE) ------ */
    //print_prompt(ps1_env);
    fputs(ps1_env, stderr); 

    /* ------ Input - Reading a line of input (Done) ------------------ */

    // read input from stdin
    bytes_read = getline(&line, &buff_size, stdin);

    if (feof(stdin)) {
      exit(exitStatusForeground);
      break;
    }

    // signal interrupt or getline() fail...reset errno and  
    if (bytes_read == -1) {
      perror("Getline() failed.\n");
      clearerr(stdin);
      errno = 0;
      putchar('\n'); 
      continue; 
    }

    if (strcmp(line, "\n") == 0) {
      clearerr(stdin);
      continue;
    }
    /*  else {
        printf("\nRead number of bytes from getline(): %zd\n", bytes_read);
        printf("Moving onto word_splitting...\n"); 
        }
        */



    // reset SIGINT to be ignored throughout program EXCEPT @ getline() 
    sigaction(SIGINT, &ignore_action, NULL); 

    // split words
    elements = split_word(ifs_env, elements, line, ptrArray);

    //  printf("Size of elements after split_word and before perform_expansion: %d\n", elements); 


    // perform expansion 
    perform_expansion(home_env, backgroundProcessId, exitStatusForeground, elements, ptrArray);

    // print_array(elements, ptrArray); 

    // parse tokenized input 
    //   printf("\nParsing user input:\n"); 
    parse_user_input(outFile, inFile, &runInBackground, elements, ptrArray);


    /* At this point, ptrArray is updated w/ the Parsed/Expanded tokens... inFile + outFile updated*/

    // check if exit or cd commands 
    if (exit_called(elements, &exitStatusForeground, ptrArray) == -11) {continue;}
    if (cd_called(home_env, &exitStatusForeground, elements, ptrArray) == -11) {continue;}

    // fork a new process 
    pid_t spawnPid = fork(); 

    switch (spawnPid) {
      case -1: 
        // fork fail 
        fprintf(stderr, "fork failed.\n"); 
        exitStatusForeground = 1; 
        continue; 

      case 0:
        // In the child process
        //      printf("CHILD(%d) running ls command\n", getpid());
        // reset SIGINT / SIGTSTP signals
        sigaction(SIGINT, &old_int_action, NULL);
        sigaction(SIGTSTP, &old_tstp_action, NULL); 
        // child: redirect standard output to a file
        if (inFile != NULL) {
          int inFileFD = open(inFile, O_RDONLY); 
          if (inFileFD == -1) {
            fprintf(stderr, "inFileFD failed...\n"); 
            exit(1);
          }
          //         printf("inFileFD == %d", inFileFD); 

          // redirect stdin to source file 
          int redirected_input = dup2(inFileFD, 0); 
          if (redirected_input == -1) {
            fprintf(stderr, "redirected_input fail...\n"); 
            exit(2);
          }
        }

        // redirect std output to FILE... close stdOUT 
        if (outFile != NULL) {
          close(STDOUT_FILENO);
          int outFileFD = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0777); 
          if (outFileFD == -1) {
            fprintf(stderr, "outFileFD failed...\n"); 
            exit(1); 
          }
          //        printf("outFileFD == %d", outFileFD); 

          int redirected_output = dup2(outFileFD, 1);
          if (redirected_output == -1) {
            fprintf(stderr, "redirected_output fail...\n"); 
            exit(2);
          }
        }

        if (execvp(ptrArray[0], ptrArray) == -1) { 
          fprintf(stderr, "execvp() failed when calling command...exiting with non-zero status code\n"); 
          exit(2);
        }

      default:
        // in parent process 
        // Wait for child's termination
        if (runInBackground == 2) {
          // don't wait on child process
          mainChildPid = waitpid(mainChildPid, &mainChildStatus, WNOHANG);
          printf("In the parent process waitpid returned value %d\n", mainChildPid);
          backgroundProcessId = mainChildPid;
        }

        else {

          // blocking wait for child process 
          mainChildPid = waitpid(mainChildPid, &mainChildStatus, 0);
          //       printf("The parent is done waiting. The pid of child that terminated is %d\n", mainChildPid);

          if(WIFEXITED(mainChildStatus)){
            printf("Child %d exited normally with status %d\n", mainChildPid, WEXITSTATUS(mainChildStatus));
            exitStatusForeground = WEXITSTATUS(mainChildStatus);
          } 

          // if signaled
          if (WIFSIGNALED(mainChildStatus)) {
            int signalExitNumber = WTERMSIG(mainChildStatus);
            exitStatusForeground = signalExitNumber + 128;
            fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) mainChildPid, signalExitNumber);
          }

          // if stopped 
          if (WIFSTOPPED(mainChildStatus)) {
            int killStatus = 0;

            // if SIGCONT kill() fails
            if ((killStatus = kill(mainChildPid, SIGCONT)) == -1) {
              perror("kill() failed with the mainChildPid after fork.\n"); 
            }
            fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) mainChildPid); 
            backgroundProcessId = mainChildPid;
          }

          //    exit(3);
          break; 
        }
    }
    // exit:

    // free(line);
    line = NULL;

    // free each ptr array index
    for (int u = 0; u < elements; u++) {
      free(ptrArray[u]);

    }

  }
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
    if (counter != elements-2 && strcmp(ptrArray[counter], "#") != 0) {
      continue; 
    }

    // printf("parse_user_input is processing...continue iterations done...\n"); 

    //  # (comment token) found ... stop looking
    if (strcmp(ptrArray[counter], "#") == 0) {
      free(ptrArray[counter]); 
      ptrArray[counter] = NULL; 

      // check behind the #
      if (counter-1 >= 0) {

        // "&" exists behind "#" (comment) ... this conditional check does not affect < / > checks below 
        if (strcmp(ptrArray[counter-1], "&") == 0) {
          free(ptrArray[counter-1]);
          ptrArray[counter-1] = NULL; 
          *runInBackground = 2;
        }
        // check for "<" - input redirection operator
        if (counter-3 >= 0) {

          if (strcmp(ptrArray[counter-3], "<") == 0) {
            inFile = ptrArray[counter-2];
            free(ptrArray[counter-3]);
            ptrArray[counter-3] = NULL;

            // check for "> - output_file redirection operator
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
    else if (counter == elements-2) {

      //    printf("counter == elements-2 (aka end of line before NULL) reached\n"); 

      if (counter >= 0) {

        // "&" exists behind NULL  
        if (strcmp(ptrArray[counter], "&") == 0) {
          free(ptrArray[counter]);
          ptrArray[counter] = NULL; 
          *runInBackground = 2;


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
              }
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

                }
              }
            }
          }
          break; 
        }

        // counter (aka end of wordlist) is not & after checking it's not a #
        else {
          if (counter-1 >= 0) {
            if (strcmp(ptrArray[counter-1], "<") == 0) {
              inFile = ptrArray[counter];
              free(ptrArray[counter-1]);
              ptrArray[counter-1] = NULL;

              if (counter-3 >= 0) {
                if (strcmp(ptrArray[counter-3], ">") == 0) {
                  outFile = ptrArray[counter-2];
                  free(ptrArray[counter-3]);
                  ptrArray[counter-3] = NULL;
                }
              }
            }

            else if (strcmp(ptrArray[counter-1], ">") == 0) {
              outFile = ptrArray[counter];
              free(ptrArray[counter-1]);
              ptrArray[counter-1] = NULL;

              if (counter-3 >= 0) {
                if (strcmp(ptrArray[counter-3], "<") == 0) {
                  inFile = ptrArray[counter-2];
                  free(ptrArray[counter-3]);
                  ptrArray[counter-3] = NULL;
                }
              }
            }
          }
          break;
        }
      }
    }
  }

  // print_array(elements, ptrArray);
}



static void print_prompt(char *ps1_env) {

  fprintf(stderr, "%s",ps1_env);

}

static void perform_expansion(char *home_env, pid_t backgroundProcessId, int exitStatusForeground, int elements, char *ptrArray[]) {


  char exit_str[10];
  char process_id[10];

  char temp[50];


  int i = 0;
  //  printf("size of ptrArray is %d\n", elements);

  // elements - 1 to disregard the NULL pointer @ the end 
  while (i < elements-1) {

    // ensure each token is >= size 2
    if (strlen(ptrArray[i]) >= 2) {

      // bullet point 1: “~/” at the beginning of a word shall be replaced with the value of the HOME environment variable
      if (strncmp(ptrArray[i], "~/", 2) == 0) {
        str_substitute(&ptrArray[i], "~", home_env);
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


static int split_word(char *ifs_env, int elements, char *input, char *ptrArray[]) {

  char *token = NULL;
  int index = 0;

 /*
  char *ch = input; 

  printf("input char is: "); 
  while (*ch != '\0') {

    printf("%c", *ch); 
    ch++; 
  }

  */


  token = strtok(input, ifs_env);

  while (token != NULL) {
    ptrArray[index] = (char*)malloc((strlen(token) + 1) * sizeof (char));

    if (ptrArray[index] == NULL) {
      perror("mallloc failed, exiting...\n");
      exit(1); 
    }

    strcpy(ptrArray[index], token);
    //  free(token);
    elements++;


    index++;
    //  copy = NULL;

    token = strtok(NULL, ifs_env);
    //   printf("Next Token =%s\n", token); 

    if (token == NULL) { 
      ptrArray[index] = NULL; 
      elements++;
      //    printf("NULL TOKEN IS INSERTED\n"); 
      break;}
  }

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

static int cd_called(char *home_env, int *exitStatusForeground, int elements, char *ptrArray[]) {


  char *home_copy_env = strdup(home_env);

  size_t len = strlen(home_copy_env); 

  char *new_home = malloc(len+2);
  strcpy(new_home, home_copy_env);
  new_home[len] = '/';
  new_home[len+1] = '\0';

  // printf("The old string before / is %s\n", home_copy_env); 

  // printf("The new string after / is %s\n", home_copy_env); 

  if (strcmp(ptrArray[0], "cd") == 0) {

    // no args provided... `cd NULL`
    if (elements == 2) {
      if (chdir(home_copy_env) == 0) { 
        printf("chdir success\n"); 
        free(new_home);
      }
      else { 
        fprintf(stderr, "no args provided but chdir() failed...\n");
        *exitStatusForeground = 1;

      }
    }

    // more than 1 arg provided... `cd arg1 arg2 NULL` 
    else if (elements >3) {
      fprintf(stderr, "Failure..More than one arg provided to cd()\n");
      *exitStatusForeground = 1;
      goto door;
    }

    // good len... `cd arg NULL`
    else {
      if (chdir(ptrArray[1]) == 0) { printf("chdir success\n"); }
      else { 
        fprintf(stderr, "chdir() failed...good len of args"); 
        *exitStatusForeground = 1;
      }
    }
  }
  return 0;
door:
  printf("cd command error");
  return -11;

}


static int exit_called(int elements, int *exitStatusForeground, char *ptrArray[]) {

  int isAnInt = 2;
  int exit_val = -1111;

  // get process group id of calling process..always sucess 
  pid_t currGroupId = getpgrp();
  pid_t childPID = 0;
  int childStatus = 0;


  if (strcmp(ptrArray[0], "exit") == 0) {

    // no arg provided ... `exit (NULL)`
    if (elements == 2) {
      exit_val = *exitStatusForeground;
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
      exit(exit_val);
    }
    // more than 1 arg is provided... `exit arg1 arg2 (NULL)`
    else if (elements > 3) {
      fprintf(stderr, "Too many arguments provided to exit()...\n"); 
      *exitStatusForeground = 1; 
      return -11;
    }
    // good len... `exit arg (NULL)`
    else if (elements == 3) {
      // check if 2nd arg is an int or not
      for (int i = 0; ptrArray[1][i] != '\0'; i++) {
        printf("running in the for loop when elem == 3\n");
        printf("char is %c\n", ptrArray[1][i]); 
        if (isdigit(ptrArray[1][i]) == 0) {
          printf("Not an int\n"); 
          isAnInt = -2;
          fprintf(stderr, "Argument passed into exit() is not an int..\n");
          break;
        }
      }

      // arg is not int --> leave func call 
      if (isAnInt == -2) { 
        fprintf(stderr, "exit command argrs is not an int\n");
        *exitStatusForeground = 1;
        return -11;
      }
      // arg is int
      else if (isAnInt == 2) {

        exit_val = atoi(ptrArray[1]);
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
      }
      exit(exit_val);
    }

  }

  return 0;
}

