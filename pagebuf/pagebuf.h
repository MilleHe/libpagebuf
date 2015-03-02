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

#ifndef PAGEBUF_H
#define PAGEBUF_H


#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif



/** Indicates how the allocated memory region will be used. */
enum pb_allocator_type {
  pb_alloc_type_struct =                                  1,
  pb_alloc_type_region =                                  2,
};

/** Wrapper for allocation and freeing of data regions
 *
 * Allocate data memory regions through these interfaces.
 *
 * Allocators must zero memory regions designated for use in structs.
 *
 * These functions receive a reference to their parent as a parameter so that
 * implementations may use the pointer to hold additional information.
 */
struct pb_allocator {
  /** Allocate a memory region.
   *
   * type indicates whether the memory allocated will be used for a data
   * structure, or as a memory region.
   *
   * size indicates the size of the memory region to allocate.
   */
  void *(*alloc)(enum pb_allocator_type type, size_t size,
                 const struct pb_allocator *allocator);
  /** Free a memory region.
   *
   * type indicates whether the memory allocated was used for a data
   * structure, or as a memory region.
   *
   * obj is the address of beginning of the memory region.
   *
   * size indicates the size of the memory region that was allocated.
   */
  void  (*free)(enum pb_allocator_type type, void *obj, size_t size,
                const struct pb_allocator *allocator);
};

/** Get a built in, trivial heap based allocator. */
const struct pb_allocator *pb_get_trivial_allocator(void);



/** A structure used to represent a data region.
 */
struct pb_data_vec {
  /** The starting address of the region. */
  uint8_t *base;
  /** The length of the region. */
  size_t len;
};



/** Responsibility that the pb_data instance has over the data region.
 *
 * Either owned, thus freed when use count reaches zero,
 * or merely referenced.
 */
enum pb_data_responsibility {
  pb_data_none =                                          0,
  pb_data_owned,
  pb_data_referenced,
};

/** Reference counted structure that represents a memory region.
 *
 * Instances of this object are created using the create routines below, but
 * it should rarely be explicitly destroyed.  Instead the get and put
 * functions should be used when accepting or releasing an instance.
 * When a call to put finds a zero use count it will internally call destroy.
 */
struct pb_data {
  /** The bounds of the region. */
  struct pb_data_vec data_vec;

  /** Use count, maintained with atomic operations. */
  uint16_t use_count;

  /** Responsibility that this structure has over the memory region. */
  enum pb_data_responsibility responsibility;

  /** Allocator used to allocate storage for this struct and its owned
   * memory region */
  const struct pb_allocator *allocator;
};



/** Create a pb_data instance.
 *
 * Memory region buf is now owned by the pb_data instance and will be freed
 * when the instance is destroyed.
 *
 * Use count is initialised to one, therefore returned data instance must
 * be treated with the pb_data_put function when the creator is finished with
 * it, whether it is passed to another owner or not.
 */
struct pb_data *pb_data_create(uint8_t * const buf, size_t len,
                               const struct pb_allocator *allocator);

/** Create a pb_data instance.
 *
 * Memory region buf is not owned by the pb_data instance.
 *
 * Use count is initialised to one, therefore returned data instance must
 * be treated with the pb_data_put function when the creator is finished with
 * it, whether it is passed to another owner or not.
 */
struct pb_data *pb_data_create_ref(const uint8_t *buf, size_t len,
                                   const struct pb_allocator * allocator);

/** Clone a pb_data instance.
 *
 * A new memory region is allocated and the source memory region is copied
 * byte by byte.
 */
struct pb_data *pb_data_clone(uint8_t * const buf, size_t len, size_t src_off,
                              const struct pb_data *src_data,
                              const struct pb_allocator *allocator);

/** Destroy a pb_data instance.
 *
 * Not to be called directly unless the instance was never 'get'd by a
 * container.
 *
 * The memory region will be freed if it is owned by the instance.
 * The pb_data instance will be freed.
 */
