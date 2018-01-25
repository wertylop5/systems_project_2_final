#include "server.h"

typedef struct {
  struct chat_room *array;
  size_t len;
  size_t size;
} Array;

void init_arr(Array *, size_t);
void insert(Array *, struct chat_room);
void free_arr(Array *);
