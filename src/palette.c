// This file is auto generated by icy-vector.
#ifndef TABLE_COMPILER_INDEX
#define TABLE_COMPILER_INDEX
#define array_element_size(array) sizeof(array[0])
#define array_count(array) (sizeof(array)/array_element_size(array))
#include "icydb.h"
#include <stdlib.h>
#endif

palette * palette_create(const char * optional_name){
  static const char * const column_names[] = {(char *)"color", (char *)"glow"};
  static const char * const column_types[] = {"u32", "u8"};

  palette * instance = calloc(sizeof(palette), 1);
  unsigned int sizes[] = {sizeof(u32), sizeof(u8)};
  
  instance->column_names = (char **)column_names;
  instance->column_types = (char **)column_types;
  
  ((size_t *)&instance->column_count)[0] = 2;
  for(unsigned int i = 0; i < 2; i++)
    ((size_t *)instance->column_sizes)[i] = sizes[i];

  icy_vector_abs_init((icy_vector_abs * )instance, optional_name);

  return instance;
}


palette_index palette_alloc(palette * table){
  icy_index idx = icy_vector_abs_alloc((icy_vector_abs *) table);
  palette_index index;
  index.index = idx.index;
  return index;
}

palette_indexes palette_alloc_sequence(palette * table, size_t count){
  icy_indexes idx = icy_vector_abs_alloc_sequence((icy_vector_abs *) table, count);
  palette_indexes index;
  index.index = idx.index.index;
  index.count = count;
  return index;
}

void palette_remove(palette * table, palette_index index){
  icy_index idx = {.index = index.index};
  icy_vector_abs_remove((icy_vector_abs *) table, idx);
}

void palette_remove_sequence(palette * table, palette_indexes * indexes){
  icy_indexes idxs;
  idxs.index.index = (unsigned int)indexes->index;
  idxs.count = indexes->count;
  icy_vector_abs_remove_sequence((icy_vector_abs *) table, &idxs);
  memset(&indexes, 0, sizeof(indexes));
  
}
void palette_clear(palette * table){
  icy_vector_abs_clear((icy_vector_abs *) table);
}

void palette_optimize(palette * table){
  icy_vector_abs_optimize((icy_vector_abs *) table);
}
void palette_destroy(palette ** table){
  icy_vector_abs_destroy((icy_vector_abs **) table);
}