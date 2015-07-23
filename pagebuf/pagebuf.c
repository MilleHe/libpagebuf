/*******************************************************************************
 *  Copyright 2013-2015 Nick Jones <nick.fa.jones@gmail.com>
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
static void *pb_trivial_alloc(const struct pb_allocator *allocator,
    enum pb_allocator_type type, size_t size) {
  void *obj = malloc(size);
  if (!obj)
    return NULL;

  if (type == pb_alloc_type_struct)
    memset(obj, 0, size);

  return obj;
};

static void pb_trivial_free(const struct pb_allocator *allocator,
    enum pb_allocator_type type, void *obj, size_t size) {
  memset(obj, 0, size);

  free(obj);
}

static struct pb_allocator pb_trivial_allocator = {
  .alloc = pb_trivial_alloc,
  .free = pb_trivial_free,
};

const struct pb_allocator *pb_get_trivial_allocator(void) {
  return &pb_trivial_allocator;
}



/*******************************************************************************
 */
struct pb_data *pb_data_create(uint8_t * const buf, size_t len,
    const struct pb_allocator *allocator) {
  struct pb_data *data =
    allocator->alloc(allocator, pb_alloc_type_struct, sizeof(struct pb_data));
  if (!data)
    return NULL;

  data->data_vec.base = buf;
  data->data_vec.len = len;

  data->use_count = 1;

  data->responsibility = pb_data_owned;

  data->allocator = allocator;

  return data;
}

struct pb_data *pb_data_create_ref(const uint8_t *buf, size_t len,
    const struct pb_allocator *allocator) {
  struct pb_data *data =
    allocator->alloc(allocator, pb_alloc_type_struct, sizeof(struct pb_data));
  if (!data)
    return NULL;

  data->data_vec.base = (uint8_t*)buf; // we won't actually change it
  data->data_vec.len = len;

  data->use_count = 1;

  data->responsibility = pb_data_referenced;

  data->allocator = allocator;

  return data;
}

struct pb_data *pb_data_clone(uint8_t * const buf, size_t len, size_t src_off,
    const struct pb_data *src_data,
    const struct pb_allocator *allocator) {
  struct pb_data *data =
    allocator->alloc(allocator, pb_alloc_type_struct, sizeof(struct pb_data));
  if (!data)
    return NULL;

  data->data_vec.base = buf;
  data->data_vec.len = len;

  memcpy(data->data_vec.base, src_data->data_vec.base + src_off, len);

  data->use_count = 1;

  data->responsibility = pb_data_owned;

  data->allocator = allocator;

  return data;
  }

void pb_data_destroy(struct pb_data * const data) {
  const struct pb_allocator *allocator = data->allocator;

  if (data->responsibility == pb_data_owned)
    allocator->free(
      allocator, pb_alloc_type_struct, data->data_vec.base, data->data_vec.len);

  allocator->free(
    allocator, pb_alloc_type_struct, data, sizeof(struct pb_data));
}

/*******************************************************************************
 */
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
struct pb_page *pb_page_create(struct pb_data *data,
    const struct pb_allocator *allocator) {
  struct pb_page *page =
    allocator->alloc(allocator, pb_alloc_type_struct, sizeof(struct pb_page));
  if (!page)
    return NULL;

  pb_data_get(data);

  page->data_vec.base = data->data_vec.base;
  page->data_vec.len = data->data_vec.len;
  page->data = data;
  page->prev = NULL;
  page->next = NULL;

  return page;
}

struct pb_page *pb_page_transfer(const struct pb_page *src_page,
    size_t len, size_t src_off,
    const struct pb_allocator *allocator) {
  struct pb_page *page =
    allocator->alloc(allocator, pb_alloc_type_struct, sizeof(struct pb_page));
  if (!page)
    return NULL;

  pb_data_get(src_page->data);

  page->data_vec.base = src_page->data_vec.base + src_off;
  page->data_vec.len = len;
  page->data = src_page->data;
  page->prev = NULL;
  page->next = NULL;

  return page;
}

void pb_page_destroy(struct pb_page *page,
    const struct pb_allocator *allocator) {
  pb_data_put(page->data);

  page->data_vec.base = NULL;
  page->data_vec.len = 0;
  page->data = NULL;
  page->prev = NULL;
  page->next = NULL;

  allocator->free(
    allocator, pb_alloc_type_struct, page, sizeof(struct pb_page));
}



/*******************************************************************************
 */
static struct pb_buffer_strategy pb_trivial_buffer_strategy = {
  .page_size = PB_BUFFER_DEFAULT_PAGE_SIZE,
  .clone_on_write = false,
  .fragment_as_target = false,
  .supports_insert = true,
};

const struct pb_buffer_strategy *pb_get_trivial_buffer_strategy(void) {
  return &pb_trivial_buffer_strategy;
}



/*******************************************************************************
 */
static struct pb_buffer_operations pb_trivial_buffer_operations = {
  .get_data_revision = &pb_trivial_buffer_get_data_revision,

  .get_data_size = &pb_trivial_buffer_get_data_size,

  .insert = &pb_trivial_buffer_insert,
  .seek = &pb_trivial_buffer_seek,
  .reserve = &pb_trivial_buffer_reserve,
  .rewind = &pb_trivial_buffer_rewind,

  .get_iterator = &pb_trivial_buffer_get_iterator,
  .get_iterator_end = &pb_trivial_buffer_get_iterator_end,
  .iterator_is_end = &pb_trivial_buffer_iterator_is_end,
  .iterator_cmp = &pb_trivial_buffer_iterator_cmp,
  .iterator_next = &pb_trivial_buffer_iterator_next,
  .iterator_prev = &pb_trivial_buffer_iterator_prev,

  .get_byte_iterator = &pb_trivial_buffer_get_byte_iterator,
  .get_byte_iterator_end = &pb_trivial_buffer_get_byte_iterator_end,
  .byte_iterator_is_end = &pb_trivial_buffer_byte_iterator_is_end,
  .byte_iterator_cmp = &pb_trivial_buffer_byte_iterator_cmp,
  .byte_iterator_next = &pb_trivial_buffer_byte_iterator_next,
  .byte_iterator_prev = &pb_trivial_buffer_byte_iterator_prev,

  .write_data = &pb_trivial_buffer_write_data,
  .write_data_ref = &pb_trivial_buffer_write_data_ref,
  .write_buffer = &pb_trivial_buffer_write_buffer,

