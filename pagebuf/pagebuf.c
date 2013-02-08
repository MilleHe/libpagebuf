/*******************************************************************************
 *  Copyright 2013 Nick Jones <nick.fa.jones@gmail.com>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ******************************************************************************/

#include "pagebuf.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>


/*******************************************************************************
 */
struct pb_data *pb_data_create(void *buf, uint16_t len) {
  struct pb_data *data = malloc(sizeof(struct pb_data));
  if (!data)
    return NULL;

  memset(data, 0, sizeof(struct pb_data));

  data->base = buf;
  data->len = len;

  data->use_count = 1;
  data->responsibility = pb_data_owned;

  data->root = NULL;

  return data;
}

struct pb_data *pb_data_create_buf_ref(const void *buf, uint16_t len) {
  struct pb_data *data = malloc(sizeof(struct pb_data));
  if (!data)
    return NULL;

  memset(data, 0, sizeof(struct pb_data));

  data->base = (void*)buf;
  data->len = len;

  data->use_count = 1;
  data->responsibility = pb_data_referenced;

  data->root = NULL;

  return data;
}

struct pb_data *pb_data_create_data_ref(
    struct pb_data *root_data, uint64_t off, uint16_t len) {
  struct pb_data *data = malloc(sizeof(struct pb_data));
  if (!data)
    return NULL;

  memset(data, 0, sizeof(struct pb_data));

  data->base = (char*)root_data->base + off;
  data->len = len;

  data->use_count = 1;
  data->responsibility = pb_data_referenced;

  data->root = root_data;
  pb_data_get(data->root);

  return data;
}

void pb_data_destroy(struct pb_data *data) {
  if (data->responsibility == pb_data_owned)
    free(data->base);
  else if (data->root)
    pb_data_put(data->root);

  data->base = NULL;
  data->len = 0;

  data->use_count = 0;

  data->root = NULL;

  free(data);
}

void pb_data_get(struct pb_data *data) {
  __sync_add_and_fetch(&data->use_count, 1);
}

void pb_data_put(struct pb_data *data) {
  if (__sync_sub_and_fetch(&data->use_count, 1) != 0)
    return;

  pb_data_destroy(data);
}



/*******************************************************************************
 */
struct pb_page *pb_page_create(struct pb_data *data) {
  struct pb_page *page = malloc(sizeof(struct pb_page));
  if (!page)
    return NULL;

  memset(page, 0, sizeof(struct pb_page));

  page->base = data->base;
  page->len = data->len;
  page->data = data;
  page->prev = NULL;
  page->next = NULL;

  pb_data_get(data);

  return page;
}

struct pb_page *pb_page_clone(const struct pb_page *src_page) {
  struct pb_page *page = malloc(sizeof(struct pb_page));
  if (!page)
    return NULL;

  memset(page, 0, sizeof(struct pb_page));

  page->base = src_page->base;
  page->len = src_page->len;
  page->data = src_page->data;
  page->prev = NULL;
  page->next = NULL;

  pb_data_get(page->data);

  return page;
}

void pb_page_destroy(struct pb_page *page) {
  pb_data_put(page->data);

  page->base = NULL;
  page->len = 0;
  page->data = NULL;
  page->prev = NULL;
  page->next = NULL;

  free(page);
}



/*******************************************************************************
 */
void pb_page_list_clear(struct pb_page_list *list) {
  struct pb_page *itr = list->head;

  while (itr) {
    struct pb_page *page = itr;

    itr = itr->next;

    pb_page_destroy(page);
  }

  list->head = NULL;
  list->tail = NULL;
}

uint64_t pb_page_list_get_size(const struct pb_page_list *list) {
  uint64_t size = 0;
  struct pb_page *itr = list->head;

  while (itr) {
    size += itr->len;

    itr = itr->next;
  }

  return size;
}

/*******************************************************************************
 */
bool pb_page_list_prepend_data(
    struct pb_page_list *list, struct pb_data *data) {
  struct pb_page *page = pb_page_create(data);
  if (!page)
    return false;

  if (list->head == NULL) {
    list->head = page;
    list->tail = page;

    return true;
  }

  list->head->prev = page;
  page->next = list->head;
  list->head = page;

  return true;
}

bool pb_page_list_append_data(
    struct pb_page_list *list, struct pb_data *data) {
  struct pb_page *page = pb_page_create(data);
  if (!page)
    return false;

  if (list->head == NULL) {
    list->head = page;
    list->tail = page;

    return true;
  }

  list->tail->next = page;
  page->prev = list->tail;
  list->tail = page;

  return true;
}

