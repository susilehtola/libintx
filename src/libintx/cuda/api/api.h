#ifndef LIBINTX_CUDA_API_H
#define LIBINTX_CUDA_API_H

#include <memory>
#include <stdexcept>
#include <algorithm>

namespace libintx::cuda {

  struct Stream;

  void memcpy(void *dst, const void *src, size_t bytes);
  void memcpy(void *dst, const void *src, size_t bytes, Stream&);

  template<typename T>
  void copy(const T *begin, const T *end, T *it) {
    memcpy(it, begin, sizeof(T)*size_t(end-begin));
  }

  template<typename T>
  void copy(const T *begin, const T *end, T *it, Stream &s) {
    memcpy(it, begin, sizeof(T)*size_t(end-begin), s);
  }

  void memset(void *dst, const int value, size_t bytes);
  void memset(void *dst, const int value, size_t bytes, Stream&);

  namespace detail {

    template<typename T, class memory_t>
    T* allocate(size_t n) {
      static_assert(
        sizeof(T) % alignof(T) == 0,
        "Type alignment not supported"
      );
      static_assert(
        std::is_trivially_constructible<T>::value,
        "Non-trivial constructor not supported"
      );
      return static_cast<T*>(
        memory_t::allocate(n*sizeof(T))
      );
    }

    template<typename T, class memory_t>
    std::shared_ptr<T> make_shared(size_t n) {
      static_assert(std::is_array<T>::value, "");
      using element_type = typename std::remove_extent<T>::type;
      auto *ptr = allocate<element_type,memory_t>(n);
      return std::shared_ptr<T>(ptr, &memory_t::free);
    }

  }

  struct device_memory_t {
    static void* allocate(size_t);
    static void free(void*);
    static void memset(void *dst, const int value, size_t bytes);
    static void memset(void *dst, const int value, size_t bytes, Stream& stream);
  };

  struct host_memory_t {
    static void* allocate(size_t);
    static void free(void*);
    static void memset(void *dst, const int value, size_t bytes);
  };

  template<typename T, class memory_t>
  struct vector {

    vector() : data_(nullptr, &memory_t::free) {}

    explicit vector(size_t size) : vector() {
      resize(size);
    }

    //template<typename U>
    vector(const std::initializer_list<T> &data) : vector() {
      static_assert(std::is_same<memory_t,host_memory_t>::value, "");
      resize(data.size());
      std::copy(data.begin(), data.end(), this->data());
    }

    vector(const T *data, size_t size) : vector() {
      assign(data, size);
    }

    template<typename M>
    vector(const vector<T,M> &v) : vector(v.data(), v.size()) {}

    T* begin() { return this->data(); }
    T* end() { return this->data() + this->size(); }

    const T* begin() const { return this->data(); }
    const T* end() const { return this->data() + this->size(); }

    void assign(const T *begin, const T *end) {
      assert(begin <= end);
      assign(begin, end-begin);
    }

    void assign(const T *data, size_t size) {
      resize(size);
      memcpy(this->data(), data, sizeof(T)*size);
    }

    void assign_zero(size_t size) {
      resize(size);
      this->memset(0);
    }

    void memset(char value) {
      memory_t::memset(this->data(), value, sizeof(T)*this->size());
    }

    void reserve(size_t size) {
      if (size > capacity_) {
        data_.reset(detail::allocate<T,memory_t>(size));
        capacity_ = size;
      }
    }

    void resize(size_t size) {
      reserve(size);
      size_ = size;
    }

    void clear() {
      size_ = 0;
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }

    void push_back(const T& v) {
      if (this->size()+1 > this->capacity_) {
        throw std::runtime_error("cuda::vector::capacity exceeded");
      }
      data_[size_] = v;
      ++size_;
    }

    auto& operator[](size_t idx) { return data_[idx]; }
    const auto& operator[](size_t idx) const { return data_[idx]; }

    T* data() { return data_.get(); }
    const T* data() const { return data_.get(); }

    void swap(vector &rhs) {
      std::swap(this->size_, rhs.size_);
      std::swap(this->capacity_, rhs.capacity_);
      std::swap(this->data_, rhs.data_);
    }

  private:
    size_t size_ = 0;
    size_t capacity_ = 0;
    std::unique_ptr<T[], void(*)(void*) /*deleter*/ > data_;

  };

  template<typename T, typename M>
  void memset(vector<T,M> &v, char value) {
    M::memset(v.data(), value, sizeof(T)*v.size());
  }

  template<typename T>
  void memset(vector<T,device_memory_t> &v, const char value, Stream& stream) {
    device_memory_t::memset(v.data(), value, sizeof(T)*v.size(), stream);
  }

  namespace host {

    template<typename T>
    std::shared_ptr<T> make_shared(size_t n) {
      return detail::make_shared<T, host_memory_t>(n);
    }

    template<typename T>
    using vector = vector<T,host_memory_t>;

    template<typename T>
    void register_pointer(T *ptr, size_t size) {
      register_pointer<const void>(ptr, size*sizeof(T));
    }

    template<>
    void register_pointer(const void *ptr, size_t);

    void unregister_pointer(const void *ptr);

    template<typename T>
    T* device_pointer(T *ptr) {
      return reinterpret_cast<T*>(
        device_pointer((void*)ptr)
      );
    }

    template<>
    void* device_pointer(void*);

  }

  namespace device {

    template<typename T>
    std::shared_ptr<T> make_shared(size_t n) {
      return detail::make_shared<T, device_memory_t>(n);
    }

    template<typename T>
    using vector = vector<T,device_memory_t>;

    bool synchronize();

  }

  namespace kernel {

    void set_max_dynamic_shared_memory_size(const void*, size_t);
    void set_prefered_shared_memory_carveout(const void*, size_t);

    template<typename F>
    void set_max_dynamic_shared_memory_size(const F &f, size_t bytes) {
      set_max_dynamic_shared_memory_size((const void*)f, bytes);
    }

    template<typename F>
    void set_prefered_shared_memory_carveout(const F &f, size_t carveout) {
      set_prefered_shared_memory_carveout((const void*)f, carveout);
    }

  }

  namespace error {
    void ensure_none(const char* = nullptr);
  }

}

#endif /* LIBINTX_CUDA_API_H */
