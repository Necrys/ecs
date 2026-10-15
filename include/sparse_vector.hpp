#pragma once

#include <cassert>
#include <iostream>

namespace ecs {

//=============================================================================
//
// sparse_vector
//
//=============================================================================
template < typename T, size_t PageSize >
inline typename sparse_vector< T, PageSize >::page& sparse_vector< T, PageSize >::get_or_create_page( const size_t page_idx ) {
  if ( m_pages.size() <= page_idx ) {
    m_pages.resize( page_idx + 1, nullptr );
  }

  if ( !m_pages[ page_idx ] ) {
    m_pages[ page_idx ] = new page();
  }

  return *m_pages[ page_idx ];
}

template < typename T, size_t PageSize >
inline typename sparse_vector< T, PageSize >::page* sparse_vector< T, PageSize >::get_page( const size_t page_idx ) const {
  page* result( nullptr );
  if ( m_pages.size() > page_idx ) {
    result = m_pages[ page_idx ];
  }

  return result;
}

template < typename T, size_t PageSize >
T& sparse_vector< T, PageSize >::insert( const size_t pos, const T& arg ) {
  assert( pos < bad_index );

  auto& pg = get_or_create_page( pos / PageSize );
  const bool added = !pg.exist( pos % PageSize );
  T& result = pg.insert( pos % PageSize, arg );
  if ( added ) {
    ++m_size;
  }

  return result;
}

template < typename T, size_t PageSize >
template < typename... Args >
T& sparse_vector< T, PageSize >::emplace( const size_t pos, Args&&... args ) {
  assert( pos < bad_index );

  auto& pg = get_or_create_page( pos / PageSize );
  const bool added = !pg.exist( pos % PageSize );
  T& result = pg.emplace( pos % PageSize, std::forward< Args >( args )... );
  if ( added ) {
    ++m_size;
  }

  return result;
}

template < typename T, size_t PageSize >
void sparse_vector< T, PageSize >::erase( const size_t pos ) {
  assert( pos < bad_index );

  size_t page_idx = pos / PageSize;
  auto pg = get_page( page_idx );
  if ( pg ) {
    const bool removed = pg->exist( pos % PageSize );
    pg->erase( pos % PageSize );
    if ( removed ) {
      --m_size;
    }
    if ( pg->size() == 0 ) {
      erase_page( page_idx );
    }
  }
}

template < typename T, size_t PageSize >
bool sparse_vector< T, PageSize >::exist( const size_t pos ) const noexcept {
  assert( pos < bad_index );

  bool result( false );

  auto pg = get_page( pos / PageSize );
  if ( pg ) {
    result = pg->exist( pos % PageSize );
  }

  return result;
}

// totally unsafe access
template < typename T, size_t PageSize >
T& sparse_vector< T, PageSize >::get_unsafe( const size_t pos ) noexcept {
  assert( pos < bad_index );
  assert( m_pages[ pos / PageSize ] );

  return m_pages[ pos / PageSize ]->get_unsafe( pos % PageSize );
}

template < typename T, size_t PageSize >
const T& sparse_vector< T, PageSize >::get_unsafe( const size_t pos ) const noexcept {
  assert( pos < bad_index );
  assert( m_pages[ pos / PageSize ] );

  return m_pages[ pos / PageSize ]->get_unsafe( pos % PageSize );
}

// safe access with on-access creation
template < typename T, size_t PageSize >
T& sparse_vector< T, PageSize >::operator[] ( const size_t pos ) {
  assert( pos < bad_index );

  auto& pg = get_or_create_page( pos / PageSize );
  const bool added = !pg.exist( pos % PageSize );
  T& result = pg[ pos % PageSize ];
  if ( added ) {
    ++m_size;
  }

  return result;
}

template < typename T, size_t PageSize >
size_t sparse_vector< T, PageSize >::size() const noexcept {
  return m_size;
}

template < typename T, size_t PageSize >
template < typename F >
void sparse_vector< T, PageSize >::each( F&& visitor ) const {
  for ( size_t page_idx = 0; page_idx < m_pages.size(); ++page_idx ) {
    const page* pg = m_pages[ page_idx ];
    if ( !pg ) {
      continue;
    }

    // A page keeps its live objects packed into the front of m_data and records
    // the position of each in m_back_index, so this walks the live elements
    // directly: no testing of indices that hold nothing.
    const size_t base = page_idx * PageSize;
    const size_t count = pg->size();
    for ( size_t slot = 0; slot < count; ++slot ) {
      if ( !visitor( base + pg->position( slot ), pg->value( slot ) ) ) {
        return;
      }
    }
  }
}

template < typename T, size_t PageSize >
std::pair< size_t, size_t > sparse_vector< T, PageSize >::index_range() const {
  auto result = std::make_pair( bad_index, bad_index );

  if ( m_pages.size() == 0 ) {
    return result;
  }

  for ( size_t i = 0; i < m_pages.size(); ++i ) {
    if ( m_pages[ i ] ) {
      result.first = m_pages[ i ]->min_index() + ( i * PageSize );
      break;
    }
  }

  for ( size_t i = m_pages.size(); i > 0; --i ) {
    if ( m_pages[ i - 1 ] ) {
      auto max_index = m_pages[ i - 1 ]->max_index();
      if ( max_index != bad_index ) {
        result.second = max_index + ( ( i - 1 ) * PageSize );
        break;
      }
    }
  }

  return result;
}

template < typename T, size_t PageSize >
sparse_vector< T, PageSize >::~sparse_vector() {
  clear_and_release();
}

template < typename T, size_t PageSize >
void sparse_vector< T, PageSize >::clear() {
  clear_and_release();
  m_pages.swap( std::vector< page* >() );
}

template < typename T, size_t PageSize >
void sparse_vector< T, PageSize >::clear_and_release() {
  for ( auto& pg : m_pages ) {
    delete pg;
    pg = nullptr;
  }

  m_size = 0;
}

template < typename T, size_t PageSize >
void sparse_vector< T, PageSize >::erase_page( const size_t page_idx ) {
  if ( m_pages[ page_idx ] ) {
    delete m_pages[ page_idx ];
    m_pages[ page_idx ] = nullptr;
  }
}

template < typename T, size_t PageSize >
void sparse_vector< T, PageSize >::reserve( const size_t count ) {
  assert( count < bad_index );

  size_t pages_count = ( count % PageSize == 0 )
    ? ( count / PageSize )
    : ( count / PageSize + 1 );

  m_pages.resize( pages_count, nullptr );

  for ( size_t i = 0; i < pages_count; ++i ) {
    if ( !m_pages[ i ] ) {
      m_pages[ i ] = new page();
    }
  }
}

//=============================================================================
//
// sparse_vector::page
//
//=============================================================================

template < typename T, size_t PageSize >
sparse_vector< T, PageSize >::page::page():
  m_size( 0 ) {
  m_index.fill( bad_index );
  m_back_index.fill( bad_index );
}

template < typename T, size_t PageSize >
sparse_vector< T, PageSize >::page::~page() {
  for ( size_t i = 0; i < m_size; ++i ) {
    reinterpret_cast< T* >( m_data + i )->~T();
  }
}

template < typename T, size_t PageSize >
T& sparse_vector< T, PageSize >::page::insert( const size_t pos, const T& arg ) {
  assert( pos < PageSize );

  if ( m_index[ pos ] == bad_index ) {
    take_place( pos, arg );
  }

  return *reinterpret_cast< T* >( m_data + m_index[ pos ] );
}

template < typename T, size_t PageSize >
template < typename... Args >
T& sparse_vector< T, PageSize >::page::emplace( const size_t pos, Args&&... args ) {
  assert( pos < PageSize );

  if ( m_index[ pos ] == bad_index ) {
    take_place( pos, std::forward< Args >( args )... );
  }

  return *reinterpret_cast< T* >( m_data + m_index[ pos ] );
}

// Page invariants, all in slot order:
//
//   * m_data[0 .. m_size) holds exactly the live objects, with no gaps, so the
//     destructor can destroy that range and nothing else;
//   * m_back_index[slot] is the position stored in that slot, and the sequence
//     increases with the slot;
//   * m_index[position] is the slot holding that position, or bad_index.
//
// Everything below is written against those three rules. Mixing the object move
// with the bookkeeping is what made the previous version destroy the wrong
// number of elements: the two do not walk the same range.
template < typename T, size_t PageSize >
void sparse_vector< T, PageSize >::page::erase( const size_t pos ) {
  assert( pos < PageSize );

  const size_t slot = m_index[ pos ];
  if ( slot == bad_index ) {
    return;
  }

  reinterpret_cast< T* >( m_data + slot )->~T();

  // Pull the following elements down one slot so that the hole ends up on top of
  // the live range. Constructing into the slot that was just released is what
  // keeps one live object per slot; assigning onto a live object would leave the
  // old value neither released nor accounted for.
  for ( size_t i = slot; i + 1 < m_size; ++i ) {
    new ( &m_data[ i ] ) T( std::move( *reinterpret_cast< T* >( m_data + ( i + 1 ) ) ) );
    reinterpret_cast< T* >( m_data + ( i + 1 ) )->~T();
  }

  // The bookkeeping covers one slot less than the live range: the entry that
  // fell off the top is simply dropped.
  for ( size_t i = slot; i + 1 < m_size; ++i ) {
    m_back_index[ i ] = m_back_index[ i + 1 ];
    m_index[ m_back_index[ i ] ] = i;
  }

  m_index[ pos ] = bad_index;
  m_back_index[ m_size - 1 ] = bad_index;

  --m_size;
}

template < typename T, size_t PageSize >
bool sparse_vector< T, PageSize >::page::exist( const size_t pos ) const noexcept {
  assert( pos < PageSize );
  return m_index[ pos ] != bad_index;
}

template < typename T, size_t PageSize >
T& sparse_vector< T, PageSize >::page::get_unsafe( const size_t pos ) noexcept {
  assert( pos < PageSize );
  return *reinterpret_cast< T* >( m_data + m_index[ pos ] );
}

template < typename T, size_t PageSize >
const T& sparse_vector< T, PageSize >::page::get_unsafe( const size_t pos ) const noexcept {
  assert( pos < PageSize );
  return *reinterpret_cast< const T* >( m_data + m_index[ pos ] );
}

template < typename T, size_t PageSize >
T& sparse_vector< T, PageSize >::page::operator[] ( const size_t pos ) {
  assert( pos < PageSize );

  if ( m_index[ pos ] == bad_index ) {
    take_place( pos, T() );
  }

  return *reinterpret_cast< T* >( m_data + m_index[ pos ] );
}

template < typename T, size_t PageSize >
size_t sparse_vector< T, PageSize >::page::min_index() const {
  size_t result = bad_index;
  for ( size_t i = 0; i < PageSize; ++i ) {
    if ( m_index[ i ] != bad_index ) {
      result = i;
      break;
    }
  }
  
  return result;
}

template < typename T, size_t PageSize >
size_t sparse_vector< T, PageSize >::page::max_index() const {
  size_t result = bad_index;
  for ( size_t i = PageSize; i > 0; --i ) {
    if ( m_index[ i - 1 ] != bad_index ) {
      result = i - 1;
      break;
    }
  }

  return result;
}

template < typename T, size_t PageSize >
size_t sparse_vector< T, PageSize >::page::size() const {
  return m_size;
}

template < typename T, size_t PageSize >
size_t sparse_vector< T, PageSize >::page::position( const size_t slot ) const noexcept {
  assert( slot < m_size );
  return m_back_index[ slot ];
}

template < typename T, size_t PageSize >
const T& sparse_vector< T, PageSize >::page::value( const size_t slot ) const noexcept {
  assert( slot < m_size );
  return *reinterpret_cast< const T* >( m_data + slot );
}

template < typename T, size_t PageSize >
template < typename... Args >
void sparse_vector< T, PageSize >::page::take_place( const size_t pos, Args&&... args ) {
  assert( pos < PageSize );
  assert( m_index[ pos ] == bad_index );

  // The first slot whose position is above pos, which is exactly where pos
  // belongs: the positions are kept increasing along the slots.
  const size_t end_idx = static_cast< size_t >(
    std::lower_bound( &m_back_index[ 0 ], &m_back_index[ m_size ], pos ) - &m_back_index[ 0 ] );

  // Make room in the bookkeeping first, so that the shifts below read the old
  // values and write the new ones without an intermediate buffer. The uppermost
  // entry, m_back_index[ m_size ], is dead: it is written before it is read, and
  // the slot above m_size holds no object.
  for ( size_t i = m_size; i > end_idx; --i ) {
    m_back_index[ i ] = m_back_index[ i - 1 ];
  }

  m_back_index[ end_idx ] = pos;

  for ( size_t i = end_idx + 1; i <= m_size; ++i ) {
    m_index[ m_back_index[ i ] ] = i;
  }

  m_index[ pos ] = end_idx;

  // Move the objects right, starting from the top so that every source is still
  // intact when it is read. The slot released by each step is reused by the next
  // one, and the release only happens after the value has been moved out, so
  // every slot holds exactly one live object throughout.
  for ( size_t i = m_size; i > end_idx; --i ) {
    new ( &m_data[ i ] ) T( std::move( *reinterpret_cast< T* >( m_data + ( i - 1 ) ) ) );
    reinterpret_cast< T* >( m_data + ( i - 1 ) )->~T();
  }

  // The slot at end_idx was released by the last step and now holds no object.
  new ( &m_data[ end_idx ] ) T( std::forward< Args >( args )... );

  ++m_size;
}

}