bool pb_page_list_append_page_clone(
    struct pb_page_list *list, const struct pb_page *page) {
  struct pb_page *new_page = pb_page_clone(page);
  if (!new_page)
    return false;

  if (list->head == NULL) {
    list->head = new_page;
    list->tail = new_page;

    return true;
  }

  list->tail->next = new_page;
  new_page->prev = list->tail;
  list->tail = new_page;

  return true;
}

/*******************************************************************************
 */
uint64_t pb_page_list_reserve(
    struct pb_page_list *list, uint64_t len, uint16_t max_page_len) {
  uint64_t reserved = 0;

  while (len > 0) {
    uint16_t to_reserve;
    void *bytes;
    struct pb_data *data;
    bool append_result;

    to_reserve = (len < UINT16_MAX) ? len : UINT16_MAX;
    to_reserve =
      ((max_page_len != 0) && (max_page_len < to_reserve)) ?
        max_page_len : to_reserve;

    bytes = malloc(to_reserve);
    if (!bytes)
      break;

    data = pb_data_create(bytes, to_reserve);
    if (!data) {
      free(bytes);

      break;
    }

    append_result = pb_page_list_append_data(list, data);
    pb_data_put(data);
    if (!append_result)
      break;

    reserved += to_reserve;
    len -= to_reserve;
  }

  return reserved;
}

/*******************************************************************************
 */
uint64_t pb_page_list_write_data(
    struct pb_page_list *list, const void *buf, uint64_t len) {
  uint64_t written = 0;
  void *bytes;
  struct pb_data *root_data;

  bytes = malloc(len);
  if (!bytes)
    return written;

  memcpy(bytes, buf, len);

  root_data = pb_data_create(bytes, 0);
  if (!root_data) {
    free(bytes);

    return written;
  }

  while (len > 0) {
    uint16_t to_write = (len < UINT16_MAX) ? len : UINT16_MAX;
    struct pb_data *data;
    bool append_result;

    data = pb_data_create_data_ref(root_data, written, to_write);
    if (!data)
      break;

    append_result = pb_page_list_append_data(list, data);
    pb_data_put(data);
    if (!append_result)
      break;

    written += to_write;
    len -= to_write;
  }

  pb_data_put(root_data);

  return written;
}

uint64_t pb_page_list_write_data_ref(
    struct pb_page_list *list, const void *buf, uint64_t len) {
  uint64_t written = 0;

  while (len > 0) {
    uint16_t to_write = (len < UINT16_MAX) ? len : UINT16_MAX;
    struct pb_data *data;
    bool append_result;

    data = pb_data_create_buf_ref((char*)buf + written, to_write);
    if (!data)
      break;

    append_result = pb_page_list_append_data(list, data);
    pb_data_put(data);
    if (!append_result)
      break;

    written += to_write;
    len -= to_write;
  }

  return written;
}

uint64_t pb_page_list_write_page_list(struct pb_page_list *list,
    const struct pb_page_list *src_list, uint64_t len) {
  uint64_t written = 0;
  struct pb_page *itr = src_list->head;

  while ((len > 0) && (itr)) {
    uint16_t to_write = (len < itr->len) ? len : itr->len;

    if (!pb_page_list_append_page_clone(list, itr))
      break;

    list->tail->len = to_write;

    written += to_write;
    len -= to_write;

    itr = itr->next;
  }

  return written;
}

/*******************************************************************************
 */
uint64_t pb_page_list_seek(struct pb_page_list *list, uint64_t len) {
  uint64_t seeked = 0;
  struct pb_page *itr = list->head;

  while ((len > 0) && (itr)) {
    uint16_t to_seek = (len < itr->len) ? len : itr->len;

    itr->base = (char*)itr->base + to_seek;
    itr->len -= to_seek;

    if (itr->len == 0) {
      list->head = itr->next;
      if (list->head)
        list->head->prev = NULL;
      else
        list->tail = NULL;

      pb_page_destroy(itr);
    }

    seeked += to_seek;
    len -= to_seek;

    itr = list->head;
  }

  return seeked;
}

uint64_t pb_page_list_trim(struct pb_page_list *list, uint64_t len) {
  uint64_t trimmed = 0;
  struct pb_page *itr = list->tail;

  while ((len > 0) && (itr)) {
    uint16_t to_trim = (len < itr->len) ? len : itr->len;

    itr->len -= to_trim;

    if (itr->len == 0) {
      list->tail = itr->prev;
      if (list->tail)
        list->tail->next = NULL;
      else
        list->head = NULL;

      pb_page_destroy(itr);
    }

    trimmed += to_trim;
    len -= to_trim;

    itr = list->tail;
  }

  return trimmed;
}