  .overwrite_data = &pb_trivial_buffer_overwrite_data,

  .read_data = &pb_trivial_buffer_read_data,

  .clear = &pb_trivial_buffer_clear,
  .destroy = &pb_trivial_buffer_destroy,
};

const struct pb_buffer_operations *pb_get_trivial_buffer_operations(void) {
  return &pb_trivial_buffer_operations;
}



/*******************************************************************************
 */
struct pb_trivial_buffer {
  struct pb_buffer buffer;

  struct pb_page page_end;

  uint64_t data_revision;
  uint64_t data_size;
};



/*******************************************************************************
 */
uint64_t pb_buffer_get_data_revision(struct pb_buffer * const buffer) {
  return buffer->operations->get_data_revision(buffer);
}

/*******************************************************************************
 */
uint64_t pb_buffer_get_data_size(struct pb_buffer * const buffer) {
  return buffer->operations->get_data_size(buffer);
}

/*******************************************************************************
 */
uint64_t pb_buffer_insert(struct pb_buffer * const buffer,
    struct pb_page * const page,
    struct pb_buffer_iterator * const buffer_iterator,
    size_t offset) {
  return buffer->operations->insert(buffer, page, buffer_iterator, offset);
}

uint64_t pb_buffer_reserve(struct pb_buffer * const buffer,
  uint64_t len) {
  return buffer->operations->reserve(buffer, len);
}

uint64_t pb_buffer_rewind(struct pb_buffer * const buffer,
  uint64_t len) {
  return buffer->operations->rewind(buffer, len);
}

uint64_t pb_buffer_seek(struct pb_buffer * const buffer, uint64_t len) {
  return buffer->operations->seek(buffer, len);
}

/*******************************************************************************
 */
void pb_buffer_get_iterator(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const iterator) {
  buffer->operations->get_iterator(buffer, iterator);
}

void pb_buffer_get_iterator_end(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const iterator) {
  buffer->operations->get_iterator_end(buffer, iterator);
}

bool pb_buffer_iterator_is_end(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const iterator) {
  return buffer->operations->iterator_is_end(buffer, iterator);
}

bool pb_buffer_iterator_cmp(struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *lvalue,
    const struct pb_buffer_iterator *rvalue) {
  return buffer->operations->iterator_cmp(buffer, lvalue, rvalue);
}

void pb_buffer_iterator_next(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const iterator) {
  buffer->operations->iterator_next(buffer, iterator);
}

void pb_buffer_iterator_prev(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const iterator) {
  buffer->operations->iterator_prev(buffer, iterator);
}

/*******************************************************************************
 */
void pb_buffer_get_byte_iterator(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const iterator) {
  buffer->operations->get_byte_iterator(buffer, iterator);
}

void pb_buffer_get_byte_iterator_end(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const iterator) {
  buffer->operations->get_byte_iterator_end(buffer, iterator);
}

bool pb_buffer_byte_iterator_is_end(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const iterator) {
  return buffer->operations->byte_iterator_is_end(buffer, iterator);
}

bool pb_buffer_byte_iterator_cmp(struct pb_buffer * const buffer,
    const struct pb_buffer_byte_iterator *lvalue,
    const struct pb_buffer_byte_iterator *rvalue) {
  return buffer->operations->byte_iterator_cmp(buffer, lvalue, rvalue);
}

void pb_buffer_byte_iterator_next(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const iterator) {
  buffer->operations->byte_iterator_next(buffer, iterator);
}

void pb_buffer_byte_iterator_prev(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const iterator) {
  buffer->operations->byte_iterator_prev(buffer, iterator);
}

/*******************************************************************************
 */
uint64_t pb_buffer_write_data(struct pb_buffer * const buffer,
    const uint8_t *buf,
    uint64_t len) {
  return buffer->operations->write_data(buffer, buf, len);
}

uint64_t pb_buffer_write_data_ref(struct pb_buffer * const buffer,
    const uint8_t *buf,
    uint64_t len) {
  return buffer->operations->write_data_ref(buffer, buf, len);
}

uint64_t pb_buffer_write_buffer(struct pb_buffer * const buffer,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  return buffer->operations->write_buffer(buffer, src_buffer, len);
}

uint64_t pb_buffer_overwrite_data(struct pb_buffer * const buffer,
    const uint8_t *buf,
    uint64_t len) {
  return buffer->operations->overwrite_data(buffer, buf, len);
}


uint64_t pb_buffer_read_data(struct pb_buffer * const buffer,
    uint8_t * const buf,
    uint64_t len) {
  return buffer->operations->read_data(buffer, buf, len);
}

/*******************************************************************************
 */
void pb_buffer_clear(struct pb_buffer * const buffer) {
  buffer->operations->clear(buffer);
}

void pb_buffer_destroy(struct pb_buffer * const buffer) {
  buffer->operations->destroy(buffer);
}



/*******************************************************************************
 */
struct pb_buffer* pb_trivial_buffer_create(void) {
  return
    pb_trivial_buffer_create_with_strategy_with_alloc(
      pb_get_trivial_buffer_strategy(), pb_get_trivial_allocator());
}

struct pb_buffer *pb_trivial_buffer_create_with_strategy(
    const struct pb_buffer_strategy *strategy) {
  return
    pb_trivial_buffer_create_with_strategy_with_alloc(
      strategy, pb_get_trivial_allocator());
}

struct pb_buffer *pb_trivial_buffer_create_with_alloc(
    const struct pb_allocator *allocator) {
  return
    pb_trivial_buffer_create_with_strategy_with_alloc(
      pb_get_trivial_buffer_strategy(), allocator);
}

struct pb_buffer *pb_trivial_buffer_create_with_strategy_with_alloc(
    const struct pb_buffer_strategy *strategy,
    const struct pb_allocator *allocator) {
  struct pb_trivial_buffer *trivial_buffer =
    allocator->alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_trivial_buffer));
  if (!trivial_buffer)
    return NULL;

  ((struct pb_buffer_strategy*)&trivial_buffer->buffer.strategy)->
    page_size = strategy->page_size;
  ((struct pb_buffer_strategy*)&trivial_buffer->buffer.strategy)->
    clone_on_write = strategy->clone_on_write;
  ((struct pb_buffer_strategy*)&trivial_buffer->buffer.strategy)->
    fragment_as_target = strategy->fragment_as_target;
  ((struct pb_buffer_strategy*)&trivial_buffer->buffer.strategy)->
    supports_insert = strategy->supports_insert;

  trivial_buffer->buffer.operations = pb_get_trivial_buffer_operations();

  trivial_buffer->buffer.allocator = allocator;

  trivial_buffer->page_end.prev = &trivial_buffer->page_end;
  trivial_buffer->page_end.next = &trivial_buffer->page_end;

  trivial_buffer->data_revision = 0;
  trivial_buffer->data_size = 0;

  return &trivial_buffer->buffer;
}

