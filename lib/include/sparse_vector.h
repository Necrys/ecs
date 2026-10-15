#pragma once

#include <array>
#include <vector>
#include <limits>

namespace ecs {

class sparse_vector_base {
public:
  // Defined out of line, in lib/src/sparse_vector.cpp: a virtual destructor that is
  // the class's only out-of-line virtual gives the vtable a single home instead of
  // a weak copy in every translation unit that touches a storage.
  virtual ~sparse_vector_base();

  // The rest of the set, declared for the same reason as in the other bases: the
  // class says how it is copied and moved as well as how it dies.
  sparse_vector_base() = default;
  sparse_vector_base( const sparse_vector_base& ) = default;
  sparse_vector_base( sparse_vector_base&& ) = default;
  sparse_vector_base& operator=( const sparse_vector_base& ) = default;
  sparse_vector_base& operator=( sparse_vector_base&& ) = default;

  virtual void erase( size_t pos ) = 0;
};

template < typename T, size_t PageSize = 64 >
class sparse_vector : public sparse_vector_base {
public:
  static const size_t bad_index;

  sparse_vector() = default;
  ~sparse_vector() override;

  // A storage owns pages of raw memory, so a copy would be two owners of the same
  // pages. Until it is explicitly written, copying and moving are deleted rather
  // than left to the implicit shallow copy that a declared destructor would
  // otherwise still allow.
  sparse_vector( const sparse_vector& ) = delete;
  sparse_vector( sparse_vector&& ) = delete;
  sparse_vector& operator=( const sparse_vector& ) = delete;
  sparse_vector& operator=( sparse_vector&& ) = delete;

  // modify
  void clear();

  T& insert( size_t pos, const T& arg );

  template < typename... Args >
  T& emplace( size_t pos, Args&&... args );

  void erase( size_t pos ) override;

  // access
  [[nodiscard]] bool exist( size_t pos ) const noexcept;

  // totally unsafe access
  T& get_unsafe( size_t pos ) noexcept;
  [[nodiscard]] const T& get_unsafe( size_t pos ) const noexcept;

  // safe access with on-access creation
  T& operator[]( size_t pos );

  [[nodiscard]] std::pair< size_t, size_t > index_range() const;

  // Number of live elements, without walking the pages.
  [[nodiscard]] size_t size() const noexcept;

  // Visits every live element in slot order, which is ascending id order, and
  // stops early if the visitor asks it to. This is what makes iteration cost
  // follow the number of elements rather than the spread of their ids: the walk
  // skips the empty stretches entirely instead of testing each index in them.
  template < typename F >
  void each( F visitor ) const;

  void reserve( size_t count );

private:
  class page {
  public:
    page();
    ~page();

    // A page is a fixed array of slots it carves objects into; copying it would
    // copy the slots without copying the objects' identity, and the owner is the
    // sparse_vector either way.
    page( const page& ) = delete;
    page( page&& ) = delete;
    page& operator=( const page& ) = delete;
    page& operator=( page&& ) = delete;

    T& insert( size_t pos, const T& arg );

    template < typename... Args >
    T& emplace( size_t pos, Args&&... args );

    void erase( size_t pos );

    [[nodiscard]] bool exist( size_t pos ) const noexcept;

    T& operator[]( size_t pos );

    T& get_unsafe( size_t pos ) noexcept;
    [[nodiscard]] const T& get_unsafe( size_t pos ) const noexcept;

    [[nodiscard]] size_t min_index() const;
    [[nodiscard]] size_t max_index() const;

    [[nodiscard]] size_t size() const;

    // Slot-indexed access to the packed live range, used by sparse_vector::each.
    [[nodiscard]] size_t position( size_t slot ) const noexcept;
    [[nodiscard]] const T& value( size_t slot ) const noexcept;

  private:
    template < typename... Args >
    void take_place( size_t pos, Args&&... args );

    std::array< size_t, PageSize > m_index;
    std::array< size_t, PageSize > m_back_index;
    std::aligned_storage_t< sizeof( T ), alignof( T ) > m_data[ PageSize ];
    size_t m_size{ 0 };
  };

  page& get_or_create_page( size_t page_idx );
  page* get_page( size_t page_idx ) const;
  void erase_page( size_t page_idx );
  void clear_and_release();

  std::vector< page* > m_pages;

  // Maintained on every mutation so that the walk can pick the storage with the
  // fewest elements without touching the pages first.
  size_t m_size = 0;
};

template < typename T, size_t PageSize >
const size_t sparse_vector< T, PageSize >::bad_index = std::numeric_limits< size_t >::max();

}

#include "sparse_vector.hpp"
