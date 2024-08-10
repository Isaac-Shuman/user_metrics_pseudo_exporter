#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> //used for checking if line is empy
#include <signal.h> //used for stopping gracefully
#include <unistd.h> //for sleeping
#include <errno.h> //need o insert perror("whatever I want") and exit(errno) later
#include <fcntl.h> 
#include <assert.h>
#include <stdbool.h>
#include "uthash.h"

#define MAX_LINE_SIZE 256
#define MAX_FILE_PATH 256
#define MAX_USER_SIZE 64 //only used for mallocing the user name when storing it in the user_attributes structure
#define MAX_COMMAND_SIZE 64
#define DELAY 3

#define TOP_WIDTH 512 //passed with -w flag

#define PID_WIDTH  8
#define PGID_WIDTH 8
#define USER_WIDTH 16
#define COMM_WIDTH 16
#define PCPU_WIDTH 5
#define PMEM_WIDTH 5

#define STRINGIZE(x) #x
#define STRINGIZE_WRAP(x) STRINGIZE(x)

//where you would like to write the metrics for node-exporter. Make sure you also change the --collector.textfile.directory when running node_exporter
//#define EXPOSITION_FILENAME "/tmp/added_by_pseudo_exporter.prom"
#define DEBUG
//#define GATHER_TOP
//#define GATHER_SLURM
#define GATHER_PS

#define NUM_OF_TOP_METRICS 3
#define NUM_OF_SLURM_METRICS 2
#define NUM_OF_PS_ATTRIBUTES 6