/*******************************************************************************
 */
static void pb_trivial_buffer_increment_data_revision(
    struct pb_buffer * const buffer) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  ++trivial_buffer->data_revision;
}

uint64_t pb_trivial_buffer_get_data_revision(struct pb_buffer * const buffer) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  return trivial_buffer->data_revision;
}

/*******************************************************************************
 */
static void pb_trivial_buffer_increment_data_size(
    struct pb_buffer * const buffer, uint64_t size) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  trivial_buffer->data_size += size;
}

static void pb_trivial_buffer_decrement_data_size(
    struct pb_buffer * const buffer, uint64_t size) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  trivial_buffer->data_size -= size;
}

uint64_t pb_trivial_buffer_get_data_size(struct pb_buffer * const buffer) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  return trivial_buffer->data_size;
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_insert(struct pb_buffer * const buffer,
    struct pb_page * const page,
    struct pb_buffer_iterator * const buffer_iterator,
    size_t offset) {
  if (!pb_buffer_iterator_is_end(buffer, buffer_iterator) &&
      !buffer->strategy.supports_insert)
    return 0;

  while ((offset > 0) &&
         (offset >= buffer_iterator->page->data_vec.len)) {
    pb_buffer_iterator_next(buffer, buffer_iterator);

    if (!pb_buffer_iterator_is_end(buffer, buffer_iterator))
      offset -= buffer_iterator->page->data_vec.len;
    else
      offset = 0;
  }

  struct pb_page *page1;
  struct pb_page *page2;

  if (offset != 0) {
    page1 = buffer_iterator->page;
    page2 =
      pb_page_transfer(
        page1, page1->data_vec.len, 0,
        buffer->allocator);
    if (!page2)
      return 0;

    page2->data_vec.base += offset;
    page2->data_vec.len -= offset;
    page2->prev = page1;
    page2->next = page1->next;

    page1->data_vec.len = offset;
    page1->next->prev = page2;
    page1->next = page2;
  } else {
    page1 = buffer_iterator->page->prev;
    page2 = buffer_iterator->page;
  }

  page->prev = page1;
  page->next = page2;

  page1->next = page;
  page2->prev = page;

  pb_trivial_buffer_increment_data_size(buffer, page->data_vec.len);

  return page->data_vec.len;
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_seek(struct pb_buffer * const buffer, uint64_t len) {
  const struct pb_allocator *allocator = buffer->allocator;

  pb_trivial_buffer_increment_data_revision(buffer);

  uint64_t seeked = 0;

  struct pb_buffer_iterator itr;
  pb_buffer_get_iterator(buffer, &itr);

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(buffer, &itr))) {
    uint64_t seek_len =
      (itr.page->data_vec.len < len) ?
       itr.page->data_vec.len : len;

    itr.page->data_vec.base += seek_len;
    itr.page->data_vec.len -= seek_len;

    if (itr.page->data_vec.len == 0) {
      struct pb_page *page = itr.page;

      pb_buffer_iterator_next(buffer, &itr);

      page->prev->next = itr.page;
      itr.page->prev = page->prev;

      page->prev = NULL;
      page->next = NULL;

      pb_page_destroy(page, allocator);
    }

    len -= seek_len;
    seeked += seek_len;

    pb_trivial_buffer_decrement_data_size(buffer, seek_len);
  }

  return seeked;
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_reserve(struct pb_buffer * const buffer,
    uint64_t len) {
  const struct pb_allocator *allocator = buffer->allocator;

  uint64_t reserved = 0;

  while (len > 0) {
    uint64_t reserve_len =
      (buffer->strategy.page_size != 0) ?
      (buffer->strategy.page_size < len) ?
       buffer->strategy.page_size : len :
       len;

    void *buf =
      allocator->alloc(allocator, pb_alloc_type_region, reserve_len);
    if (!buf)
      return reserved;

    struct pb_data *data = pb_data_create(buf, reserve_len, allocator);
    if (!data) {
      allocator->free(allocator, pb_alloc_type_region, buf, reserve_len);

      return reserved;
    }

    struct pb_page *page = pb_page_create(data, allocator);
    if (!page) {
      pb_data_put(data);

      return reserved;
    }

    pb_data_put(data);

    struct pb_buffer_iterator end_itr;
    pb_buffer_get_iterator_end(buffer, &end_itr);

    reserve_len = pb_buffer_insert(buffer, page, &end_itr, 0);
    if (reserve_len == 0)
      break;

    len -= reserve_len;
    reserved += reserve_len;
  }

  return reserved;
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_rewind(struct pb_buffer * const buffer,
    uint64_t len) {
  const struct pb_allocator *allocator = buffer->allocator;

  pb_trivial_buffer_increment_data_revision(buffer);

  uint64_t rewinded = 0;

  while (len > 0) {
    uint64_t reserve_len =
      (buffer->strategy.page_size != 0) ?
      (buffer->strategy.page_size < len) ?
       buffer->strategy.page_size : len :
       len;

    void *buf =
      allocator->alloc(allocator, pb_alloc_type_region, reserve_len);
    if (!buf)
      return rewinded;

    struct pb_data *data = pb_data_create(buf, reserve_len, allocator);
    if (!data) {
      allocator->free(allocator, pb_alloc_type_region, buf, reserve_len);

      return rewinded;
    }

    struct pb_page *page = pb_page_create(data, allocator);
    if (!page) {
      pb_data_put(data);

      return rewinded;
    }

    pb_data_put(data);

    struct pb_buffer_iterator itr;
    pb_buffer_get_iterator(buffer, &itr);

    reserve_len = pb_buffer_insert(buffer, page, &itr, 0);
    if (reserve_len == 0)
      break;

    len -= reserve_len;
    rewinded += reserve_len;
  }

  return rewinded;
}

/*******************************************************************************
 */
void pb_trivial_buffer_get_iterator(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const iterator) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  iterator->page = trivial_buffer->page_end.next;
}