uint64_t pb_page_list_rewind(struct pb_page_list *list, uint64_t len) {
  uint64_t rewinded = 0;

  while (len > 0) {
    uint16_t to_rewind = (len < UINT16_MAX) ? len : UINT16_MAX;
    void *bytes;
    struct pb_data *data;
    bool prepend_result;

    bytes = malloc(to_rewind);
    if (!bytes)
      break;

    data = pb_data_create(bytes, to_rewind);
    if (!data) {
      free(bytes);

      break;
    }

    prepend_result = pb_page_list_prepend_data(list, data);
    pb_data_put(data);
    if (!prepend_result)
      break;

    rewinded += to_rewind;
    len -= to_rewind;
  }

  return rewinded;
}

/*******************************************************************************
 */
uint64_t pb_page_list_read_data(
    struct pb_page_list *list, void *buf, uint64_t len) {
  uint64_t readed = 0;
  struct pb_page *itr = list->head;

  while ((len > 0) && (itr)) {
    uint16_t to_read = (len < itr->len) ? len : itr->len;

    memcpy((char*)buf + readed, itr->base, to_read);

    readed += to_read;
    len -= to_read;

    itr = itr->next;
  }

  return readed;
}

/*******************************************************************************
 */
bool pb_page_list_dup(
    struct pb_page_list *list, const struct pb_page_list *src_list,
    uint64_t off, uint64_t len) {
  struct pb_page *itr = src_list->head;

  while ((off > 0) && (itr)) {
    if (off < itr->len)
      break;

    off -= itr->len;

    itr = itr->next;
  }

  while ((len > 0) && (itr)) {
    if (!pb_page_list_append_page_clone(list, itr))
      return false;

    if (off > 0) {
      list->tail->base = (char*)itr->base + off;
      list->tail->len -= off;

      off = 0;
    }

    uint16_t to_retain = (len < list->tail->len) ? len : list->tail->len;

    list->tail->len = to_retain;

    len -= to_retain;

    itr = itr->next;
  }

  return true;
}



/*******************************************************************************
 */
struct pb_buffer *pb_buffer_create() {
  struct pb_buffer *buffer = malloc(sizeof(struct pb_buffer));
  if (!buffer)
    return NULL;

  memset(buffer, 0, sizeof(struct pb_buffer));

  pb_buffer_clear(buffer);

  buffer->reserve_max_page_len = PB_BUFFER_DEFAULT_PAGE_SIZE;

  return buffer;
}

/*******************************************************************************
 */
void pb_buffer_destroy(struct pb_buffer *buffer) {
  pb_buffer_clear(buffer);

  buffer->reserve_max_page_len = 0;

  free(buffer);
}

/*******************************************************************************
 */
void pb_buffer_optimise(struct pb_buffer *buffer) {
  pb_page_list_clear(&buffer->write_list);
  pb_page_list_clear(&buffer->retain_list);
}

void pb_buffer_clear(struct pb_buffer *buffer) {
  pb_buffer_optimise(buffer);

  pb_page_list_clear(&buffer->data_list);

  buffer->data_size = 0;

  buffer->is_data_dirty = false;
}

/*******************************************************************************
 */
uint64_t pb_buffer_get_data_size_ro(const struct pb_buffer *buffer) {
  if (!buffer->is_data_dirty)
    return buffer->data_size;

  return pb_page_list_get_size(&buffer->data_list);
}

uint64_t pb_buffer_get_data_size(struct pb_buffer *buffer) {
  if (!buffer->is_data_dirty)
    return buffer->data_size;

  buffer->data_size = pb_page_list_get_size(&buffer->data_list);
  buffer->is_data_dirty = false;

  return buffer->data_size;
}

bool pb_buffer_is_empty_ro(const struct pb_buffer *buffer) {
  return (pb_buffer_get_data_size_ro(buffer) == 0);
}

bool pb_buffer_is_empty(struct pb_buffer *buffer) {
  return (pb_buffer_get_data_size(buffer) == 0);
}

/*******************************************************************************
 */
uint64_t pb_buffer_reserve(struct pb_buffer *buffer, uint64_t len) {
  uint64_t capacity = 0;

  capacity = pb_page_list_get_size(&buffer->write_list);
  if (capacity >= len)
    return len;

  return
    capacity +
    pb_page_list_reserve(
      &buffer->write_list, buffer->reserve_max_page_len, len - capacity);
}

uint64_t pb_buffer_push(struct pb_buffer *buffer, uint64_t len) {
  uint64_t pushed =
    pb_page_list_write_page_list(&buffer->data_list, &buffer->write_list, len);

  buffer->is_data_dirty = true;

  return pushed;
}