void pb_data_destroy(struct pb_data *data);

/** Increment the use count of the pb_data instance.
 */
void pb_data_get(struct pb_data *data);

/** Decrement the use count of the pb_data instance.
 *
 *  Will destroy the instance if the use count becomes zero.
 */
void pb_data_put(struct pb_data *data);



/** Reference to a pb_data instance and a bounded subset of its memory region.
 *
 * Embodies the consumption of the memory region referenced by the
 * pb_data instance.
 * Multiple pb_page instances may refer to one pb_data instance, each
 * increasing the usage count.
 *
 * Because a page stays always within the boundary of its owner list, it
 * needn't carry an allocator with it.
 */
struct pb_page {
  /** The offset into the referenced region, and the length of the reference */
  struct pb_data_vec data_vec;

  /** The reference to the pb_data instance. */
  struct pb_data *data;

  /** Previous page in a list structure. */
  struct pb_page *prev;
  /** Next page in a list structure */
  struct pb_page *next;
};

/** Create a pb_page instance.
 *
 * pb_data instance has its use count incremented.
 */
struct pb_page *pb_page_create(struct pb_data *data,
                               const struct pb_allocator *allocator);

/** Transfer pb_page data.
 *
 * Duplicate the base and len values of the source page, and reference its
 * pb_data instance.
 */
struct pb_page *pb_page_transfer(const struct pb_page *src_page,
                                 size_t len, size_t src_off,
                                 const struct pb_allocator *allocator);

#if 0
/** Clone an existing pb_page instance
 *
 * Duplicate the base and len values as well as a full clone of the referenced
 * pb_data instance.
 */
struct pb_page *pb_page_clone(const struct pb_page *src_page,
                              uint8_t * const buf, size_t len, size_t src_off,
                              const struct pb_allocator *allocator);
#endif

/** Destroy the pb_page instance.
 *
 * pb_data instance will be released.
 * pb_page instance will be cleared and freed.
 */
void pb_page_destroy(struct pb_page *page,
                     const struct pb_allocator *allocator);



/** A structure used to iterate over list data.
 */
struct pb_list_iterator {
  struct pb_page *page;
};



/** A structure that describes the internal strategy of a pb_list.
 *
 * A list strategy controls the way that a list will carry out actions such
 * as writing of data, or management of memory regions.
 */
struct pb_list_strategy {
  /** The size of data regions that the pb_list will internally dynamically
   *  allocate.
   *
   * If this value is zero, there will be no limit on fragment size,
   * so write operations will cause allocations of data regions equal in size
   * to the length of the source data. */
  size_t page_size;

  /** Indicates whether data written to the list from another list is:
   *
   * not_cloned (false): reference to the pb_data instance is incremented.
   *
   * cloned (true): new pb_data instance created and memory regions copied.
   */
  bool clone_on_write;

  /** Indicates how data written to the pb_list will be fragmented.
   *
   * as source (false): source data fragmentation takes precedence.
   *                    When clone_on_write is false, source pages are
   *                    moved to the target as is and pb_data references are
   *                    incremented.  Source pages are only fragmented by
   *                    truncation to fit the operation length value.
   *                    When clone_on_write is true, source pages are
   *                    fragmented according to the lower of the source
   *                    fragment size, and the target page_size, which may be
   *                    zero, in which case the source fragment size is used.
   * as target (true):  target pb_list page_size takes precedence.
   *                    When clone_on_write is false, source fragments that
   *                    are greater than the target page_size limit are split.
   *                    When clone_on_write is true, source fragments will be
   *                    packed into target fragments up to the target page_size
   *                    in size.
   */
  bool fragment_as_target;

  /** Indicates whether the pb_list supports insertion into the middle.
   *
   * supports (false): Insertion into the middle of a pb_list is supported.
   * rejects (true):   Insertion into the middle of a pb_list is not supported.
   */
  bool no_insertion;
};