void pb_trivial_buffer_get_iterator_end(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const iterator) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  iterator->page = &trivial_buffer->page_end;
}

bool pb_trivial_buffer_iterator_is_end(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const iterator) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  return (iterator->page == &trivial_buffer->page_end);
}

bool pb_trivial_buffer_iterator_cmp(struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *lvalue,
    const struct pb_buffer_iterator *rvalue) {

  return (lvalue->page == rvalue->page);
}

void pb_trivial_buffer_iterator_next(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const iterator) {
  iterator->page = iterator->page->next;
}

void pb_trivial_buffer_iterator_prev(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const iterator) {
  iterator->page = iterator->page->prev;
}

/*******************************************************************************
 */
void pb_trivial_buffer_get_byte_iterator(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const iterator) {
  pb_buffer_get_iterator(buffer, &iterator->buffer_iterator);

  iterator->page_offset = 0;

  if (pb_buffer_iterator_is_end(buffer, &iterator->buffer_iterator)) {
    iterator->current_byte = NULL;

    return;
  }

  iterator->current_byte =
    (char*)(
      &iterator->buffer_iterator.page->data_vec.base[iterator->page_offset]);
}

void pb_trivial_buffer_get_byte_iterator_end(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const iterator) {
  pb_buffer_get_iterator_end(buffer, &iterator->buffer_iterator);

  iterator->page_offset = 0;

  iterator->current_byte = NULL;
}

bool pb_trivial_buffer_byte_iterator_is_end(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const iterator) {
  return pb_buffer_iterator_is_end(buffer, &iterator->buffer_iterator);
}

bool pb_trivial_buffer_byte_iterator_cmp(struct pb_buffer * const buffer,
    const struct pb_buffer_byte_iterator *lvalue,
    const struct pb_buffer_byte_iterator *rvalue) {
  if (!pb_buffer_iterator_cmp(buffer,
        &lvalue->buffer_iterator, &rvalue->buffer_iterator))
    return false;

  return (lvalue->page_offset == rvalue->page_offset);
}

void pb_trivial_buffer_byte_iterator_next(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const iterator) {
  ++iterator->page_offset;

  if (iterator->page_offset >= iterator->buffer_iterator.page->data_vec.len) {
    pb_buffer_iterator_next(buffer, &iterator->buffer_iterator);

    iterator->page_offset = 0;
  }

  if (pb_buffer_iterator_is_end(buffer, &iterator->buffer_iterator)) {
    iterator->current_byte = NULL;

    return;
  }

  iterator->current_byte =
    (char*)(
      &iterator->buffer_iterator.page->data_vec.base[iterator->page_offset]);
}

void pb_trivial_buffer_byte_iterator_prev(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const iterator) {
  if (iterator->page_offset == 0) {
    pb_buffer_iterator_prev(buffer, &iterator->buffer_iterator);

    iterator->page_offset = iterator->buffer_iterator.page->data_vec.len;
  }

  --iterator->page_offset;

  if (pb_buffer_iterator_is_end(buffer, &iterator->buffer_iterator)) {
    iterator->current_byte = NULL;

    return;
  }

  iterator->current_byte =
    (char*)(
      &iterator->buffer_iterator.page->data_vec.base[iterator->page_offset]);
}

/*******************************************************************************
 * fragment_as_target: false
 */
static
#ifdef NDEBUG
inline
#endif
uint64_t pb_trivial_buffer_write_data1(struct pb_buffer * const buffer,
    const uint8_t *buf,
    uint64_t len) {
  if (pb_buffer_get_data_size(buffer) == 0)
    pb_trivial_buffer_increment_data_revision(buffer);

  struct pb_buffer_iterator itr;
  pb_buffer_get_iterator_end(buffer, &itr);
  pb_buffer_iterator_prev(buffer, &itr);

  uint64_t written = 0;

  while (len > 0) {
    uint64_t write_len =
      (buffer->strategy.page_size != 0) ?
      (buffer->strategy.page_size < len) ?
       buffer->strategy.page_size : len :
       len;

    write_len = pb_buffer_reserve(buffer, write_len);

    if (!pb_buffer_iterator_is_end(buffer, &itr))
      pb_buffer_iterator_next(buffer, &itr);
    else
      pb_buffer_iterator_prev(buffer, &itr);

    memcpy(itr.page->data_vec.base, buf + written, write_len);

    len -= write_len;
    written += write_len;

    pb_buffer_iterator_next(buffer, &itr);
  }

  return written;
}

/*
 * fragment_as_target: true
 */
static
#ifdef NDEBUG
inline
#endif
uint64_t pb_trivial_buffer_write_data2(struct pb_buffer * const buffer,
    const uint8_t *buf,
    uint64_t len) {
  if (pb_buffer_get_data_size(buffer) == 0)
    pb_trivial_buffer_increment_data_revision(buffer);

  struct pb_buffer_iterator itr;
  pb_buffer_get_iterator_end(buffer, &itr);
  pb_buffer_iterator_prev(buffer, &itr);

  len = pb_buffer_reserve(buffer, len);

  if (!pb_buffer_iterator_is_end(buffer, &itr))
    pb_buffer_iterator_next(buffer, &itr);
  else
    pb_buffer_get_iterator(buffer, &itr);

  uint64_t written = 0;

  while (len > 0) {
    uint64_t write_len =
      (itr.page->data_vec.len < len) ?
       itr.page->data_vec.len : len;

    memcpy(itr.page->data_vec.base, buf + written, write_len);

    len -= write_len;
    written += write_len;

    pb_buffer_iterator_next(buffer, &itr);
  }

  return written;
}

uint64_t pb_trivial_buffer_write_data(struct pb_buffer * const buffer,
    const uint8_t *buf,
    uint64_t len) {
  if (!buffer->strategy.fragment_as_target)
    return pb_trivial_buffer_write_data1(buffer, buf, len);

  return pb_trivial_buffer_write_data2(buffer, buf, len);
}

/*******************************************************************************
 * clone_on_write: false
 * fragment_as_target: false
 */
