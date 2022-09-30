//
//  csvReader.c
//  libCacheSim
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include "../../libCacheSim/include/libCacheSim/macro.h"
#include "libcsv.h"
#include "readerInternal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief count the number of times char c appears in string str
 *
 * @param str
 * @param c
 * @return int
 */
static int count_occurrence(const char *str, const char c) {
  int count = 0;
  for (int i = 0; i < strlen(str); i++) {
    if (str[i] == c) count++;
  }
  return count;
}

/**
 * @brief read the first line of the trace without changing the state of the
 * reader
 *
 * @param reader
 * @param in_buf
 * @param in_buf_size
 * @return int
 */
static int read_first_line(const reader_t *reader, char *in_buf,
                           const size_t in_buf_size) {
  FILE *ifile = fopen(reader->trace_path, "r");
  char *buf = NULL;
  size_t n = 0;
  size_t read_size = getline(&buf, &n, ifile);

  if (in_buf_size < read_size) {
    WARN(
        "in_buf_size %zu is smaller than the first line size %zu, "
        "the first line will be truncated",
        in_buf_size, read_size);
  }

  read_size = read_size > in_buf_size ? in_buf_size : read_size;

  /* + 1 to copy the null terminator */
  memcpy(in_buf, buf, read_size + 1);

  fclose(ifile);
  free(buf);

  return read_size;
}

/**
 * @brief detect delimiter in the csv trace
 * it uses a simple algorithm: count the occurrence of each possible delimiter
 * in the first line, the delimiter with the most occurrence is the delimiter
 * we support 5 possible delimiters: '\t', ',', '|', ':'
 *
 * @param reader
 * @return char
 */
static char csv_detect_delimiter(const reader_t *reader) {
  char first_line[1024] = {0};
  int _buf_size = read_first_line(reader, first_line, 1024);

  char possible_delims[4] = {'\t', ',', '|', ':'};
  int possible_delim_counts[4] = {0, 0, 0, 0};
  int max_count = 0;
  char delimiter = 0;

  for (int i = 0; i < 4; i++) {
    // (5-i) account for the commonality of the delimiters
    possible_delim_counts[i] =
        count_occurrence(first_line, possible_delims[i]) * (4 - i);
    if (possible_delim_counts[i] > max_count) {
      max_count = possible_delim_counts[i];
      delimiter = possible_delims[i];
    }
  }

  assert(delimiter != 0);
  static bool has_printed = false;
  if (!has_printed) {
    INFO("detect csv delimiter %c\n", delimiter);
    has_printed = true;
  }

  return delimiter;
}

/**
 * @brief detect whether the csv trace has header,
 * it uses a simple algorithm: compare the number of digits and letters in the
 * first line, if there are more digits than letters, then the first line is not
 * header, otherwise, the first line is header
 *
 * @param reader
 * @return true
 * @return false
 */
static bool csv_detect_header(const reader_t *reader) {
  char first_line[1024] = {0};
  int buf_size = read_first_line(reader, first_line, 1024);
  assert(buf_size < 1024);

  bool has_header = true;

  int n_digit = 0, n_af = 0, n_letter = 0;
  for (int i = 0; i < buf_size; i++) {
    if (isdigit(first_line[i])) {
      n_digit += 1;
    } else if (isalpha(first_line[i])) {
      n_letter += 1;
      if ((first_line[i] <= 'f' && first_line[i] >= 'a') ||
          (first_line[i] <= 'F' && first_line[i] >= 'A')) {
        /* a-f can be hex number */
        n_af += 1;
      }
    } else {
      ;
    }
  }

  if (n_digit > n_letter) {
    has_header = false;
  } else if (n_digit < n_letter - n_af) {
    has_header = true;
  }

  static bool has_printed = false;
  if (!has_printed) {
    INFO("detect csv trace has header %d\n", has_header);
    has_printed = true;
  }

  return has_header;
}

/**
 * @brief   call back for csv field end
 *
 * @param s     the string of the field
 * @param len   length of the string
 * @param data  user passed data: reader_t*
 */
