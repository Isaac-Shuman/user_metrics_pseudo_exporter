#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> //used for checking if line is empy
#include <signal.h> //used for stopping gracefully
#include <unistd.h> //for sleeping
#include <errno.h> //need o insert perror("whatever I want") and exit(errno) later
#include <fcntl.h> 

#include "uthash.h"

#define MAX_LINE_SIZE 256
#define MAX_FILE_PATH 256
#define MAX_USER_SIZE 64 //only used for mallocing the user name when storing it in the user_attributes structure
#define DELAY 3

#define TOP_WIDTH 512 //passed with -w flag
#define STRINGIZE(x) #x
#define STRINGIZE_WRAP(x) STRINGIZE(x)

//where you would like to write the metrics for node-exporter. Make sure you also change the --collector.textfile.directory when running node_exporter
#define EXPOSITION_FILENAME "/tmp/added_by_pseudo_exporter.prom"

//#define DEBUG
#define GATHER_TOP
//#define GATHER_SLURM

#define NUM_OF_TOP_METRICS 3
#define NUM_OF_SLURM_METRICS 2

int find_empty_line(FILE *fp, char *line, int line_size);
int is_line_empty(const char *line);
int find_cols_of_fields(char **fields, int size_of_fields, int *col_nums, FILE *fp, char *line, int line_size);
int parse_top_for_metrics(FILE *fp, char *line, int line_size, int *col_nums);
void write_top_metrics(void);
int parse_slurm_for_metricss(FILE *fp, char *line, int line_size, int *col_nums);
void write_slurm_metrics(void);

struct user_attributes *init_user_atts(char *username);
void clear_table(void);
void handle_sigint(int sig);

struct user_attributes {
  char user[MAX_USER_SIZE];
  double cpu_usage;
  double ram_usage;
  int ncpus;
  UT_hash_handle hh; //makes this structure hashable
};

struct user_attributes *hash_table = NULL;

FILE *fp; //change to top_fp
FILE *slurm_fp;

int main(int argc, char **argv) {
    char line[MAX_LINE_SIZE];
    char filepath[MAX_FILE_PATH];
    long buffer_size;
    long read_size;
    char *fields[] = {"USER", "%CPU", "%MEM"};
    int col_nums[NUM_OF_TOP_METRICS];
    char *slurm_fields[] = {"User", "NCPUS"};
    int slurm_col_nums[NUM_OF_SLURM_METRICS];

    signal(SIGINT, handle_sigint);

    while(1) {
      //system("top -b -n 1 > added_by_exporter.prom");
      //fp = fopen("./added_by_exporter.prom", "r");

      #ifdef GATHER_TOP
      // Open the command for reading
      fp = popen("top -b -n 1 -w " STRINGIZE_WRAP(TOP_WIDTH), "r"); //assumes 512 is sufficient width
      if (fp == NULL) {
        printf("Failed to run command\n" );
        handle_sigint(0);
        return 1;
      }
      find_empty_line(fp, line, sizeof(line));
      find_cols_of_fields((char **) fields, sizeof(fields)/sizeof(fields[0]), col_nums, fp, line, sizeof(line)); //make sizeof stuff a macro??
      parse_top_for_metrics(fp, line, sizeof(line), col_nums);
      write_top_metrics();
      #endif

      #ifdef GATHER_SLURM
      slurm_fp = popen("sacct -r gpus  --allusers --format=User,ncpus -X -s R", "r"); //should fail silently
      find_cols_of_fields((char **) slurm_fields, sizeof(slurm_fields)/sizeof(slurm_fields[0]), slurm_col_nums, slurm_fp, line, sizeof(line));
      if (fgets(line, sizeof(line), slurm_fp) == NULL) //skip line
            return 1;
      parse_slurm_for_metrics(slurm_fp, line, sizeof(line), slurm_col_nums);
      write_slurm_metrics();
      #endif

      sleep(DELAY);

      //double free if CTRL_C is hit during this block
      clear_table();
      //find_empty_line(fp, line, sizeof(line));
      #ifdef GATHER_TOP
      fclose(fp);
      #endif
      #ifdef GATHER_SLURM
      fclose(slurm_fp)
      #endif
    }
    return 0; 

}

int find_empty_line(FILE *fp, char *line, int line_size) {
    do {
        if (fgets(line, line_size, fp) == NULL)
            return 1;
    } while (!is_line_empty(line));
}

int is_line_empty(const char *line) {
    // Check if the line is NULL or empty
    if (line == NULL || *line == '\0') {
        return 1; // Line is empty
    }
    // Check if the line contains only whitespace characters
    while (*line) {
        if (!isspace(*line)) {
            return 0; // Line is not empty
        }
        line++;
    }
    return 1; // Line is empty
}

int find_cols_of_fields(char **fields, int size_of_fields, int *col_nums, FILE *fp, char *line, int line_size) {
  if (fgets(line, line_size, fp) == NULL)
    return 1;
  //    PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM
  char *token;
  int i = 0;
  int c = 0;
  token = strtok(line, " \n");
  while (token != NULL) {
    if (strcmp(token, fields[i]) == 0) {
      col_nums[i] = c;
      i++;
      if (i == size_of_fields)
        break;
    }

    token = strtok(NULL, " \n");
    c++;
  }

  #if defined DEBUG
  for (int i=0; i < size_of_fields; i++) {
    printf("%i\n", col_nums[i]);
  }
  #endif
  return 0;
}