static
#ifdef NDEBUG
inline
#endif
uint64_t pb_trivial_buffer_write_data_ref1(struct pb_buffer * const buffer,
    const uint8_t *buf,
    uint64_t len) {
  const struct pb_allocator *allocator = buffer->allocator;

  struct pb_data *data = pb_data_create_ref(buf, len, allocator);
  if (!data)
    return 0;

  struct pb_page *page = pb_page_create(data, allocator);
  if (!page) {
    pb_data_put(data);

    return 0;
  }

  pb_data_put(data);

  struct pb_buffer_iterator end_itr;
  pb_buffer_get_iterator_end(buffer, &end_itr);

  return pb_buffer_insert(buffer, page, &end_itr, 0);
}

/*
 * clone_on_write: false
 * fragment_as_target: true
 */
static
#ifdef NDEBUG
inline
#endif
uint64_t pb_trivial_buffer_write_data_ref2(struct pb_buffer * const buffer,
    const uint8_t *buf,
    uint64_t len) {
  const struct pb_allocator *allocator = buffer->allocator;

  uint64_t written = 0;

  while (len > 0) {
    uint64_t write_len =
      (buffer->strategy.page_size != 0) ?
      (buffer->strategy.page_size < len) ?
       buffer->strategy.page_size : len :
       len;

    struct pb_data *data =
      pb_data_create_ref(buf + written, write_len, allocator);
    if (!data)
      return written;

    struct pb_page *page = pb_page_create(data, allocator);
    if (!page) {
      pb_data_put(data);

      return written;
    }

    pb_data_put(data);

    struct pb_buffer_iterator end_itr;
    pb_buffer_get_iterator_end(buffer, &end_itr);

    write_len = pb_buffer_insert(buffer, page, &end_itr, 0);

    len -= write_len;
    written += write_len;
  }

  return written;
}

uint64_t pb_trivial_buffer_write_data_ref(struct pb_buffer * const buffer,
    const uint8_t *buf,
    uint64_t len) {
  if ((!buffer->strategy.clone_on_write) &&
      (!buffer->strategy.fragment_as_target)) {
    return pb_trivial_buffer_write_data_ref1(buffer, buf, len);
  } else if ((!buffer->strategy.clone_on_write) &&
             (buffer->strategy.fragment_as_target)) {
    return pb_trivial_buffer_write_data_ref2(buffer, buf, len);
  } else if ((buffer->strategy.clone_on_write) &&
             (!buffer->strategy.fragment_as_target)) {
    return pb_trivial_buffer_write_data1(buffer, buf, len);
  }
  /*else if ((buffer->strategy->clone_on_write) &&
             (buffer->strategy->fragment_as_target)) {*/
  return pb_trivial_buffer_write_data2(buffer, buf, len);
}

/*******************************************************************************
 * clone_on_write: false
 * fragment_as_target: false
 */
static
#ifdef NDEBUG
inline
#endif
uint64_t pb_trivial_buffer_write_buffer1(struct pb_buffer * const buffer,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  const struct pb_allocator *allocator = buffer->allocator;

  if (pb_buffer_get_data_size(buffer) == 0)
    pb_trivial_buffer_increment_data_revision(buffer);

  struct pb_buffer_iterator itr;
  pb_buffer_get_iterator_end(buffer, &itr);

  struct pb_buffer_iterator src_itr;
  pb_buffer_get_iterator(src_buffer, &src_itr);

  uint64_t written = 0;

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(src_buffer, &src_itr))) {
    uint64_t write_len =
      (src_itr.page->data_vec.len < len) ?
       src_itr.page->data_vec.len : len;

    struct pb_page *page =
      pb_page_transfer(src_itr.page, write_len, 0, allocator);
    if (!page)
      return written;

    write_len = pb_buffer_insert(buffer, page, &itr, 0);

    len -= write_len;
    written += write_len;

    pb_buffer_iterator_next(src_buffer, &src_itr);
  }

  return written;
}

/*
 * clone_on_write: false
 * fragment_as_target: true
 */
static
#ifdef NDEBUG
inline
#endif
uint64_t pb_trivial_buffer_write_buffer2(struct pb_buffer * const buffer,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  const struct pb_allocator *allocator = buffer->allocator;

  if (pb_buffer_get_data_size(buffer) == 0)
    pb_trivial_buffer_increment_data_revision(buffer);

  struct pb_buffer_iterator itr;
  pb_buffer_get_iterator_end(buffer, &itr);

  struct pb_buffer_iterator src_itr;
  pb_buffer_get_iterator(src_buffer, &src_itr);

  uint64_t written = 0;
  size_t src_offset = 0;

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(src_buffer, &src_itr))) {
    uint64_t write_len =
      (buffer->strategy.page_size != 0) ?
      (buffer->strategy.page_size < len) ?
       buffer->strategy.page_size : len :
       len;

    write_len =
      (src_itr.page->data_vec.len - src_offset < write_len) ?
       src_itr.page->data_vec.len - src_offset : write_len;

    struct pb_page *page =
      pb_page_transfer(src_itr.page, write_len, src_offset, allocator);
    if (!page)
      return written;

    write_len = pb_buffer_insert(buffer, page, &itr, 0);

    len -= write_len;
    written += write_len;
    src_offset += write_len;

    if (src_offset == src_itr.page->data_vec.len) {
      pb_buffer_iterator_next(src_buffer, &src_itr);

      src_offset = 0;
    }
  }

  return written;
}

/*
 * clone_on_write: true
 * fragment_as_target: false
 */
static
#ifdef NDEBUG
inline
#endif
uint64_t pb_trivial_buffer_write_buffer3(struct pb_buffer * const buffer,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  if (pb_buffer_get_data_size(buffer) == 0)
    pb_trivial_buffer_increment_data_revision(buffer);

  struct pb_buffer_iterator itr;
  pb_buffer_get_iterator_end(buffer, &itr);
  pb_buffer_iterator_prev(buffer, &itr);

  struct pb_buffer_iterator src_itr;
  pb_buffer_get_iterator(src_buffer, &src_itr);

  uint64_t written = 0;
  size_t src_offset = 0;

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(src_buffer, &src_itr))) {
    uint64_t write_len =
      (buffer->strategy.page_size != 0) ?
      (buffer->strategy.page_size < len) ?
       buffer->strategy.page_size : len :
       len;

    write_len =
      (src_itr.page->data_vec.len - src_offset < write_len) ?
       src_itr.page->data_vec.len - src_offset : write_len;

    write_len = pb_buffer_reserve(buffer, write_len);

    if (!pb_buffer_iterator_is_end(buffer, &itr))
      pb_buffer_iterator_next(buffer, &itr);
    else
      pb_buffer_get_iterator(buffer, &itr);

    memcpy(
      itr.page->data_vec.base,
      src_itr.page->data_vec.base + src_offset,
      write_len);

    len -= write_len;
    written += write_len;
    src_offset += write_len;

    if (src_offset == src_itr.page->data_vec.len) {
      pb_buffer_iterator_next(src_buffer, &src_itr);

      src_offset = 0;
    }
  }

  return written;
}

