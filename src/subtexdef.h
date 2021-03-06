// This file is auto generated by icy-vector
#include "icydb.h"
typedef struct _subtexdef{
  char ** column_names;
  char ** column_types;
  size_t * count;
  size_t * capacity;
  size_t * free_index_count;
  const size_t column_count;
  icy_mem * free_indexes;
  icy_mem * header;
  const size_t column_sizes[5];
  
  f32 * x;
  f32 * y;
  f32 * w;
  f32 * h;
  texdef_index * texture;
  icy_mem * x_area;
  icy_mem * y_area;
  icy_mem * w_area;
  icy_mem * h_area;
  icy_mem * texture_area;
}subtexdef;

// a vector index.
typedef struct{
  size_t index;
}subtexdef_index;

typedef struct{
  size_t index;
  size_t count;
}subtexdef_indexes;

subtexdef * subtexdef_create(const char * optional_name);
subtexdef_index subtexdef_alloc(subtexdef * table);
subtexdef_indexes subtexdef_alloc_sequence(subtexdef * table, size_t count);
void subtexdef_remove(subtexdef * table, subtexdef_index index);
void subtexdef_remove_sequence(subtexdef * table, subtexdef_indexes * indexes);
void subtexdef_clear(subtexdef * table);
void subtexdef_optimize(subtexdef * table);
void subtexdef_destroy(subtexdef ** table);