/** Get a built in, trivial list strategy.
 *
 * page_size is 4096
 *
 * clone_on_write is false
 *
 * fragment_as_source is false
 *
 * no_insertion is false;
 */
const struct pb_list_strategy *pb_get_trivial_list_strategy(void);



/** Represents a list of pages, and operations for the manipulation of the
 * pages and the buffers of data therein.
 *
 * The list also represents the strategy for allocation and freeing of
 * data buffers.
 */
struct pb_list {
  /** Return the amount of data in the list, in bytes. */
  uint64_t (*get_data_size)(struct pb_list * const list);

  /** Return a revision stamp of the data.
   *
   * This value is incremented by pb_list implementations whenever the
   * pb_list is:
   * - seek'd
   * - rewind'd
   * - overwritten
   * - cleared
   *
   * External users such as line readers can retain and compare this value
   * to determine whether the pb_list instance has been invalidated since
   * last use, and therefore needs to be re-processed.
   */
  uint64_t (*get_data_revision)(struct pb_list * const list);

  /** Append a pb_page instance to the pb_list.
   *
   * list_iterator is the position in the list to update the pb_list.  The
   *               pb_page will be inserted in front of the iterator position.
   * offset is the offset within the iterator page to insert the new pb_page
   *        into.
   * page is the page to insert.  The data base and len values must be set
   *      before this operation is called.
   *
   * If the offset is zero, the new pb_page will be inserted before the
   * iterator page.  If the offset is non zero, then the iterator page may be
   * split according to the pb_list instances internal implementation.
   * If the offset is greater than or equal to the iterator page len, then the
   * new pb_page will be inserted after the iterator page, or at the head of
   * the pb_list if the iterator is an end iterator.
   *
   * This operation will affect the data size of the pb_list instance.
   *
   * The return value is the amount of data actually inserted to the
   * pb_list instance.  Users of the insert operation must reflect the value
   * returned back to their own callers.
   */
  uint64_t (*insert)(struct pb_list * const list,
                     struct pb_list_iterator * const list_iterator,
                     size_t offset,
                     struct pb_page * const page);
  /** Increase the size of the list by adding len bytes of data to the end.
   *
   * len indicates the amount of data to add in bytes.
   *
   * Depending on the list implementation details, the total len may be split
   * across multiple pages.
   */
  uint64_t (*reserve)(struct pb_list * const list,
                      uint64_t len);
  /** Seek the list by len bytes.
   *
   * len indicates the amount of data to seek, in bytes.
   *
   * The seek operation may cause internal pages to be consumed depending on
   * the list implementation details.  These pb_pages will be destroyed during
   * the seek operation and their respective data 'put'd.
   */
  uint64_t (*seek)(struct pb_list * const list,
                   uint64_t len);
  /** Increases the size of the list by adding len bytes of data to the head.
   *
   * len indicates the amount of data to ad in bytes.
   *
   * Depending on the list implementation details, the total len may be split
   * across multiple pages.
   */
  uint64_t (*rewind)(struct pb_list * const list,
                     uint64_t len);

  /** Initialise an iterator to the start of the pb_list instance data.
   *
   * The iterator should be a pointer to an object on the stack that should
   * be manipulated only by the iterator methods of the same list instance.
   */
  void (*get_iterator)(struct pb_list * const list,
                       struct pb_list_iterator * const list_iterator);
  /** Initialise an iterator to the end of the pb_list instance data.
     *
     * The iterator should be a pointer to an object on the stack that should
     * be manipulated only by the iterator methods of the same list instance.
     */
  void (*get_iterator_end)(struct pb_list * const list,
                           struct pb_list_iterator * const list_iterator);