/*
 * clone_on_write: true
 * fragment_as_target: true
 */
static
#ifdef NDEBUG
inline
#endif
uint64_t pb_trivial_buffer_write_buffer4(struct pb_buffer * const buffer,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  if (pb_buffer_get_data_size(buffer) == 0)
    pb_trivial_buffer_increment_data_revision(buffer);

  struct pb_buffer_iterator itr;
  pb_buffer_get_iterator_end(buffer, &itr);
  pb_buffer_iterator_prev(buffer, &itr);

  struct pb_buffer_iterator src_itr;
  pb_buffer_get_iterator(src_buffer, &src_itr);

  len = pb_buffer_reserve(buffer, len);

  if (!pb_buffer_iterator_is_end(buffer, &itr))
    pb_buffer_iterator_next(buffer, &itr);
  else
    pb_buffer_get_iterator(buffer, &itr);

  uint64_t written = 0;
  size_t offset = 0;
  size_t src_offset = 0;

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(buffer, &itr)) &&
         (!pb_buffer_iterator_is_end(src_buffer, &src_itr))) {
    uint64_t write_len =
      (itr.page->data_vec.len - offset < len) ?
       itr.page->data_vec.len - offset: len;

    write_len =
      (src_itr.page->data_vec.len - src_offset < write_len) ?
       src_itr.page->data_vec.len - src_offset: write_len;

    memcpy(
      itr.page->data_vec.base + offset,
      src_itr.page->data_vec.base + src_offset,
      write_len);

    len -= write_len;
    written += write_len;
    offset += write_len;
    src_offset += write_len;

    if (offset == itr.page->data_vec.len) {
      pb_buffer_iterator_next(buffer, &itr);

      offset = 0;
    }

    if (src_offset == src_itr.page->data_vec.len) {
      pb_buffer_iterator_next(src_buffer, &src_itr);

      src_offset = 0;
    }
  }

  return written;
}

uint64_t pb_trivial_buffer_write_buffer(struct pb_buffer * const buffer,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  if ((!buffer->strategy.clone_on_write) &&
      (!buffer->strategy.fragment_as_target)) {
    return pb_trivial_buffer_write_buffer1(buffer, src_buffer, len);
  } else if ((!buffer->strategy.clone_on_write) &&
             (buffer->strategy.fragment_as_target)) {
    return pb_trivial_buffer_write_buffer2(buffer, src_buffer, len);
  } else if ((buffer->strategy.clone_on_write) &&
             (!buffer->strategy.fragment_as_target)) {
    return pb_trivial_buffer_write_buffer3(buffer, src_buffer, len);
  }
  /*else if ((buffer->strategy->clone_on_write) &&
             (buffer->strategy->fragment_as_target)) {*/
  return pb_trivial_buffer_write_buffer4(buffer, src_buffer, len);
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_overwrite_data(struct pb_buffer * const buffer,
    const uint8_t *buf,
    uint64_t len) {
  pb_trivial_buffer_increment_data_revision(buffer);

  struct pb_buffer_iterator itr;
  pb_buffer_get_iterator(buffer, &itr);

  uint64_t written = 0;

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(buffer, &itr))) {
    uint64_t write_len =
      (itr.page->data_vec.len < len) ?
       itr.page->data_vec.len : len;

    memcpy(itr.page->data_vec.base, buf + written, write_len);

    len -= write_len;
    written += write_len;

    pb_buffer_iterator_next(buffer, &itr);
  }

  return written;
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_read_data(struct pb_buffer * const buffer,
    uint8_t * const buf,
    uint64_t len) {
  struct pb_buffer_iterator itr;
  pb_buffer_get_iterator(buffer, &itr);

  uint64_t readed = 0;

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(buffer, &itr))) {
    size_t read_len =
      (itr.page->data_vec.len < len) ?
       itr.page->data_vec.len : len;

    memcpy(buf + readed, itr.page->data_vec.base, read_len);\

    len -= read_len;
    readed += read_len;

    pb_buffer_iterator_next(buffer, &itr);
  }

  return readed;
}

/*******************************************************************************
 */
void pb_trivial_buffer_clear(struct pb_buffer * const buffer) {
  uint64_t data_size = pb_buffer_get_data_size(buffer);

  pb_buffer_seek(buffer, data_size);
}

/*******************************************************************************
 */
void pb_trivial_buffer_destroy(struct pb_buffer * const buffer) {
  pb_buffer_clear(buffer);

  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;
  const struct pb_allocator *allocator = buffer->allocator;

  allocator->free(
    allocator,
    pb_alloc_type_struct, trivial_buffer, sizeof(struct pb_trivial_buffer));
}






/*******************************************************************************
 */
struct pb_data_reader_operations pb_trivial_data_reader_operations = {
  .read = pb_trivial_data_reader_read,
  .reset = pb_trivial_data_reader_reset,
  .destroy = pb_trivial_data_reader_destroy,
};

const struct pb_data_reader_operations *pb_get_trivial_data_reader_operations(void) {
  return &pb_trivial_data_reader_operations;
}



/*******************************************************************************
 */
struct pb_trivial_data_reader {
  struct pb_data_reader data_reader;

  struct pb_buffer_iterator iterator;

  uint64_t buffer_data_revision;

  uint64_t page_offset;
};



/*******************************************************************************
 */
uint64_t pb_data_reader_read(struct pb_data_reader * const data_reader,
    uint8_t * const buf, uint64_t len) {
  return data_reader->operations->read(data_reader, buf, len);
}

void pb_data_reader_reset(struct pb_data_reader * const data_reader) {
  data_reader->operations->reset(data_reader);
}

void pb_data_reader_destroy(struct pb_data_reader * const data_reader) {
  data_reader->operations->destroy(data_reader);
}




/*******************************************************************************
 */
