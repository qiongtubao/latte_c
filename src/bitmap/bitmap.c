#include "bitmap.h"

bitmap_t bitmap_new(int len) {
    sds_t map = sds_new_len("", len);
    if (map == NULL) return NULL;
    bitmap_clear_all(map);
    return map;
}

void bitmap_clear_all(bitmap_t map) {
    int size = bit_size(map);
    for (int iter = 0;  iter < size; iter++) {
        map[iter] = 0;
    }
}

bool bitmap_get(bitmap_t map ,int index) {
    char bits = map[index/8];
    return (bits & (1 << (index %8))) != 0;
}

void bitmap_set(bitmap_t map, int index) {
    char* bits = &map[index/8];
    *bits |= (1 << (index %8));
}

void bitmap_clear(bitmap_t map, int index) {
    char* bits = &map[index/8];
    *bits &= ~(1 << (index % 8));
}

int bytes(int size) { return size % 8 == 0 ? size / 8 : size / 8 + 1; }
int find_char_first_zero(char byte, int start)
{
  for (int i = start; i < 8; i++) {
    if ((byte & (1 << i)) == 0) {
      return i;
    }
  }
  return -1;
}

int find_char_first_setted(char byte, int start)
{
  for (int i = start; i < 8; i++) {
    if ((byte & (1 << i)) != 0) {
      return i;
    }
  }
  return -1;
}
int bitmap_next_unsetted(bitmap_t map, int start) {
    int ret = -1;
    int start_in_byte = start % 8;
    int size = bit_size(map);
    for (int iter = start / 8 , end = bytes(size) ; iter < end; iter++) {
        char byte = map[iter];
        if (byte != -1) {
            int index_in_byte = find_char_first_zero(byte, start_in_byte);
            if (index_in_byte >= 0) {
                ret = iter * 8 + index_in_byte;
                break;
            }
        }
        start_in_byte = 0;
    } 
    if (ret >= size) {
        ret = -1;
    }
    return ret;
}

int bitmap_next_setted(bitmap_t map, int start) {
    int ret = -1;
    int start_in_byte = start % 8;
    int size = bit_size(map);
    for (int iter = start / 8, end = bytes(size); iter < end; iter++) {
        char byte = map[iter];
        if (byte != 0x00) {
            int index_in_byte = find_char_first_setted(byte, start_in_byte);
            if (index_in_byte >= 0) {
                ret = iter * 8 + index_in_byte;
                break;
            }
        }
        start_in_byte = 0;
    }
    if (ret >= size) {
        ret = -1;
    }
    return ret;
}

//iterator 

typedef struct bitmap_iterator_data_t {
    bitmap_t bitmap;
    int start;
    int next;
} bitmap_iterator_data_t;

#define UNFIND -1
#define WILLFIND -2
bool setted_iterator_has_next(latte_iterator_t* iterator) {
    bitmap_iterator_data_t* data = iterator->data;
    data->next = bitmap_next_setted(data->bitmap, data->start + 1);
    return data->next != UNFIND;
}

void* bitmap_iterator_next(latte_iterator_t* iterator) {
    bitmap_iterator_data_t* data = iterator->data;
    if (data->next == UNFIND) return NULL;
    data->start = data->next;
    return (void*)(long)data->next;
}

void bitmap_iterator_delete(latte_iterator_t* iterator) {
    zfree(iterator->data);
    zfree(iterator);
}

latte_iterator_func latte_iterator_setted_func = {
    .has_next  = setted_iterator_has_next,
    .next = bitmap_iterator_next,
    .release = bitmap_iterator_delete
};




bitmap_iterator_data_t* bitmap_iterator_data_new(bitmap_t map, int start) {
    bitmap_iterator_data_t* data = zmalloc(sizeof(bitmap_iterator_data_t));
    data->bitmap = map;
    data->start = start-1;
    data->next = WILLFIND;
    return data;
}
latte_iterator_t* bitmap_get_setted_iterator(bitmap_t map, int start) {
    latte_iterator_t* iterator = latte_iterator_new(&latte_iterator_setted_func, 
    bitmap_iterator_data_new(map, start));
    return iterator;
}


bool unsetted_iterator_has_next(latte_iterator_t* iterator) {
    bitmap_iterator_data_t* data = iterator->data;
    data->next = bitmap_next_unsetted(data->bitmap, data->start + 1);
    return data->next != UNFIND;
}

latte_iterator_func latte_iterator_unsetted_func = {
    .has_next  = unsetted_iterator_has_next,
    .next = bitmap_iterator_next,
    .release = bitmap_iterator_delete
};
latte_iterator_t* bitmap_get_unsetted_iterator(bitmap_t map, int start) {
    latte_iterator_t* iterator = latte_iterator_new(&latte_iterator_unsetted_func, 
    bitmap_iterator_data_new(map, start));
    return iterator;
}