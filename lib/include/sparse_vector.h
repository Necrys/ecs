#pragma once

#include <array>
#include <vector>
#include <limits>

namespace ecs {

class sparse_vector_base {
public:
  virtual ~sparse_vector_base() = default;
  virtual void erase( const size_t pos ) = 0;
};

template < typename T, size_t PageSize = 64 >
class sparse_vector : public sparse_vector_base {
public:
  static const size_t bad_index;

  ~sparse_vector() override;

  // modify
  void clear();

  T& insert( const size_t pos, const T& arg );

  template < typename... Args >
  T& emplace( const size_t pos, Args&&... args );

  void erase( const size_t pos ) override;

  // access
  bool exist( const size_t pos ) const noexcept;

  // totally unsafe access
  T& get_unsafe( const size_t pos ) noexcept;
  const T& get_unsafe( const size_t pos ) const noexcept;

  // safe access with on-access creation
  T& operator[]( const size_t pos );

  std::pair< size_t, size_t > index_range() const;

  // Number of live elements, without walking the pages.
  size_t size() const noexcept;

  // Visits every live element in slot order, which is ascending id order, and
  // stops early if the visitor asks it to. This is what makes iteration cost
  // follow the number of elements rather than the spread of their ids: the walk
  // skips the empty stretches entirely instead of testing each index in them.
  template < typename F >
  void each( F&& visitor ) const;

  void reserve( const size_t count );

private:
  class page {
  public:
    page();
    ~page();

    T& insert( const size_t pos, const T& arg );

    template < typename... Args >
    T& emplace( const size_t pos, Args&&... args );

    void erase( const size_t pos );

    bool exist( const size_t pos ) const noexcept;

    T& operator[]( const size_t pos );

    T& get_unsafe( const size_t pos ) noexcept;
    const T& get_unsafe( const size_t pos ) const noexcept;

    size_t min_index() const;
    size_t max_index() const;

    size_t size() const;

    // Slot-indexed access to the packed live range, used by sparse_vector::each.
    size_t position( const size_t slot ) const noexcept;
    const T& value( const size_t slot ) const noexcept;

  private:
    template < typename... Args >
    void take_place( const size_t pos, Args&&... args );

    std::array< size_t, PageSize > m_index;
    std::array< size_t, PageSize > m_back_index;
    typename std::aligned_storage< sizeof( T ), alignof( T ) >::type m_data[ PageSize ];
    size_t m_size;
  };

  page& get_or_create_page( const size_t page_idx );
  page* get_page( const size_t page_idx ) const;
  void erase_page( const size_t page_idx );
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