struct pb_data_reader *pb_trivial_data_reader_create(
    struct pb_buffer * const buffer) {
  const struct pb_allocator *allocator = buffer->allocator;

  struct pb_trivial_data_reader *trivial_data_reader =
    allocator->alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_trivial_data_reader));
  if (!trivial_data_reader)
    return NULL;

  trivial_data_reader->data_reader.operations =
    pb_get_trivial_data_reader_operations();

  trivial_data_reader->data_reader.buffer = buffer;

  pb_data_reader_reset(&trivial_data_reader->data_reader);

  return &trivial_data_reader->data_reader;
}

/*******************************************************************************
 */
uint64_t pb_trivial_data_reader_read(
    struct pb_data_reader * const data_reader,
    uint8_t * const buf,
    uint64_t len) {
  struct pb_trivial_data_reader *trivial_data_reader =
    (struct pb_trivial_data_reader*)data_reader;
  struct pb_buffer *buffer = trivial_data_reader->data_reader.buffer;
  struct pb_buffer_iterator *itr = &trivial_data_reader->iterator;

  if (pb_buffer_get_data_revision(buffer) !=
      trivial_data_reader->buffer_data_revision)
    pb_data_reader_reset(&trivial_data_reader->data_reader);

  if (trivial_data_reader->page_offset == itr->page->data_vec.len)
    pb_buffer_iterator_next(buffer, itr);

  uint64_t readed = 0;

  while ((len > 0) &&
         (!buffer->operations->iterator_is_end(buffer, itr))) {
    uint64_t read_len =
      (itr->page->data_vec.len < len) ?
       itr->page->data_vec.len : len;

    memcpy(
      buf + trivial_data_reader->page_offset + readed,
      itr->page->data_vec.base,
      read_len);

    len -= read_len;
    readed += read_len;

    trivial_data_reader->page_offset += read_len;

    if (trivial_data_reader->page_offset != itr->page->data_vec.len)
      return readed;

    buffer->operations->iterator_next(buffer, itr);

    trivial_data_reader->page_offset = 0;
  }

  if (buffer->operations->iterator_is_end(buffer, itr)) {
    buffer->operations->iterator_prev(buffer, itr);

    trivial_data_reader->page_offset = itr->page->data_vec.len;
  }

  return readed;
}

/*******************************************************************************
 */
void pb_trivial_data_reader_reset(struct pb_data_reader * const data_reader) {
  struct pb_trivial_data_reader *trivial_data_reader =
    (struct pb_trivial_data_reader*)data_reader;
  struct pb_buffer *buffer = trivial_data_reader->data_reader.buffer;

  pb_buffer_get_iterator(buffer, &trivial_data_reader->iterator);

  trivial_data_reader->buffer_data_revision =
    buffer->operations->get_data_revision(buffer);

  trivial_data_reader->page_offset = 0;
}

void pb_trivial_data_reader_destroy(struct pb_data_reader * const data_reader) {
  struct pb_trivial_data_reader *trivial_data_reader =
    (struct pb_trivial_data_reader*)data_reader;
  const struct pb_allocator *allocator =
    trivial_data_reader->data_reader.buffer->allocator;

  allocator->free(
    allocator,
    pb_alloc_type_struct,
    trivial_data_reader, sizeof(struct pb_trivial_data_reader));
}






/*******************************************************************************
 */
static struct pb_line_reader_operations pb_trivial_line_reader_operations = {
  .has_line = &pb_trivial_line_reader_has_line,

  .get_line_len = &pb_trivial_line_reader_get_line_len,
  .get_line_data = &pb_trivial_line_reader_get_line_data,

  .seek_line = &pb_trivial_line_reader_seek_line,

  .is_crlf = &pb_trivial_line_reader_is_crlf,
  .is_end = &pb_trivial_line_reader_is_end,

  .terminate_line = &pb_trivial_line_reader_terminate_line,
  .terminate_line_check_cr = &pb_trivial_line_reader_terminate_line_check_cr,

  .clone = &pb_trivial_line_reader_clone,

  .reset = &pb_trivial_line_reader_reset,
  .destroy = &pb_trivial_line_reader_destroy,
};

const struct pb_line_reader_operations *pb_get_trivial_line_reader_operations() {
  return &pb_trivial_line_reader_operations;
}



/*******************************************************************************
 */
struct pb_trivial_line_reader {
  struct pb_line_reader line_reader;

  struct pb_buffer_byte_iterator iterator;

  uint64_t buffer_data_revision;

  size_t buffer_offset;

  bool has_line;
  bool has_cr;
  bool is_terminated;
  bool is_terminated_with_cr;
};



/*******************************************************************************
 */
bool pb_line_reader_has_line(struct pb_line_reader * const line_reader) {
  return line_reader->operations->has_line(line_reader);
}

size_t pb_line_reader_get_line_len(struct pb_line_reader * const line_reader) {
  return line_reader->operations->get_line_len(line_reader);
}

size_t pb_line_reader_get_line_data(struct pb_line_reader * const line_reader,
    uint8_t * const buf, uint64_t len) {
  return line_reader->operations->get_line_data(line_reader, buf, len);
}

bool pb_line_reader_is_crlf(struct pb_line_reader * const line_reader) {
  return line_reader->operations->is_crlf(line_reader);
}

bool pb_line_reader_is_end(struct pb_line_reader * const line_reader) {
  return line_reader->operations->is_end(line_reader);
}

void pb_line_reader_terminate_line(struct pb_line_reader * const line_reader) {
  line_reader->operations->terminate_line(line_reader);
}

void pb_line_reader_terminate_line_check_cr(
    struct pb_line_reader * const line_reader) {
  line_reader->operations->terminate_line_check_cr(line_reader);
}

uint64_t pb_line_reader_seek_line(struct pb_line_reader * const line_reader) {
  return line_reader->operations->seek_line(line_reader);
}

struct pb_line_reader *pb_line_reader_clone(
    struct pb_line_reader * const line_reader) {
  return line_reader->operations->clone(line_reader);
}

void pb_line_reader_reset(struct pb_line_reader * const line_reader) {
  line_reader->operations->reset(line_reader);
}

void pb_line_reader_destroy(struct pb_line_reader * const line_reader) {
  line_reader->operations->destroy(line_reader);
}



/*******************************************************************************
 */