  /** Indicates whether an iterator has traversed to the end of a lists
   *  internal chain of pages.
   *
   * This function must always be called, and return false, before the data
   * vector of the pb_page of the iterator can be used.  The value of the
   * pb_page pointer is undefined when the iterator end function returns true.
   */
  bool (*is_iterator_end)(struct pb_list * const list,
                          struct pb_list_iterator * const list_iterator);
  /** Increments an iterator to the next pb_page in a list's internal chain. */
  void (*iterator_next)(struct pb_list * const list,
                        struct pb_list_iterator * const list_iterator);
  /** Decrements an iterator to the previous pb_page in a list's internal
   *  chain.
   *
   * It is valid to call this function on an iterator that is the end
   * iterator, according to is_iterator_end.  If this function is called on such
   * an iterator, the list implementation must correctly decrement back to the
   * position before end in this case. */
  void (*iterator_prev)(struct pb_list * const list,
                        struct pb_list_iterator * const list_iterator);

  /** Write data from a memory region to the pb_list instance.
   *
   * buf indicates the start of the source memory region.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.
   *
   * Data is appended to the tail of the list, allocating new storage if
   * necessary.
   */
  uint64_t (*write_data)(struct pb_list * const list,
                         const uint8_t *buf,
                         uint64_t len);
  /** Write data from a source pb_list instance to the pb_list instance.
   *
   * src_list is the list to write from.  This pb_list instance will not have
   *          it's data modified, but it is not const because iterator
   *          operations of this pb_list may cause internal state changes.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.  This list will not be altered by
   * this operation.
   *
   * Data is appended to the tail of the list, allocating new storage if
   * necessary.
   */
  uint64_t (*write_list)(struct pb_list * const list,
                         struct pb_list * const src_list,
                         uint64_t len);

  /** Write data from a memory region to the pb_list instance.
   *
   * buf indicates the start of the source memory region.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.
   *
   * Data is written from the head of the source pb_list.  No new storage will
   * be allocated if len is greater than the size of the target pb_list.
   */
  uint64_t (*overwrite_data)(struct pb_list * const list,
                             const uint8_t *buf,
                             uint64_t len);

  /** Clear all data in the list. */
  void (*clear)(struct pb_list * const list);

  /** Destroy a pb_list. */
  void (*destroy)(struct pb_list * const list);

  /** The strategy used by the pb_list instance. */
  const struct pb_list_strategy *strategy;

  const struct pb_allocator *allocator;
};

/** A trivial list implementation using builtin or supplied allocator.
 *
 * Default page size: 4096 bytes
 */
struct pb_list *pb_trivial_list_create(void);
struct pb_list *pb_trivial_list_create_with_alloc(
                                      const struct pb_allocator *allocator);
struct pb_list *pb_trivial_list_create_with_strategy(
                                      const struct pb_list_strategy *strategy);
struct pb_list *pb_trivial_list_create_with_strategy_with_alloc(
                                      const struct pb_list_strategy *strategy,
                                      const struct pb_allocator *allocator);

uint64_t pb_trivial_list_get_data_size(struct pb_list * const list);

uint64_t pb_trivial_list_get_data_revision(struct pb_list * const list);

uint64_t pb_trivial_list_insert(struct pb_list * const list,
                                struct pb_list_iterator * const list_iterator,
                                size_t offset,
                                struct pb_page * const page);
uint64_t pb_trivial_list_append(struct pb_list * const list,
                                struct pb_page * const page);
uint64_t pb_trivial_list_reserve(struct pb_list * const list,
                                 uint64_t len);
uint64_t pb_trivial_list_seek(struct pb_list * const list,
                              uint64_t len);
uint64_t pb_trivial_list_rewind(struct pb_list * const list,
                                uint64_t len);

void pb_trivial_list_get_iterator(struct pb_list * const list,
                                  struct pb_list_iterator * const iterator);
void pb_trivial_list_iterator_next(struct pb_list * const list,
                                   struct pb_list_iterator * const iterator);
void pb_trivial_list_iterator_prev(struct pb_list * const list,
                                   struct pb_list_iterator * const iterator);
bool pb_trivial_list_is_iterator_end(struct pb_list * const list,
                                     struct pb_list_iterator * const iterator);

