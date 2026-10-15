#pragma once

#include <functional>

namespace ecs {

template < typename T >
sparse_vector< T >& components_storage::get_or_create_storage() {
  const auto idx = component_type< T >::id;

  if ( m_componentStorages.size() <= idx ) {
    m_componentStorages.resize( idx + 1, nullptr );
  }

  if ( !m_componentStorages[ idx ] ) {
    m_componentStorages[ idx ] = static_cast< sparse_vector_base* >( new sparse_vector< T >() );
  }

  return static_cast< sparse_vector< T >& >( *( m_componentStorages[ idx ] ) );
}

template < typename T >
sparse_vector< T >* components_storage::get_storage() const {
  const auto idx = component_type< T >::id;

  if ( m_componentStorages.size() <= idx ) {
    return nullptr;
  }

  return static_cast< sparse_vector< T >* >( m_componentStorages[ idx ] );
}

template < typename T, typename... Ts >
T& components_storage::add_entity_component( const eid_t id, Ts&&... args ) {
  auto& storage = get_or_create_storage< T >();
  return storage.emplace( id, args... );
}

template < typename T >
T* components_storage::get_entity_component( const eid_t id ) {
  auto& storage = get_or_create_storage< T >();
  if ( storage.exist( id ) ) {
    return &storage.get_unsafe( id );
  }

  return nullptr;
}

template < typename T >
const T* components_storage::get_entity_component( const eid_t id ) const {
  const auto storage = get_storage< T >();
  if ( storage && storage->exist( id ) ) {
    return &storage->get_unsafe( id );
  }

  return nullptr;
}

template < typename T, typename... Ts >
T& components_storage::set_entity_component( const eid_t id, const T& c ) {
  auto& storage = get_or_create_storage< T >();
  return storage[ id ] = c;
}

template < typename T >
void components_storage::remove_entity_component( const eid_t id ) {
  auto& storage = get_or_create_storage< T >();
  storage.erase( id );
}

template < class Ft, class... Rs >
void components_storage::join_impl( const eid_t id, Ft&& f, Rs&&... rs ) {
  f( id, rs... );
}

template < class T, class... Ts, class Ft, class... Rs >
void components_storage::join_impl( const eid_t id, Ft&& f, Rs&&... rs ) {
  const auto storage = get_storage< T >();
  if ( !storage || !storage->exist( id ) ) {
    return;
  }

  join_impl < Ts... >( id, std::forward< Ft >( f ), std::forward< Rs >( rs )..., storage->get_unsafe( id ) );
}

template < typename... Ts, typename Ft >
void components_storage::join( Ft&& f ) {
  eid_t min_index = 0;
  eid_t max_index = 0;

  if ( !get_index_range< Ts... >( min_index, max_index ) ) {
    return;
  }

  for ( eid_t i = min_index; i <= max_index; ++i ) {
    join_impl< Ts... >( i, std::forward< Ft >( f ) );
  }
}

// Returns false when none of the requested components has a single element, in
// which case there is nothing to iterate at all. The bounds are reported through
// a bool rather than a sentinel value: index_range() works in size_t and its
// "no index" marker does not survive the narrowing to eid_t.
template < class T >
bool components_storage::get_index_range( eid_t& min_index, eid_t& max_index ) {
  const auto storage = get_storage< T >();
  if ( !storage ) {
    return false;
  }

  const auto range = storage->index_range();
  if ( range.first == sparse_vector< T >::bad_index ) {
    return false;
  }

  min_index = static_cast< eid_t >( range.first );
  max_index = static_cast< eid_t >( range.second );
  return true;
}

template < class T, class... Rs >
typename std::enable_if< sizeof...( Rs ) != 0, bool >::type components_storage::get_index_range( eid_t& min_index, eid_t& max_index ) {
  const auto storage = get_storage< T >();

  if ( !storage ) {
    return get_index_range< Rs... >( min_index, max_index );
  }

  const auto range = storage->index_range();
  if ( range.first == sparse_vector< T >::bad_index ) {
    return get_index_range< Rs... >( min_index, max_index );
  }

  min_index = static_cast< eid_t >( range.first );
  max_index = static_cast< eid_t >( range.second );

  eid_t other_min = 0;
  eid_t other_max = 0;
  if ( get_index_range< Rs... >( other_min, other_max ) ) {
    min_index = min_index > other_min ? other_min : min_index;
    max_index = max_index < other_max ? other_max : max_index;
  }

  return true;
}

template < typename... Ts >
components_storage::join_exclude_wrapper< Ts... > components_storage::join() {
  return components_storage::join_exclude_wrapper< Ts... >( *this );
}

template < typename... Ts >
template < class Ft, class... Rs >
void components_storage::join_exclude_wrapper< Ts... >::join_impl( const eid_t id, Ft&& f, Rs&&... rs ) {
  f( id, rs... );
}

template < typename... Ts >
template < class T, class... TsJoin, class Ft, class... Rs >
void components_storage::join_exclude_wrapper< Ts... >::join_impl( const eid_t id, Ft&& f, Rs&&... rs ) {
  const auto storage = m_storage.get_storage< T >();
  if ( !storage || !storage->exist( id ) ) {
    return;
  }

  join_impl < TsJoin... >( id, std::forward< Ft >( f ), std::forward< Rs >( rs )..., storage->get_unsafe( id ) );
}

template < typename... Ts >
template < class Ft >
void components_storage::join_exclude_wrapper< Ts... >::exclude_impl( const eid_t id, Ft&& f ) {
  join_impl < Ts... >( id, std::forward< Ft >( f ) );
}

template < typename... Ts >
template < class T, class... TsEx, class Ft >
void components_storage::join_exclude_wrapper< Ts... >::exclude_impl( const eid_t id, Ft&& f ) {
  const auto storage = m_storage.get_storage< T >();
  if ( storage && storage->exist( id ) ) {
    return;
  }

  exclude_impl < TsEx... >( id, std::forward< Ft >( f ) );
}

template < typename... Ts >
components_storage::join_exclude_wrapper< Ts... >::join_exclude_wrapper( components_storage& storage ):
  m_storage( storage ) {
}

template < typename... Ts >
template < typename... TsEx, typename Ft >
void components_storage::join_exclude_wrapper< Ts... >::exclude( Ft&& func ) {
  eid_t min_index = 0;
  eid_t max_index = 0;

  if ( !m_storage.get_index_range< Ts... >( min_index, max_index ) ) {
    return;
  }

  for ( eid_t i = min_index; i <= max_index; ++i ) {
    exclude_impl< TsEx... >( i, std::forward< Ft >( func ) );
  }
}

}
