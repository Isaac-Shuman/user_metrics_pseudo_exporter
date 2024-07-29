#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> //used for checking if line is empy
#include <signal.h> //used for stopping gracefully
#include <unistd.h> //for sleeping

#include "uthash.h"
#define MAX_LINE_SIZE 256
#define MAX_USER_SIZE 64

int find_empty_line(FILE *fp, char *line, int line_size);
int is_line_empty(const char *line);
int find_cols_of_fields(char **fields, int size_of_fields, int *col_nums, FILE *fp, char *line, int line_size);
int parse_for_metrics(FILE *fp, char *line, int line_size, int *col_nums);
struct user_attributes *init_user_atts(char *username);
void clear_table(void);
void write_metrics();
void handle_sigint(int sig);

struct user_attributes {
  char user[MAX_USER_SIZE];
  double cpu_usage;
  double ram_usage;
  UT_hash_handle hh;         /* makes this structure hashable */
};

struct user_attributes *hash_table = NULL;

FILE *fp;

int main() {
    char line[MAX_LINE_SIZE];
    long buffer_size;
    long read_size;
    char *fields[] = {"USER", "%CPU", "%MEM"};
    int col_nums[3];

    signal(SIGINT, handle_sigint);

    while(1) {
      // Open the command for reading
      fp = popen("top -b -n 1", "r");
      if (fp == NULL) {
        printf("Failed to run command\n" );
        handle_sigint(0);
        return 1;
      }


      //iterate until we read an empty line
      find_empty_line(fp, line, sizeof(line));

      find_cols_of_fields((char **) fields, sizeof(fields)/sizeof(fields[0]), col_nums, fp, line, sizeof(line));

      parse_for_metrics(fp, line, sizeof(line), col_nums);

      write_metrics(fp);

      sleep(5);
      pclose(fp); //the latency between this and popen allows for a potential double free
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

  // for (int i=0; i < size_of_fields; i++) {
  //   printf("%i\n", col_nums[i]);
  // }
  return 0;
}

int parse_for_metrics(FILE *fp, char *line, int line_size, int *col_nums) {
  char *token = token;
  int c = 0;
  struct user_attributes *new_user_atts;
  
  while (fgets(line, line_size, fp) != NULL) {
    c = 0;
    token = strtok(line, " ");
    while (token != NULL) {
      if (c == col_nums[0]) {
        //printf("%s ", token);
        HASH_FIND_STR(hash_table, token, new_user_atts);
        if (new_user_atts == NULL) {
          new_user_atts = init_user_atts(token);
          HASH_ADD_STR(hash_table, user, new_user_atts);
        }
      }
      else if (c == col_nums[1]) {
        //printf("%s ", token);
        new_user_atts->cpu_usage = new_user_atts->cpu_usage + atof(token);
      }
      else if (c == col_nums[2]) {
        //printf("%s \n", token);
        new_user_atts->ram_usage = new_user_atts->ram_usage + atof(token);
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

void write_metrics() {
  struct user_attributes *current_user;
  struct user_attributes *tmp;
  FILE *file = fopen("added_by_exporter.prom", "w");

  //add test case for making sure there are no traling spaces

  fprintf(file, "# HELP cpu_usage how much cpu a user is using\n\
# TYPE cpu_usage gauge\n");
  fprintf(file, "cpu_usage{user=\"your_mom\"} %.2f\n", 4.0);
  
  HASH_ITER(hh, hash_table, current_user, tmp) {
      fprintf(file, "cpu_usage{user=\"%s\"} %.2f\n", current_user->user, current_user->cpu_usage);
  }

  fprintf(file, "\n# HELP ram_usage how much ram a user is using\n\# TYPE ram_usage gauge\n");
  fprintf(file, "ram_usage{user=\"your_mom\"} %.2f\n", 4.0);
  
  HASH_ITER(hh, hash_table, current_user, tmp) {
    fprintf(file, "ram_usage{user=\"%s\"} %.2f\n", current_user->user, current_user->ram_usage);
  }
  
  fclose(file);
}

void handle_sigint(int sig) {
  printf("Metrics writer exiting\n");

  clear_table();
  pclose(fp);

  exit(0);
}