uint64_t pb_trivial_list_write_data(struct pb_list * const list,
                                    const uint8_t *buf,
                                    uint64_t len);
uint64_t pb_trivial_list_write_list(struct pb_list * const list,
                                    struct pb_list * const src_list,
                                    uint64_t len);
uint64_t pb_trivial_list_overwrite_data(struct pb_list * const list,
                                        const uint8_t *buf,
                                        uint64_t len);

void pb_trivial_list_clear(struct pb_list * const list);
void pb_trivial_list_destroy(struct pb_list * const list);



/** An interface for reading data from a pb_list.
 *
 * Through this interface, a
 */
struct pb_data_reader {
  /** Read data from the pb_list instance, into a memory region.
   *
   * buf indicates the start of the target memory region.
   * len indicates the amount of data to read, in bytes.
   *
   * returns the amount of data read.
   *
   * Data is read from the head of the source pb_list.  The amount of data read
   * is the lower of the size of the source pb_list and the value of len.
   *
   * Following a data read, the data reader will retain the position of the
   * end of the read, thus, a subsequent call to read_list will continue from
   * where the last read left off.
   *
   * However, if the pb_list undergoes an operation that alters its data
   * revision, a subsequent call to read_list will read from the beginning
   */
  uint64_t (*read)(struct pb_data_reader * const data_reader,
                   uint8_t * const buf, uint64_t len);

  /** Reset the data reader back to the beginning of the pb_list instance. */
  void (*reset)(struct pb_data_reader * const data_reader);

  /** Destroy the pb_data_reader instance. */
  void (*destroy)(struct pb_data_reader * const data_reader);

  struct pb_list *list;

  const struct pb_allocator *allocator;
};

/** A trivial data reader implementation that reads data via iterators
 */
struct pb_data_reader *pb_trivial_data_reader_create(
                                          struct pb_list * const list);
struct pb_data_reader *pb_trivial_data_reader_create_with_alloc(
                                          struct pb_list * const list,
                                          const struct pb_allocator *allocator);

uint64_t pb_trivial_data_reader_read(struct pb_data_reader * const data_reader,
                                     uint8_t * const buf, uint64_t len);

void pb_trivial_data_reader_reset(struct pb_data_reader * const data_reader);

void pb_trivial_data_reader_destroy(struct pb_data_reader * const data_reader);



/** An interface for discovering, reading and consuming LF or CRLF terminated
 *  lines in pb_list instances.
 */
struct pb_line_reader {
  /** Indicates whether a line exists at the head of a pb_list instance. */
  bool (*has_line)(struct pb_line_reader * const line_reader);

  /** Returns the length of the line discovered by has_line. */
  uint64_t (*get_line_len)(struct pb_line_reader * const line_reader);
  /** Extracts the data of the line discovered by has_line.
   *
   * buf indicates the start of the destination memory region
   * len indicates the shorter of the number of bytes to read or the length of
   *     the memory region.
   */
  uint64_t (*get_line_data)(struct pb_line_reader * const line_reader,
                            uint8_t * const base, uint64_t len);

  uint64_t (*seek_line)(struct pb_line_reader * const line_reader);

  /** Marks the present position of line discovery as a line end.
   *
   * If the character proceeding the termination point is a cr, it will be
   * ignored in the line length calculation and included in the line. */
  void (*terminate_line)(struct pb_line_reader * const line_reader);
  /** Marks the present position of line discovery as a line end.
   *
   * If the character proceeding the termination point is a cr, it will be
   * considered in the line length calculation. */
  void (*terminate_line_and_cr)(struct pb_line_reader * const line_reader);

  /** Indicates whether the line discovered in has_line is:
   *    LF (false) or CRLF (true) */
  bool (*is_line_crlf)(struct pb_line_reader * const line_reader);
  /** Indicates whether the line discovery has reached the end of the list.
   *
   * Buffer end may be used as line end if terminate_line is called. */
  bool (*is_list_end)(struct pb_line_reader * const line_reader);

