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
  return pg.insert( pos % PageSize, arg );
}

template < typename T, size_t PageSize >
template < typename... Args >
T& sparse_vector< T, PageSize >::emplace( const size_t pos, Args&&... args ) {
  assert( pos < bad_index );

  auto& pg = get_or_create_page( pos / PageSize );
  return pg.emplace( pos % PageSize, std::forward< Args >( args )... );
}

template < typename T, size_t PageSize >
void sparse_vector< T, PageSize >::erase( const size_t pos ) {
  assert( pos < bad_index );

  size_t page_idx = pos / PageSize;
  auto pg = get_page( page_idx );
  if ( pg ) {
    pg->erase( pos % PageSize );
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
  return pg[ pos % PageSize ];
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
void sparse_vector< T, PageSize >::clear() {
  for ( auto& pg : m_pages ) {
    if ( pg ) {
      delete pg;
    }
  }

  m_pages.swap( std::vector< page* >() );
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

template < typename T, size_t PageSize >
void sparse_vector< T, PageSize >::page::erase( const size_t pos ) {
  assert( pos < PageSize );

  if ( m_index[ pos ] == bad_index ) {
    return;
  }

  reinterpret_cast< const T* >( m_data + m_index[ pos ] )->~T();

  for ( size_t i = m_index[ pos ] + 1; i < m_size; ++i ) {
    *reinterpret_cast< T* >( m_data + ( i - 1 ) ) = std::move( *reinterpret_cast< const T* >( m_data + i ) );
    reinterpret_cast< const T* >( m_data + i )->~T();
    --m_index[ m_back_index[ i ] ];
    m_back_index[ i - 1 ] = m_back_index[ i ];
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
  for ( size_t i = PageSize - 1; i >= 0; --i ) {
    if ( m_index[ i ] != bad_index ) {
      result = i;
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
template < typename... Args >
void sparse_vector< T, PageSize >::page::take_place( const size_t pos, Args&&... args ) {
  assert( pos < PageSize );
  assert( m_index[ pos ] == bad_index );

  if ( m_size == 0 || m_back_index[ m_size - 1 ] < pos ) {
    // inserting at end, fast way
    m_index[ pos ] = m_size;
    m_back_index[ m_size ] = pos;
    new( &m_data[ m_size++ ] ) T( std::forward< Args >( args )... );
  } else {
    size_t next = bad_index;
    for ( size_t i = pos; i < PageSize; ++i ) {
      if ( m_index[ i ] != bad_index ) {
        next = i;
        break;
      }
    }

    assert( next != bad_index );

    m_index[ pos ] = m_index[ next ];

    // shift elements
    size_t endIdx = m_index[ next ];
    for ( size_t i = m_size; i > endIdx; --i ) {
      *reinterpret_cast< T* >( m_data + i ) = std::move( *reinterpret_cast< const T* >( m_data + ( i - 1 ) ) );
      reinterpret_cast< const T* >( m_data + ( i - 1 ) )->~T();
      ++m_index[ m_back_index [ i - 1 ] ];
      m_back_index[ i ] = m_back_index[ i - 1 ];
    }

    m_back_index[ m_index[ pos ] ] = pos;

    new( &m_data[ m_index[ pos ] ] ) T( std::forward< Args >( args )... );
    ++m_size;
  }
}

}