int parse_top_for_metrics(FILE *fp, char *line, int line_size, int *col_nums) {
  char *token = token;
  int c = 0;
  struct user_attributes *new_user_atts;
  
  while (fgets(line, line_size, fp) != NULL) {
    c = 0;
    token = strtok(line, " ");
    while (token != NULL) {
      if (c == col_nums[0]) {
        #ifdef DEBUG
        printf("%s ", token);
        #endif
        HASH_FIND_STR(hash_table, token, new_user_atts);
        if (new_user_atts == NULL) {
          new_user_atts = init_user_atts(token);
          HASH_ADD_STR(hash_table, user, new_user_atts);
        }
      }
      else if (c == col_nums[1]) {
        #ifdef DEBUG
        printf("%s ", token);
        #endif
        new_user_atts->cpu_usage = new_user_atts->cpu_usage + atof(token);
      }
      else if (c == col_nums[2]) {
        #ifdef DEBUG
        printf("%s \n", token);
        #endif
        new_user_atts->ram_usage = new_user_atts->ram_usage + atof(token);
      }
      token = strtok(NULL, " ");
      c++;
    }
  }

  return 0;
}

int parse_slurm_for_metrics(FILE *fp, char *line, int line_size, int *col_nums) {
  char *token = token;
  int c = 0;
  struct user_attributes *new_user_atts;
  
  while (fgets(line, line_size, fp) != NULL) {
    c = 0;
    token = strtok(line, " ");
    while (token != NULL) {
      if (c == col_nums[0]) {
        #ifdef DEBUG
        printf("%s ", token);
        #endif
        HASH_FIND_STR(hash_table, token, new_user_atts);
        if (new_user_atts == NULL) {
          new_user_atts = init_user_atts(token);
          HASH_ADD_STR(hash_table, user, new_user_atts);
        }
      }
      else if (c == col_nums[1]) {
        #ifdef DEBUG
        printf("%s ", token);
        #endif
        new_user_atts->ncpus = new_user_atts->ncpus + atoi(token);
      }
      token = strtok(NULL, " ");
      c++;
    }
  }
  return 0;
}

struct user_attributes *init_user_atts(char *username) {
  struct user_attributes *new_user_atts;
  new_user_atts = malloc(sizeof(struct user_attributes));
  strcpy(new_user_atts->user, username);
  new_user_atts->cpu_usage = 0.0;
  new_user_atts->ram_usage = 0.0;

  return new_user_atts;
}

void clear_table(void)
{
    struct user_attributes *current_user;
    struct user_attributes *tmp;

    HASH_ITER(hh, hash_table, current_user, tmp) {
        HASH_DEL(hash_table, current_user);  /* delete it (users advances to next) */
        free(current_user);             /* free it */
    }
}

void write_top_metrics(void) {
  struct user_attributes *current_user;
  struct user_attributes *tmp;
  FILE *file = fopen(EXPOSITION_FILENAME, "w");

  //add test case for making sure there are no traling spaces

  fprintf(file, "# HELP cpu_usage how much cpu a user is using\n\
# TYPE cpu_usage gauge\n");
  fprintf(file, "cpu_usage{user=\"not_from_top\"} %.2f\n", 4.0);
  
  HASH_ITER(hh, hash_table, current_user, tmp) {
      fprintf(file, "cpu_usage{user=\"%s\"} %.2f\n", current_user->user, current_user->cpu_usage);
  }

  fprintf(file, "\n# HELP ram_usage how much ram a user is using\n\# TYPE ram_usage gauge\n");
  fprintf(file, "ram_usage{user=\"not_from_top\"} %.2f\n", 4.0);
  
  HASH_ITER(hh, hash_table, current_user, tmp) {
    fprintf(file, "ram_usage{user=\"%s\"} %.2f\n", current_user->user, current_user->ram_usage);
  }
  fclose(file);
}

void write_slurm_metrics(void) {
  struct user_attributes *current_user;
  struct user_attributes *tmp;

  #ifdef GATHER_TOP
  FILE *file = fopen(EXPOSITION_FILENAME, "a"); //assumes top has been run prior
  #else
  FILE *file = fopen(EXPOSITION_FILENAME, "w");
  #endif

  //add test case for making sure there are no traling spaces

  fprintf(file, "# HELP ncpu how many cpus the user is using on the gpu nodes\n\
# TYPE ncpu gauge\n");
  fprintf(file, "cpu_usage{user=\"not_from_top\"} %.2f\n", 4.0);
  
  HASH_ITER(hh, hash_table, current_user, tmp) {
      fprintf(file, "cpu_usage{user=\"%s\"} %.2f\n", current_user->user, current_user->ncpus);
  }

  fclose(file);
}


void handle_sigint(int sig) {
  printf("\nMetrics writer exiting\n");

  clear_table();
  pclose(fp);

  exit(0);
}