  /** Reset the current line discovery back to an initial state. */
  void (*reset)(struct pb_line_reader * const line_reader);

  /** Destroy a line reader instance. */
  void (*destroy)(struct pb_line_reader * const line_reader);

  struct pb_list *list;

  const struct pb_allocator *allocator;
};

/** A trivial line reader implementation that searches for lines via iterators.
 */
struct pb_line_reader *pb_trivial_line_reader_create(
                                          struct pb_list *list);
struct pb_line_reader *pb_trivial_line_reader_create_with_alloc(
                                          struct pb_list *list,
                                          const struct pb_allocator *allocator);

bool pb_trivial_line_reader_has_line(struct pb_line_reader * const line_reader);

uint64_t pb_trivial_line_reader_get_line_len(
                                     struct pb_line_reader * const line_reader);
uint64_t pb_trivial_line_reader_get_line_data(
                                     struct pb_line_reader * const line_reader,
                                     uint8_t * const buf, uint64_t len);

uint64_t pb_trivial_line_reader_seek_line(
                                     struct pb_line_reader * const line_reader);

bool pb_trivial_line_reader_is_crlf(struct pb_line_reader * const line_reader);
bool pb_trivial_line_reader_is_end(struct pb_line_reader * const line_reader);

void pb_trivial_line_reader_terminate_line(
                                     struct pb_line_reader * const line_reader);

void pb_trivial_line_reader_reset(struct pb_line_reader * const line_reader);
void pb_trivial_line_reader_destroy(struct pb_line_reader * const line_reader);





















#if 0
/**
 * The callbacks that a pb_list instance can use to allocate and free memory.
 *
 * The value ops passed the the various functions is the ops instance itself,
 * allowing a user to use the ops instance as a token for their specific
 * allocate and free implementation.
 */
struct pb_list_mem_ops {
  void *(*alloc_fn)(struct pb_list_mem_ops *ops, size_t size);
  void  (*free_fn)(struct pb_list_mem_ops *ops, void *ptr);
};



/**
 * List of pb_page structures that represent scattered memory regions for
 * reading and writing
 *
 * The pages allocated for or assigned to this list are considered in order
 * within the list, but may not be strictly in order in the process name space.
 */
struct pb_page_list {
  struct pb_page *head;
  struct pb_page *tail;

  struct pb_list_mem_ops *mem_ops;
};

/**
 * Clear all of the pages in a pb_page_list instance.
 */
void pb_page_list_clear(struct pb_page_list *list);

/**
 * Get the combined size of all non consumed data in a page list
 */
uint64_t pb_page_list_get_size(const struct pb_page_list *list);

/**
 * Internal functions that add data to pb_page_list structures.
 *
 * The pb_data instance will be
 */
bool pb_page_list_prepend_data(
  struct pb_page_list *list, struct pb_data *data);
bool pb_page_list_append_data(
  struct pb_page_list *list, struct pb_data *data);

void pb_page_list_prepend_page(
  struct pb_page_list *list, struct pb_page *page);
void pb_page_list_append_page(
  struct pb_page_list *list, struct pb_page *page);

bool pb_page_list_append_page_copy(
  struct pb_page_list *list, const struct pb_page *page);
bool pb_page_list_append_page_clone(
  struct pb_page_list *list, const struct pb_page *page);

/**
 * Expand a list, adding data and pages containing len bytes
 */
uint64_t pb_page_list_reserve(
  struct pb_page_list *list, uint64_t len, uint16_t max_page_len);

/**
 * Internal functions that write data to a pb_page_list from various sources
 */
uint64_t pb_page_list_write_data(
  struct pb_page_list *list, const void *buf, uint64_t len,
  uint16_t max_page_len);
uint64_t pb_page_list_write_data_ref(
  struct pb_page_list *list, const void *buf, uint64_t len);
