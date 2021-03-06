/*******************************************************************************
 *  Copyright 2015, 2016 Nick Jones <nick.fa.jones@gmail.com>
 *  Copyright 2016 Network Box Corporation Limited
 *      Jeff He <jeff.he@network-box.com>
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

#ifndef PAGEBUF_HPP
#define PAGEBUF_HPP


#include <pagebuf/pagebuf.h>

#include <string>


namespace pb
{

class data_reader;
class line_reader;



/** C++ wrapper around pb_buffer, using a pb_trivial_buffer. */
class buffer {
  public:
    friend class data_reader;
    friend class line_reader;

  public:
    /** C++ wrapper around pb_buffer_iterator. */
    class iterator {
      public:
        friend class buffer;

      public:
        typedef std::ptrdiff_t difference_type;
        typedef std::bidirectional_iterator_tag iterator_category;

        typedef struct pb_data_vec value_type;
        typedef struct pb_data_vec* pointer;
        typedef struct pb_data_vec& reference;

      public:
        iterator() :
            buffer_iterator_({}),
            buffer_(0) {
        }

      private:
        iterator(struct pb_buffer * const buffer, bool at_end) :
            buffer_iterator_({}),
            buffer_(buffer) {
          if (!at_end)
            pb_buffer_get_iterator(buffer_, &buffer_iterator_);
          else
            pb_buffer_get_end_iterator(buffer_, &buffer_iterator_);
        }

      public:
        iterator(iterator&& rvalue) :
            buffer_iterator_({}),
            buffer_(0) {
          *this = rvalue;
        }

        iterator(const iterator& rvalue) :
            buffer_iterator_({}),
            buffer_(0) {
          *this = rvalue;
        }

        ~iterator() {
          buffer_iterator_ = {};
          buffer_ = 0;
        }

      public:
        iterator& operator=(iterator&& rvalue) {
          buffer_iterator_ = rvalue.buffer_iterator_;
          buffer_ = rvalue.buffer_;

          rvalue.buffer_iterator_ = {};
          rvalue.buffer_ = 0;

          return *this;
        }

        iterator& operator=(const iterator& rvalue) {
          buffer_iterator_ = rvalue.buffer_iterator_;
          buffer_ = rvalue.buffer_;

          return *this;
        }

      public:
        bool operator==(const iterator& rvalue) const {
          return
            ((buffer_ == rvalue.buffer_) &&
             (pb_buffer_cmp_iterator(
                buffer_, &buffer_iterator_, &rvalue.buffer_iterator_)));
        }

        bool operator!=(const iterator& rvalue) const {
          return !(*this == rvalue);
        }


      public:
        iterator& operator++() {
          pb_buffer_next_iterator(buffer_, &buffer_iterator_);

          return *this;
        }

        iterator operator++(int) {
           iterator temp(*this);
           ++(*this);
           return temp;
        }

      public:
        iterator& operator--() {
          pb_buffer_prev_iterator(buffer_, &buffer_iterator_);

          return *this;
        }

        iterator operator--(int) {
          iterator temp(*this);
          --(*this);
          return temp;
        }

      public:
        struct pb_data_vec& operator*() const {
          return *buffer_iterator_.data_vec;
        }

        struct pb_data_vec* operator->() const {
          return buffer_iterator_.data_vec;
        }

      private:
        struct pb_buffer_iterator buffer_iterator_;

        struct pb_buffer *buffer_;
    };

  public:
    class byte_iterator {
      public:
        friend class buffer;

      public:
        typedef std::bidirectional_iterator_tag iterator_category;
        typedef char value_type;
        typedef std::ptrdiff_t difference_type;
        typedef char* pointer;
        typedef char& reference;

      public:
        byte_iterator() :
            byte_iterator_({}),
            buffer_(0) {
        }

      private:
        byte_iterator(struct pb_buffer * const buffer, bool at_end) :
            byte_iterator_({}),
            buffer_(buffer) {
          if (!at_end)
            pb_buffer_get_byte_iterator(buffer_, &byte_iterator_);
          else
            pb_buffer_get_end_byte_iterator(buffer_, &byte_iterator_);
        }

      public:
        byte_iterator(byte_iterator&& rvalue) :
            byte_iterator_(rvalue.byte_iterator_),
            buffer_(rvalue.buffer_) {
          rvalue.byte_iterator_ = {};
          rvalue.buffer_ = 0;
        }

        byte_iterator(const byte_iterator& rvalue) :
            byte_iterator_(rvalue.byte_iterator_),
            buffer_(rvalue.buffer_) {
        }

        ~byte_iterator() {
          buffer_ = 0;
        }

      public:
        byte_iterator& operator=(byte_iterator&& rvalue) {
          byte_iterator_ = rvalue.byte_iterator_;
          buffer_ = rvalue.buffer_;

          rvalue.byte_iterator_ = {};
          rvalue.buffer_ = 0;

          return *this;
        }

        byte_iterator& operator=(const byte_iterator& rvalue) {
          byte_iterator_ = rvalue.byte_iterator_;
          buffer_ = rvalue.buffer_;

          return *this;
        }