/*******************************************************************************
 */
uint64_t pb_buffer_write_data(
    struct pb_buffer *buffer, const void *buf, uint64_t len) {
  uint64_t written = pb_page_list_write_data(&buffer->data_list, buf, len);

  buffer->is_data_dirty = true;

  return written;
}

uint64_t pb_buffer_write_data_ref(
    struct pb_buffer *buffer, const void *buf, uint64_t len) {
  uint64_t written = pb_page_list_write_data_ref(&buffer->data_list, buf, len);

  buffer->is_data_dirty = true;

  return written;
}

uint64_t pb_buffer_write_buf(
    struct pb_buffer *buffer, const struct pb_buffer *src_buffer, uint64_t len) {
  uint64_t written =
    pb_page_list_write_page_list(
      &buffer->data_list, &src_buffer->data_list, len);

  buffer->is_data_dirty = true;

  return written;
}

/*******************************************************************************
 */
uint64_t pb_buffer_seek(struct pb_buffer *buffer, uint64_t len) {
  uint64_t seeked = pb_page_list_seek(&buffer->data_list, len);

  buffer->is_data_dirty = true;

  return seeked;
}

uint64_t pb_buffer_trim(struct pb_buffer *buffer, uint64_t len) {
  uint64_t trimmed = pb_page_list_trim(&buffer->data_list, len);

  buffer->is_data_dirty = true;

  return trimmed;
}

uint64_t pb_buffer_rewind(struct pb_buffer *buffer, uint64_t len) {
  return 0;
}

/*******************************************************************************
 */
uint64_t pb_buffer_read_data(
    struct pb_buffer *buffer, void *buf, uint64_t len) {
  return pb_page_list_read_data(&buffer->data_list, buf, len);
}

/*******************************************************************************
 */
struct pb_buffer *pb_buffer_dup(struct pb_buffer *src_buffer) {
  struct pb_buffer *buffer = pb_buffer_create();
  if (!buffer)
    return NULL;

  if (!pb_page_list_dup(&buffer->data_list, &src_buffer->data_list, 0, 0)) {
    pb_buffer_destroy(buffer);

    return NULL;
  }

  return buffer;
}
struct pb_buffer *pb_buffer_dup_seek(
    struct pb_buffer *src_buffer, uint64_t off) {
  struct pb_buffer *buffer = pb_buffer_create();
  if (!buffer)
    return NULL;

  if (!pb_page_list_dup(&buffer->data_list, &src_buffer->data_list, off, 0)) {
    pb_buffer_destroy(buffer);

    return NULL;
  }

  return buffer;
}
struct pb_buffer *pb_buffer_dup_trim(
    struct pb_buffer *src_buffer, uint64_t len) {
  struct pb_buffer *buffer = pb_buffer_create();
  if (!buffer)
    return NULL;

  if (!pb_page_list_dup(&buffer->data_list, &src_buffer->data_list, 0, len)) {
    pb_buffer_destroy(buffer);

    return NULL;
  }

  return buffer;
}
struct pb_buffer *pb_buffer_dup_sub(
    struct pb_buffer *src_buffer, uint64_t off, uint64_t len) {
  struct pb_buffer *buffer = pb_buffer_create();
  if (!buffer)
    return NULL;

  if (!pb_page_list_dup(&buffer->data_list, &src_buffer->data_list, off, len)) {
    pb_buffer_destroy(buffer);

    return NULL;
  }

  return buffer;
}



/*******************************************************************************
 */
void pb_iterator_init(
    const struct pb_buffer *buffer, struct pb_iterator *iterator,
    bool is_reverse) {
  iterator->vec.base = NULL;
  iterator->vec.len = 0;

  iterator->is_reverse = is_reverse;

  if (!iterator->is_reverse)
    iterator->page = buffer->data_list.head;
  else
    iterator->page = buffer->data_list.tail;
}

bool pb_iterator_is_reverse(struct pb_iterator *iterator) {
  return iterator->is_reverse;
}

bool pb_iterator_has_is_valid(struct pb_iterator *iterator) {
  return (iterator->page != NULL);
}

void pb_iterator_next(struct pb_iterator *iterator) {
  if (!iterator->is_reverse)
    iterator->page = iterator->page->next;
  else
    iterator->page = iterator->page->prev;
}

const struct pb_vec *pb_iterator_get_vec(struct pb_iterator *iterator) {
  if (!iterator->page) {
    iterator->vec.base = NULL;
    iterator->vec.len = 0;

    return &iterator->vec;
  }

  iterator->vec.base = iterator->page->base;
  iterator->vec.len = iterator->page->len;

  return &iterator->vec;
}