uint64_t pb_page_list_write_page_list(
  struct pb_page_list *list, const struct pb_page_list *src_list,
  uint64_t len);

/**
 * Internal functions that manipulate the starting and ending points of a list
 */
uint64_t pb_page_list_push_page_list(
  struct pb_page_list *list, struct pb_page_list *src_list,
  uint64_t len);
uint64_t pb_page_list_seek(struct pb_page_list *list, uint64_t len);
uint64_t pb_page_list_trim(struct pb_page_list *list, uint64_t len);
uint64_t pb_page_list_rewind(struct pb_page_list *list, uint64_t len);

/**
 * Internal function that reads data from a list.
 */
uint64_t pb_page_list_read_data(
  struct pb_page_list *list, void *buf, uint64_t len);

/**
 * Internal function that duplicates part of a page list into another.
 */
bool pb_page_list_dup(
  struct pb_page_list *list, const struct pb_page_list* src_list,
  uint64_t off, uint64_t len);

/**
 * Internal function that inserts one list into another.
 */
uint64_t pb_page_list_insert_page_list(
  struct pb_page_list *list, uint64_t off,
  struct pb_page_list *src_list, uint64_t src_off,
  uint64_t len);



/**
 * A structure representing a data region for external consumption.
 */
struct pb_vec {
  void *base;
  size_t len;
};

/**
 * A structure used to iterate over buffer data.
 */
struct pb_iterator {
  struct pb_vec vec;
  struct pb_page *page;

  bool is_reverse;
};

/**
 * Initialise a pb_iterator given a pb_buffer instance.
 *
 * Iterator instance should be created by the caller.  As a stack variable
 * is the recommended method
 */
void pb_iterator_init(
  const struct pb_page_list *buffer, struct pb_iterator *iterator,
  bool is_reverse);
/**
 * Is the iterator a reverse iterator?
 */
bool pb_iterator_is_reverse(const struct pb_iterator *iterator);
/**
 * Does the iterator reference a valid page?
 */
bool pb_iterator_is_valid(const struct pb_iterator *iterator);
/**
 * Advance the iterator to the next element.
 */
void pb_iterator_next(struct pb_iterator *iterator);
/**
 * Get the pb_vec of the current iterator location.
 */
const struct pb_vec *pb_iterator_get_vec(struct pb_iterator *iterator);



/**
 * When a pb_buffer is asked to reserve write space, the requested size of
 * the reservation will be split into pages of this size by default.
 */
#define PB_BUFFER_DEFAULT_PAGE_SIZE                       1024


/**
 * A structure representing data and operations and information available
 * for it
 *
 * The operations include:
 * - Writing and reading, from and to various sources and destinations.
 * - Manipulation of the data including seeking, trimming, substring
 *   isolation and insertion.
 */
struct pb_buffer {
  /**
   * List of pages that are pre-allocated for writing
   */
  struct pb_page_list write_list;
  /**
   * List of pages that represent the buffer data.  These pages are either
   * pushed in from the write list, or directly created using the write
   * interfaces.
   */
  struct pb_page_list data_list;
  /**
   * List of pages that were previously seeked out of the data list.
   * The amount retained depends on the configuration of the buffer.
   * Pages from this list will be re-used if the buffer is rewound.
   */
  struct pb_page_list retain_list;

  /**
   * Last recorded data size of a non-dirty buffer instance.
   */
  uint64_t data_size;

  /**
   * Amount of data to retain after seeking.
   */
  uint64_t retain_max_size;
  /**
   * Maximum length of pages allocated when reserving pages for writing.
   */
  uint16_t reserve_max_page_len;

  /**
   * Has the data list been altered since its size was last measured?
   */
  bool is_data_dirty;
};

/**
 * Create a pb_buffer instance.
 */
struct pb_buffer *pb_buffer_create(void);
/**
 * Create a pb_buffer instance, providing specific list mem ops.
 */
