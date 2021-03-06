// This file is auto generated by icy-vector
#include "icydb.h"
typedef struct _pstring{
  char ** column_names;
  char ** column_types;
  size_t * count;
  size_t * capacity;
  size_t * free_index_count;
  const size_t column_count;
  icy_mem * free_indexes;
  icy_mem * header;
  const size_t column_sizes[1];
  
  u32 * key;
  icy_mem * key_area;
}pstring;

// a vector index.
typedef struct{
  size_t index;
}pstring_index;

typedef struct{
  size_t index;
  size_t count;
}pstring_indexes;

pstring * pstring_create(const char * optional_name);
pstring_index pstring_alloc(pstring * table);
pstring_indexes pstring_alloc_sequence(pstring * table, size_t count);
void pstring_remove(pstring * table, pstring_index index);
void pstring_remove_sequence(pstring * table, pstring_indexes * indexes);
void pstring_clear(pstring * table);
void pstring_optimize(pstring * table);
void pstring_destroy(pstring ** table);
