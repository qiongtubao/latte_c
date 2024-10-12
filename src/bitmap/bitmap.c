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