#define WRITE_METRICS_FLOAT(metric_name, help_msg, type)                                                 \
  do {                                                                                                   \
    struct process_group_attributes *current_user;                                                                \
    struct process_group_attributes *tmp;                                                                         \
    FILE *file = fopen(EXPOSITION_FILENAME, "a");                                                        \                                       
    fprintf(file, "\n# HELP " #metric_name " " help_msg "\n# TYPE " #metric_name " " type "\n");                      \
    HASH_ITER(hh, hash_table, current_user, tmp) {                                                      \
      fprintf(file, #metric_name"{user=\"%s\"} %.2f\n", current_user->user, (double) current_user->metric_name);   \
    }                                                                                                    \
    fclose(file);                                                                                        \
  } while (0)       

#define WRITE_METRICS_PG_FLOAT(metric_name, help_msg, type)                                                 \
  do {                                                                                                   \
    struct process_group_attributes *current_user;                                                                \
    struct process_group_attributes *tmp;                                                                         \
    FILE *file = fopen(EXPOSITION_FILENAME, "a");                                                        \                                       
    fprintf(file, "\n# HELP " #metric_name " " help_msg "\n# TYPE " #metric_name " " type "\n");                      \
    HASH_ITER(hh, hash_table, current_user, tmp) {                                                      \
      fprintf(file, #metric_name"{user=\"%s\",pgid=\"%i\",command=\"%s\"} %.2f\n",  \
      current_user->user, current_user->pgid, current_user->command, (double) current_user->metric_name);   \
    }                                                                                                    \
    fclose(file);                                                                                        \
  } while (0)       

int find_empty_line(FILE *fp, char *line, int line_size);
int is_line_empty(const char *line);
bool contains_non_ascii_printable(const char *str);
int find_cols_of_fields(char **fields, int size_of_fields, int *col_nums, FILE *fp, char *line, int line_size);
int parse_top_for_metrics(FILE *fp, char *line, int line_size, int *col_nums);
int parse_slurm_for_metrics(FILE *fp, char *line, int line_size, int *col_nums);
int parse_ps_for_metrics(FILE *fp, char *line, int line_size, int *col_nums, \
size_t pid_width, size_t pgid_width, size_t user_width, size_t comm_width, size_t pcpu_width, size_t pmem_width);


struct process_group_attributes *init_process_group_atts(int pgid);
void clear_table(void);
void handle_sigint(int sig);


struct process_group_attributes {
  int pgid;
  char command[MAX_COMMAND_SIZE];
  char user[MAX_USER_SIZE];
  double cpu_usage;
  double ram_usage;
  int ncpus;
  UT_hash_handle hh; //makes this structure hashable
};

struct process_group_attributes *hash_table = NULL;

FILE *fp; //change to top_fp
FILE *slurm_fp;
FILE *ps_fp;

int main(int argc, char **argv) {
    char line[MAX_LINE_SIZE];
    char filepath[MAX_FILE_PATH];
    long buffer_size;
    long read_size;
    char *fields[] = {"USER", "%CPU", "%MEM"};
    int col_nums[NUM_OF_TOP_METRICS];
    char *slurm_fields[] = {"User", "NCPUS"};
    int slurm_col_nums[NUM_OF_SLURM_METRICS];
    char *ps_fields[] = {"PID", "PGID", "USER", "COMMAND", "%CPU", "%MEM"};
    int ps_col_nums[NUM_OF_PS_ATTRIBUTES]; 

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
      //write_top_metrics();
      #endif

      #ifdef GATHER_SLURM
      slurm_fp = popen("sacct -r gpus  --allusers --format=User,ncpus -X -s R", "r"); //should fail silently
      find_cols_of_fields((char **) slurm_fields, sizeof(slurm_fields)/sizeof(slurm_fields[0]), slurm_col_nums, slurm_fp, line, sizeof(line));
      if (fgets(line, sizeof(line), slurm_fp) == NULL) //skip line
            return 1;
      parse_slurm_for_metrics(slurm_fp, line, sizeof(line), slurm_col_nums);
      #endif

      #ifdef GATHER_PS
      ps_fp = popen("ps -e --no-header -o pid:"STRINGIZE_WRAP(PID_WIDTH)",pgid:"STRINGIZE_WRAP(PGID_WIDTH)",user:"STRINGIZE_WRAP(USER_WIDTH)\
      ",comm:"STRINGIZE_WRAP(COMM_WIDTH)",pcpu:"STRINGIZE_WRAP(PCPU_WIDTH)",pmem:"STRINGIZE_WRAP(PMEM_WIDTH), "r");
      if (ps_fp == NULL) {
        printf("Failed to run command\n" );
        handle_sigint(0);
        return 1;
      }
      //find_cols_of_fields((char **) ps_fields, sizeof(ps_fields)/sizeof(ps_fields[0]), ps_col_nums, ps_fp, line, sizeof(line));
      parse_ps_for_metrics(ps_fp, line, sizeof(line), ps_col_nums, PID_WIDTH, PGID_WIDTH, USER_WIDTH, COMM_WIDTH, PCPU_WIDTH, PMEM_WIDTH);
      #endif

      fclose(fopen(EXPOSITION_FILENAME, "w")); //empty file
      #ifdef GATHER_TOP
      WRITE_METRICS_FLOAT(cpu_usage, "how much cpu a user is using", "gauge");
      WRITE_METRICS_FLOAT(ram_usage, "how much ram a user is using", "gauge");
      #endif
      #ifdef GATHER_SLURM
      WRITE_METRICS_FLOAT(ncpus, "how many cpus the user is using on the gpu nodes", "gauge");
      #endif
      #ifdef GATHER_PS
      WRITE_METRICS_PG_FLOAT(cpu_usage, "how much cpu a user is using", "gauge");
      WRITE_METRICS_PG_FLOAT(ram_usage, "how much ram a user is using", "gauge");
      #endif

      sleep(DELAY);

      //double free if CTRL_C is hit during this block
      clear_table();
      //find_empty_line(fp, line, sizeof(line));
      #ifdef GATHER_TOP
      pclose(fp);
      #endif
      #ifdef GATHER_SLURM
      pclose(slurm_fp);
      #endif
      #ifdef GATHER_PS
      pclose(ps_fp);
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
  char *token = NULL;
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

bool contains_non_ascii_printable(const char *str) {
    while (*str) {
        if ((unsigned char)*str > 127) {
            return true; // Non-ASCII character found
        }
        str++;
    }
    return false; // All characters are ASCII
}

int parse_ps_for_metrics(FILE *fp, char *line, int line_size, int *col_nums, \
                        size_t pid_width, size_t pgid_width, size_t user_width, size_t comm_width, size_t pcpu_width, size_t pmem_width) {
  // char pid[pid_width+1];
  // char pgid[pgid_width+1];
  int pid = 0;
  int pgid = 0;
  char user[user_width+1];
  char comm[comm_width+1];
  //char pcpu[pcpu_width+1];
  //char pmem[pmem_width+1];
  double pcpu = 0.0;
  double pmem = 0.0;
  struct process_group_attributes *new_atts = NULL;
  while (fgets(line, line_size, fp) != NULL) {
    // strncpy(pid, line, pid_width);
    // strncpy(pgid, line + pid_width + 1, pgid_width);
    // strncpy(user, line + pid_width + pgid_width + 2, user_width);
    // strncpy(comm, line + pid_width + pgid_width + user_width + 3, comm_width);
    // strncpy(pcpu, line + pid_width + pgid_width + user_width + comm_width + 4, pcpu_width);
    // strncpy(pmem, line + pid_width + pgid_width + user_width + comm_width + pcpu_width + 5, pmem_width);
    sscanf(line, "%i %i %s %"STRINGIZE_WRAP(COMM_WIDTH)"c %lf %lf", &pid, &pgid, user, comm, &pcpu, &pmem);
    #ifdef DEBUG
    printf("pid is %i\n", pid);
    assert(pid <= 9999999 && pid >= 0);
    printf("pgid is %i\n", pgid);
    assert(pgid <= 9999999 && pgid >= 0);
    printf("user is %s\n", user);
    assert(strlen(user) >= 3);
    if (contains_non_ascii_printable(user)) {
      perror("user buffer contains garbage");
      exit(1);
    }
    printf("command is %s\n", comm);
    assert(strlen(comm) >= 2);
    if (contains_non_ascii_printable(comm)) {
      perror("comm buffer contains garbage");
      exit(1);
    }
    printf("CPU usage is %lf\n", pcpu);
    assert(pcpu >= 0.0 && pcpu <= 9999);
    printf("Memory usage is %lf\n", pmem);
    assert(pmem >= 0.0 && pmem <= 9999);
    #endif

    user[user_width] = '\0';
    comm[comm_width] = '\0';

    HASH_FIND_INT(hash_table, &pgid, new_atts);
    if (new_atts == NULL) {
      new_atts = init_process_group_atts(pgid);
      HASH_ADD_INT(hash_table, pgid, new_atts);
      //ensures process groups alwalys have a user or comm listed
      strcpy(new_atts->user, user);
      strcpy(new_atts->command, comm);
    }
    if (pid == pgid) {//only occurs for process group leader
        strcpy(new_atts->user, user);
        strcpy(new_atts->command, comm);
    }
    new_atts->cpu_usage = new_atts->cpu_usage + pcpu;
    new_atts->ram_usage = new_atts->ram_usage + pmem;
  }

}

struct process_group_attributes *init_process_group_atts(int pgid) {
  /*Don't forget to free the malloced structure*/
  struct process_group_attributes *new_user_atts;
  new_user_atts = malloc(sizeof(struct process_group_attributes));
  //strcpy(new_user_atts->user, username);
  new_user_atts->cpu_usage = 0.0;
  new_user_atts->ram_usage = 0.0;
  new_user_atts->ncpus = 0;
  new_user_atts->pgid = pgid;

  return new_user_atts;
}

void clear_table(void)
{
    struct process_group_attributes *current_user;
    struct process_group_attributes *tmp;

    HASH_ITER(hh, hash_table, current_user, tmp) {
        HASH_DEL(hash_table, current_user);  /* delete it (users advances to next) */
        free(current_user);             /* free it */
    }
}


void handle_sigint(int sig) {
  printf("\nMetrics writer exiting\n");

  clear_table();
  pclose(fp);
  pclose(slurm_fp);

  exit(0);
}