struct pb_buffer *pb_buffer_create_ops(struct pb_list_mem_ops *ops);
/**
 * Destroy a pb_buffer instance.
 */
void pb_buffer_destroy(struct pb_buffer *buffer);

/**
 * Clean up write and retain lists but leave data in the pb_buffer unchanged.
 */
void pb_buffer_optimise(struct pb_buffer *buffer);
/**
 * Clear all data in the pb_buffer instance.
 */
void pb_buffer_clear(struct pb_buffer *buffer);

/**
 * Get the size of the data in the pb_buffer instance that is const.
 */
uint64_t pb_buffer_get_data_size_ro(const struct pb_buffer *buffer);
/**
 * Get the size of the data in the pb_buffer instance, caching the result.
 */
uint64_t pb_buffer_get_data_size(struct pb_buffer *buffer);
/**
 * Test whether a const pb_buffer instance is empty.
 */
bool pb_buffer_is_empty_ro(const struct pb_buffer *buffer);
/**
 * Test whether a pb_buffer instance is empty, caching the result.
 */
bool pb_buffer_is_empty(struct pb_buffer *buffer);


/**
 * Reserve writing capacity in a pb_buffer instance.
 */
uint64_t pb_buffer_reserve(struct pb_buffer *buffer, uint64_t len);
/**
 * Push reserved and written pages into the data list of a pb_buffer instance.
 */
uint64_t pb_buffer_push(struct pb_buffer *buffer, uint64_t len);


/**
 * Write a memory region to the pb_buffer instance by allocating and copying.
 */
uint64_t pb_buffer_write_data(
  struct pb_buffer *buffer, const void *buf, uint64_t len);
/**
 * Reference a memory region in a pb_buffer instance.
 */
uint64_t pb_buffer_write_data_ref(
  struct pb_buffer *buffer, const void *buf, uint64_t len);
/**
 * Write len bytes of src_buffer to a pb_buffer instance.
 */
uint64_t pb_buffer_write_buf(
  struct pb_buffer *buffer, const struct pb_buffer *src_buffer, uint64_t len);

/**
 * Get iterator to the pb_buffer write list.
 */
void pb_buffer_get_write_iterator(
  struct pb_buffer *buffer, struct pb_iterator *iterator);

/**
 * Seek len bytes into data list of a pb_buffer instance.
 */
uint64_t pb_buffer_seek(struct pb_buffer *buffer, uint64_t len);
/**
 * Trim len bytes from the end of a pb_buffer instance.
 */
uint64_t pb_buffer_trim(struct pb_buffer *buffer, uint64_t len);
/**
 * Rewind a pb_buffer instance by len bytes
 */
uint64_t pb_buffer_rewind(struct pb_buffer *buffer, uint64_t len);

/**
 * Read len bytes from a pb_buffer instance, into a memory region
 */
uint64_t pb_buffer_read_data(
  struct pb_buffer *buffer, void *buf, uint64_t len);

/**
 * Get iterator to the pb_buffer data list.
 */
void pb_buffer_get_data_iterator(
  struct pb_buffer *buffer, struct pb_iterator *iterator);

/**
 * Duplicate
 */
struct pb_buffer *pb_buffer_dup(struct pb_buffer *src_buffer);
struct pb_buffer *pb_buffer_dup_seek(
  struct pb_buffer *src_buffer, uint64_t off);
struct pb_buffer *pb_buffer_dup_trim(
  struct pb_buffer *src_buffer, uint64_t len);
struct pb_buffer *pb_buffer_dup_sub(
  struct pb_buffer *src_buffer, uint64_t off, uint64_t len);

/**
 * Insert
 */
uint64_t pb_buffer_insert_buf(
  struct pb_buffer *buffer, uint64_t off,
  struct pb_buffer *src_buffer, uint64_t src_off,
  uint64_t len);
#endif

#ifdef __cplusplus
}; // extern "C"
#endif

#endif /* PAGEBUF_H */
