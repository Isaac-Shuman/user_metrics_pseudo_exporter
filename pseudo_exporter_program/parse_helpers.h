#include <stdio.h>
#include <ctype.h> //used for checking if line is empy
#include <stdbool.h>

int find_empty_line(FILE *fp, char *line, int line_size);
int is_line_empty(const char *line);
bool contains_non_ascii_printable(const char *str);
int find_cols_of_fields(const char **fields, int size_of_fields, int *col_nums, FILE *fp, char *line, int line_size);

int find_empty_line(FILE *fp, char *line, int line_size) {
/*Find an empty line in the file and consume it*/
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

int find_cols_of_fields(const char **fields, int size_of_fields, int *col_nums, FILE *fp, char *line, int line_size) {
/*Given an array of fields (e.g., PID, USER, %CPU) return the columns of these fields via col_nums (e.g., {1, 2, 3}) */
  char *token = NULL;
  int i = 0;
  int c = 0;
  if (fgets(line, line_size, fp) == NULL)
    return 1;
  //    PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM
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
/*used to check if a string is malformed*/
    while (*str) {
        if ((unsigned char)*str > 127) {
            return true; // Non-ASCII character found
        }
        str++;
    }
    return false; // All characters are ASCII
}