      public:
        bool operator==(const byte_iterator& rvalue) const {
          return
            ((buffer_ == rvalue.buffer_) &&
             (pb_buffer_cmp_byte_iterator(
                buffer_, &byte_iterator_, &rvalue.byte_iterator_)));
        }

        bool operator!=(const byte_iterator& rvalue) const {
          return !(*this == rvalue);
        }

      public:
        byte_iterator& operator++() {
          pb_buffer_next_byte_iterator(buffer_, &byte_iterator_);

          return *this;
        }

        byte_iterator operator++(int) {
           byte_iterator temp(*this);
           ++(*this);
           return temp;
        }

      public:
        byte_iterator& operator--() {
          pb_buffer_prev_byte_iterator(buffer_, &byte_iterator_);

          return *this;
        }

        byte_iterator operator--(int) {
          byte_iterator temp(*this);
          --(*this);
          return temp;
        }

      public:
        char operator*() {
          return pb_buffer_byte_iterator_get_current_byte(&byte_iterator_);
        }

      private:
        struct pb_buffer_byte_iterator byte_iterator_;

        struct pb_buffer *buffer_;
    };

  public:
    buffer() :
        buffer_(pb_trivial_buffer_create()) {
    }

    explicit buffer(const struct pb_buffer_strategy *strategy) :
        buffer_(pb_trivial_buffer_create_with_strategy(strategy)) {
    }

    explicit buffer(const struct pb_allocator *allocator) :
        buffer_(pb_trivial_buffer_create_with_alloc(allocator)) {
    }

    buffer(const struct pb_buffer_strategy *strategy,
           const struct pb_allocator *allocator) :
        buffer_(
          pb_trivial_buffer_create_with_strategy_with_alloc(
            strategy, allocator)) {
    }

    buffer(buffer&& rvalue) :
        buffer_(rvalue.buffer_) {
      rvalue.buffer_ = 0;
    }

  private:
    buffer(const buffer& rvalue) :
        buffer_(0) {
    }

  protected:
    buffer(struct pb_buffer *buf) :
        buffer_(buf) {
    }

  public:
    virtual ~buffer() {
      destroy();
    }

  public:
    buffer& operator=(buffer&& rvalue) {
      destroy();

      buffer_ = rvalue.buffer_;

      rvalue.buffer_ = 0;

      return *this;
    }

  private:
    buffer& operator=(const buffer& rvalue) {
      return *this;
    }

  public:
    const struct pb_buffer_strategy& get_strategy() const {
      return *buffer_->strategy;
    }

    const struct pb_allocator& get_allocator() const {
      return *buffer_->allocator;
    }

    struct pb_buffer& get_implementation() const {
      return *buffer_;
    }

  public:
    uint64_t get_data_revision() const {
      return pb_buffer_get_data_revision(buffer_);
    }

    uint64_t get_data_size() const {
      return pb_buffer_get_data_size(buffer_);
    }

  public:
    uint64_t extend(uint64_t len) {
      return pb_buffer_extend(buffer_, len);
    }

    uint64_t reserve(uint64_t size) {
      return pb_buffer_reserve(buffer_, size);
    }

    uint64_t rewind(uint64_t len) {
      return pb_buffer_rewind(buffer_, len);
    }

    uint64_t seek(uint64_t len) {
      return pb_buffer_seek(buffer_, len);
    }

    uint64_t trim(uint64_t len) {
      return pb_buffer_trim(buffer_, len);
    }

  public:
    iterator begin() const {
      return iterator(buffer_, false);
    }

    iterator end() const {
      return iterator(buffer_, true);
    }

  public:
    byte_iterator byte_begin() const {
      return byte_iterator(buffer_, false);
    }

    byte_iterator byte_end() const {
      return byte_iterator(buffer_, true);
    }

  public:
    uint64_t insert(
        const iterator& buffer_iterator, size_t offset,
        const void *buf, uint64_t len) {
      return
        pb_buffer_insert_data(
          buffer_, &buffer_iterator.buffer_iterator_, offset, buf, len);
    }

    uint64_t insert_ref(
        const iterator& buffer_iterator, size_t offset,
        const void *buf, uint64_t len) {
      return
        pb_buffer_insert_data_ref(
          buffer_, &buffer_iterator.buffer_iterator_, offset, buf, len);
    }

    uint64_t insert(
        const iterator& buffer_iterator, size_t offset,
        const buffer& src_buf, uint64_t len) {
      return
        pb_buffer_insert_buffer(
          buffer_, &buffer_iterator.buffer_iterator_, offset,
          src_buf.buffer_, len);
    }

  public:
    uint64_t write(const void *buf, uint64_t len) {
      return pb_buffer_write_data(buffer_, buf, len);
    }

    uint64_t write_ref(const void *buf, uint64_t len) {
      return pb_buffer_write_data_ref(buffer_, buf, len);
    }

    uint64_t write(const buffer& src_buf, uint64_t len) {
      return pb_buffer_write_buffer(buffer_, src_buf.buffer_, len);
    }

  public:
    uint64_t overwrite(const void *buf, uint64_t len) {
      return pb_buffer_overwrite_data(buffer_, buf, len);
    }

