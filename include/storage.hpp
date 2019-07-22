#pragma once

#include <functional>

namespace ecs {

template < typename T >
sparse_vector< T >& components_storage::get_or_create_storage() {
  const auto idx = component_type< T >::id;
  auto it = m_componentStorages.find( idx );
  if ( it == m_componentStorages.end() ) {
    it = m_componentStorages.emplace( idx, new sparse_vector< T >() ).first;
  }

  return static_cast< sparse_vector< T >& >( *( it->second ) );
}

template < typename T >
sparse_vector< T >* components_storage::get_storage() const {
  const auto idx = component_type< T >::id;
  auto it = m_componentStorages.find( idx );
  if ( it == m_componentStorages.end() ) {
    return nullptr;
  }

  return static_cast< sparse_vector< T >* >( it->second );
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
  if ( storage->exist( id ) ) {
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
  eid_t min_index( std::numeric_limits< eid_t >::max() ), max_index( 0 );
  get_index_range< Ts... >( min_index, max_index );

  for ( eid_t i = min_index; i < max_index; ++i ) {
    join_impl< Ts... >( i, std::forward< Ft >( f ) );
  }
}

template < class T >
void components_storage::get_index_range( eid_t& min_index, eid_t& max_index ) {
  const auto storage = get_storage< T >();
  if ( storage ) {
    const auto range = storage->index_range();
    min_index = min_index > range.first ? range.first : min_index;
    max_index = max_index < range.second ? range.second : max_index;
  }
}

template < class T, class... Rs,
  class = typename std::enable_if< sizeof...( Rs ) != 0 >::type >
void components_storage::get_index_range( eid_t& min_index, eid_t& max_index ) {
  const auto storage = get_storage< T >();
  if ( storage ) {
    const auto range = storage->index_range();
    min_index = min_index > range.first ? range.first : min_index;
    max_index = max_index < range.second ? range.second : max_index;
  }

  get_index_range< Rs... >( min_index, max_index );
}

components_storage::~components_storage() {
  for ( const auto& s : m_componentStorages ) {
    delete s.second;
  }
}

void components_storage::remove_all_components( const eid_t id ) {
  for ( const auto storage : m_componentStorages ) {
    storage.second->erase( id );
  }
}

}