struct pb_line_reader *pb_trivial_line_reader_create(
    struct pb_buffer * const buffer) {
  const struct pb_allocator *allocator = buffer->allocator;

  struct pb_trivial_line_reader *trivial_line_reader =
    allocator->alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_trivial_line_reader));
  if (!trivial_line_reader)
    return NULL;

  trivial_line_reader->line_reader.operations =
    pb_get_trivial_line_reader_operations();

  trivial_line_reader->line_reader.buffer = buffer;

  pb_line_reader_reset(&trivial_line_reader->line_reader);

  return &trivial_line_reader->line_reader;
}

/*******************************************************************************
 */
bool pb_trivial_line_reader_has_line(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = line_reader->buffer;
  struct pb_buffer_byte_iterator *iterator = &trivial_line_reader->iterator;

  if (trivial_line_reader->buffer_data_revision !=
        pb_buffer_get_data_revision(buffer))
    pb_line_reader_reset(line_reader);

  if (trivial_line_reader->has_line)
    return true;

  if (pb_buffer_get_data_size(buffer) == 0)
    return false;

  while (!pb_buffer_byte_iterator_is_end(buffer, iterator)) {
    if (*iterator->current_byte == '\n')
      return (trivial_line_reader->has_line = true);
    else if (*iterator->current_byte == '\r')
      trivial_line_reader->has_cr = true;
    else
      trivial_line_reader->has_cr = false;

    if (trivial_line_reader->buffer_offset == PB_LINE_READER_DEFAULT_LINE_MAX) {
      trivial_line_reader->has_cr = false;

      return (trivial_line_reader->has_line = true);
    }

    pb_buffer_byte_iterator_next(buffer, iterator);

    ++trivial_line_reader->buffer_offset;
  }

  // reset back to last position
  pb_buffer_byte_iterator_prev(buffer, iterator);

  --trivial_line_reader->buffer_offset;

  if (trivial_line_reader->is_terminated_with_cr)
    return (trivial_line_reader->has_line = true);

  if (trivial_line_reader->is_terminated) {
    trivial_line_reader->has_cr = false;

    return (trivial_line_reader->has_line = true);
  }

  return false;
}

/*******************************************************************************
 */
size_t pb_trivial_line_reader_get_line_len(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = line_reader->buffer;

  if (trivial_line_reader->buffer_data_revision !=
        pb_buffer_get_data_revision(buffer))
    pb_line_reader_reset(line_reader);

  if (!trivial_line_reader->has_line)
    return 0;

  if (trivial_line_reader->has_cr)
    return (trivial_line_reader->buffer_offset - 1);

  return trivial_line_reader->buffer_offset;
}

/*******************************************************************************
 */
size_t pb_trivial_line_reader_get_line_data(
    struct pb_line_reader * const line_reader,
    uint8_t * const buf, uint64_t len) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = line_reader->buffer;

  if (trivial_line_reader->buffer_data_revision !=
        pb_buffer_get_data_revision(buffer))
    pb_line_reader_reset(line_reader);

  if (!trivial_line_reader->has_line)
    return 0;

  size_t getted = 0;

  uint64_t line_len = pb_line_reader_get_line_len(line_reader);

  struct pb_buffer_iterator iterator;
  pb_buffer_get_iterator(buffer, &iterator);

  while ((len > 0) &&
         (line_len > 0) &&
         (!pb_buffer_iterator_is_end(buffer, &iterator))) {
    size_t to_get =
      (iterator.page->data_vec.len < len) ?
          iterator.page->data_vec.len : len;

    to_get =
      (to_get < line_len) ?
       to_get : line_len;

    memcpy(buf + getted, iterator.page->data_vec.base, to_get);

    len -= to_get;
    line_len -= to_get;
    getted += to_get;

    pb_buffer_iterator_next(buffer, &iterator);
  }

  return getted;
}

/*******************************************************************************
 */
uint64_t pb_trivial_line_reader_seek_line(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = line_reader->buffer;

  if (trivial_line_reader->buffer_data_revision !=
        pb_buffer_get_data_revision(buffer))
    pb_line_reader_reset(line_reader);

  if (!trivial_line_reader->has_line)
    return 0;

  uint64_t to_seek =
    (trivial_line_reader->is_terminated) ?
      trivial_line_reader->buffer_offset :
      trivial_line_reader->buffer_offset + 1;

  to_seek = pb_buffer_seek(buffer, to_seek);

  pb_line_reader_reset(line_reader);

  return to_seek;
}

/*******************************************************************************
 */
bool pb_trivial_line_reader_is_crlf(struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;

  return trivial_line_reader->has_cr;
}

bool pb_trivial_line_reader_is_end(struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = trivial_line_reader->line_reader.buffer;

  return
    pb_buffer_byte_iterator_is_end(buffer, &trivial_line_reader->iterator);
}

/*******************************************************************************
 */
void pb_trivial_line_reader_terminate_line(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;

  trivial_line_reader->is_terminated = true;
}

void pb_trivial_line_reader_terminate_line_check_cr(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;

  trivial_line_reader->is_terminated_with_cr = true;
}

/*******************************************************************************
 */
struct pb_line_reader *pb_trivial_line_reader_clone(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  const struct pb_allocator *allocator =
    line_reader->buffer->allocator;

  struct pb_trivial_line_reader *trivial_line_reader_clone =
    allocator->alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_line_reader));
  if (!trivial_line_reader_clone)
    return NULL;

  memcpy(
    trivial_line_reader_clone,
    trivial_line_reader,
    sizeof(struct pb_line_reader));

  return &trivial_line_reader_clone->line_reader;
}

/*******************************************************************************
 */
void pb_trivial_line_reader_reset(struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = trivial_line_reader->line_reader.buffer;

  pb_buffer_get_byte_iterator(buffer, &trivial_line_reader->iterator);

  trivial_line_reader->buffer_data_revision =
    pb_buffer_get_data_revision(buffer);

  trivial_line_reader->buffer_offset = 0;

  trivial_line_reader->has_line = false;
  trivial_line_reader->has_cr = false;
  trivial_line_reader->is_terminated = false;
  trivial_line_reader->is_terminated_with_cr = false;
}

/*******************************************************************************
 */
void pb_trivial_line_reader_destroy(struct pb_line_reader * const line_reader) {
  const struct pb_allocator *allocator =
    line_reader->buffer->allocator;

  pb_line_reader_reset(line_reader);

  allocator->free(
    allocator,
    pb_alloc_type_struct,
    line_reader, sizeof(struct pb_trivial_line_reader));
}