    uint64_t overwrite(const buffer& src_buf, uint64_t len) {
      return pb_buffer_overwrite_buffer(buffer_, src_buf.buffer_, len);
    }

  public:
    uint64_t read(void * const buf, uint64_t len) const {
      return pb_buffer_read_data(buffer_, buf, len);
    }

  public:
    void clear() {
      pb_buffer_clear(buffer_);
    }

  protected:
    void destroy() {
      if (buffer_) {
        pb_buffer_destroy(buffer_);
        buffer_ = 0;
      }
    }

  protected:
    struct pb_buffer *buffer_;
};



/** C++ Wrapper around pb_data_reader. */
class data_reader {
  public:
    data_reader() :
        data_reader_(0) {
    }

    explicit data_reader(buffer& buf) :
        data_reader_(pb_data_reader_create(buf.buffer_)) {
    }

    data_reader(data_reader&& rvalue) :
        data_reader_(rvalue.data_reader_) {
      rvalue.data_reader_ = 0;
    }

    data_reader(const data_reader& rvalue) :
        data_reader_(0) {
      *this = rvalue;
    }

    ~data_reader() {
      destroy();
    }

  public:
    data_reader& operator=(data_reader&& rvalue) {
      destroy();

      data_reader_ = rvalue.data_reader_;

      rvalue.data_reader_ = 0;

      return *this;
    }

    data_reader& operator=(const data_reader& rvalue) {
      destroy();

      if (rvalue.data_reader_)
        data_reader_ = pb_data_reader_clone(rvalue.data_reader_);

      return *this;
    }

  public:
    void reset() {
      if (data_reader_)
        pb_data_reader_reset(data_reader_);
    }

  protected:
    void destroy() {
      reset();

      if (data_reader_) {
        pb_data_reader_destroy(data_reader_);
        data_reader_ = 0;
      }
    }

  public:
    uint64_t read(void * const buf, uint64_t len) {
      return pb_data_reader_read(data_reader_, buf, len);
    }

    uint64_t consume(void * const buf, uint64_t len) {
      return pb_data_reader_consume(data_reader_, buf, len);
    }

  protected:
    struct pb_data_reader *data_reader_;
};



/** C++ Wrapper around pb_line_reader. */
class line_reader {
  public:
    line_reader() :
        line_reader_(0),
        has_line_(false) {
    }

    explicit line_reader(buffer& buf) :
        line_reader_(pb_line_reader_create(buf.buffer_)),
        has_line_(false) {
    }

    line_reader(line_reader&& rvalue) :
        line_reader_(rvalue.line_reader_),
        has_line_(false) {
      rvalue.line_reader_ = 0;
      rvalue.line_.clear();
      rvalue.has_line_ = false;
    }

    line_reader(const line_reader& rvalue) :
        line_reader_(0),
        has_line_(false) {
      *this = rvalue;
    }

    ~line_reader() {
      destroy();
    }

  public:
    line_reader& operator=(line_reader&& rvalue) {
      destroy();

      line_reader_ = rvalue.line_reader_;

      rvalue.line_reader_ = 0;
      rvalue.line_.clear();
      rvalue.has_line_ = false;

      return *this;
    }

    line_reader& operator=(const line_reader& rvalue) {
      destroy();

      if (rvalue.line_reader_)
        line_reader_ = pb_line_reader_clone(rvalue.line_reader_);

      return *this;
    }

  public:
    void reset() {
      if (line_reader_)
        pb_line_reader_reset(line_reader_);

      line_.clear();

      has_line_ = false;
    }

  protected:
    void destroy() {
      reset();

      if (line_reader_) {
        pb_line_reader_destroy(line_reader_);
        line_reader_ = 0;
      }
    }

  public:
    bool has_line() {
      if (has_line_)
        return true;

      if (!pb_line_reader_has_line(line_reader_))
        return false;

      has_line_ = true;

      size_t line_len = pb_line_reader_get_line_len(line_reader_);

      line_.resize(line_len);
      pb_line_reader_get_line_data(
        line_reader_,
        const_cast<char*>(line_.data()),
        line_len);

      return true;
    }

  public:
    size_t get_line_len() {
      if (!has_line())
        return 0;

      return line_.size();
    }

    const std::string& get_line() {
      return line_;
    }

  public:
    size_t seek_line() {
      if (!has_line())
        return 0;

      size_t seeked = pb_line_reader_seek_line(line_reader_);

      reset();

      return seeked;
    }

  public:
    void terminate_line() {
      terminate_line(false);
    }

    void terminate_line(bool check_cr) {
      if (!check_cr)
        pb_line_reader_terminate_line(line_reader_);
      else
        pb_line_reader_terminate_line_check_cr(line_reader_);
    }

  public:
    bool is_line_crlf() {
      if (!has_line())
        return false;

      return pb_line_reader_is_crlf(line_reader_);
    }

    bool is_line_end() {
      return pb_line_reader_is_end(line_reader_);
    }

  protected:
    struct pb_line_reader *line_reader_;

  protected:
    std::string line_;

    bool has_line_;
};

}; /* namespace pagebuf */

#endif /* PAGEBUF_HPP */