static inline void csv_cb1(void *s, size_t len, void *data) {
  reader_t *reader = (reader_t *)data;
  csv_params_t *csv_params = reader->reader_params;
  request_t *req = csv_params->request;
  char *end;

  if (csv_params->curr_field_idx == csv_params->obj_id_field_idx) {
    if (reader->obj_id_type == OBJ_ID_NUM) {
      // len is not correct for the last request for some reason
      // req->obj_id = str_to_obj_id((char *) s, len);
      req->obj_id = atoll((char *)s);
    } else {
      if (len >= MAX_OBJ_ID_LEN) {
        len = MAX_OBJ_ID_LEN - 1;
        ERROR("csvReader obj_id len %zu larger than MAX_OBJ_ID_LEN %d\n", len,
              MAX_OBJ_ID_LEN);
        abort();
      }
      char obj_id_str[MAX_OBJ_ID_LEN];
      memcpy(obj_id_str, (char *)s, len);
      obj_id_str[len] = 0;
      req->obj_id = (uint64_t)g_quark_from_string(obj_id_str);
    }
  } else if (csv_params->curr_field_idx == csv_params->time_field_idx) {
    // this does not work, because s is not null terminated
    uint64_t ts = (uint64_t)atof((char *)s);
    // uint64_t ts = (uint64_t)strtod((char *)s, &end);
    // we only support 32-bit ts
    assert(ts < UINT32_MAX);
    req->real_time = ts;
  } else if (csv_params->curr_field_idx == csv_params->obj_size_field_idx) {
    req->obj_size = (uint32_t)strtoul((char *)s, &end, 0);
    if (req->obj_size == 0 && end == s) {
      ERROR("csvReader obj_size is not a number: %s\n", (char *)s);
      abort();
    }
  }

  // printf("cb1 %d '%s'\n", csv_params->curr_field_idx, (char *)s);
  csv_params->curr_field_idx++;
}

/**
 * @brief   call back for csv row end
 *
 * @param c     the character that ends the row
 * @param data  user passed data: reader_t*
 */
static inline void csv_cb2(int c, void *data) {
  reader_t *reader = (reader_t *)data;
  csv_params_t *csv_params = reader->reader_params;
  csv_params->curr_field_idx = 1;

  // printf("cb2 %d '%c'\n", csv_params->curr_field_idx, c);
}

/**
 * @brief setup a csv reader
 *
 * @param reader
 */
void csv_setup_reader(reader_t *const reader) {
  unsigned char options = CSV_APPEND_NULL;
  reader->trace_format = TXT_TRACE_FORMAT;
  reader_init_param_t *init_params = &reader->init_params;

  reader->reader_params = (csv_params_t *)malloc(sizeof(csv_params_t));
  csv_params_t *csv_params = reader->reader_params;
  csv_params->curr_field_idx = 1;

  csv_params->time_field_idx = init_params->time_field;
  csv_params->obj_id_field_idx = init_params->obj_id_field;
  csv_params->obj_size_field_idx = init_params->obj_size_field;
  csv_params->csv_parser =
      (struct csv_parser *)malloc(sizeof(struct csv_parser));

  if (csv_init(csv_params->csv_parser, options) != 0) {
    fprintf(stderr, "Failed to initialize csv parser\n");
    exit(1);
  }

  /* if we setup something here, then we must setup in the reset_reader func */
  if (init_params->delimiter == '\0') {
    char delim = csv_detect_delimiter(reader);
    csv_params->delimiter = delim;
  } else {
    csv_params->delimiter = init_params->delimiter;
  }
  csv_set_delim(csv_params->csv_parser, csv_params->delimiter);

  if (!init_params->has_header_set) {
    csv_params->has_header = csv_detect_header(reader);
  } else {
    csv_params->has_header = init_params->has_header;
  }
  if (csv_params->has_header) {
    getline(&reader->line_buf, &reader->line_buf_size, reader->file);
  }
}

/**
 * @brief read one request from a csv file
 *
 * @param reader
 * @param req
 * @return int
 */
int csv_read_one_req(reader_t *const reader, request_t *const req) {
  csv_params_t *csv_params = reader->reader_params;
  struct csv_parser *csv_parser = csv_params->csv_parser;
  char **line_buf_ptr = &reader->line_buf;
  size_t *line_buf_size_ptr = &reader->line_buf_size;

  csv_params->request = req;
  DEBUG_ASSERT(csv_params->curr_field_idx == 1);

  size_t offset_before_read = ftell(reader->file);
  int read_size = getline(line_buf_ptr, line_buf_size_ptr, reader->file);
  if (read_size == -1) {
    req->valid = false;
    return 1;
  }

  // printf("line %s read %d byte\n", *line_buf_ptr, read_size);
  if ((size_t)csv_parse(csv_parser, *line_buf_ptr, read_size, csv_cb1, csv_cb2,
                        reader) != read_size) {
    WARN("parsing csv file error: %s\n",
         csv_strerror(csv_error(csv_params->csv_parser)));
  }

  csv_fini(csv_params->csv_parser, csv_cb1, csv_cb2, reader);

  return 0;
}

void csv_reset_reader(reader_t *reader) {
  csv_params_t *csv_params = reader->reader_params;

  fseek(reader->file, 0L, SEEK_SET);

  csv_free(csv_params->csv_parser);
  csv_init(csv_params->csv_parser, CSV_APPEND_NULL);
  if (csv_params->delimiter)
    csv_set_delim(csv_params->csv_parser, csv_params->delimiter);

  if (csv_params->has_header) {
    getline(&reader->line_buf, &reader->line_buf_size, reader->file);
  }
}


#ifdef __cplusplus
}
#endif
