#pragma once

#include <utility>

#include "selection.hpp"

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
T& components_storage::add_entity_component( eid_t id, Ts&&... ts ) {
  auto& storage = get_or_create_storage< T >();
  return storage.emplace( id, std::forward< Ts >( ts )... );
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
[[nodiscard]] const T* components_storage::get_entity_component( const eid_t id ) const {
  const auto storage = get_storage< T >();
  if ( storage && storage->exist( id ) ) {
    return &storage->get_unsafe( id );
  }

  return nullptr;
}

template < typename T, typename... Ts >
T& components_storage::set_entity_component( eid_t id, const T& t ) {
  auto& storage = get_or_create_storage< T >();
  return storage[ id ] = t;
}

template < typename T >
void components_storage::remove_entity_component( const eid_t id ) {
  auto& storage = get_or_create_storage< T >();
  storage.erase( id );
}

//=============================================================================
//
// Selection
//
// A set of component types is selected in two steps: pick the storage holding
// the fewest elements, then walk its live elements and query the other storages
// by id. Two consequences matter:
//
//   * the cost follows the number of candidate entities rather than the spread
//     of their ids, so an id space that has grown wide - or been emptied out -
//     no longer slows the walk down;
//   * the storage walked is one of the required types, so no time is spent on
//     entities that cannot match.
//
// The walk itself lives in selection.hpp, where the run-time choice of storage
// is tied back to its compile-time type.
//=============================================================================

template < class... Cs >
bool components_storage::exists_all( const eid_t id ) const {
  bool result = true;

  ( void )std::initializer_list< int >{ (
    result = result &&
             [ & ] {
               const auto storage = get_storage< Cs >();
               return storage != nullptr && storage->exist( id );
             }(),
    0
  )... };

  return result;
}

template < typename... Cs, typename Ft >
void components_storage::join( Ft f ) {
  // The components are already in hand for the walked storage, so they are passed
  // through directly rather than looked up again.
  detail::walk_required< components_storage, Cs... >( *this, [ & ]( const eid_t id, const Cs&... cs ) {
    f( id, cs... );
  } );
}

template < typename... Ts >
components_storage::join_exclude_wrapper< Ts... > components_storage::join() {
  return components_storage::join_exclude_wrapper< Ts... >( *this );
}

template < typename... Ts >
components_storage::join_exclude_wrapper< Ts... >::join_exclude_wrapper( components_storage& storage ) :
  m_storage( storage ) {
}

template < typename... Ts >
template < typename... TsEx, typename Ft >
void components_storage::join_exclude_wrapper< Ts... >::exclude( Ft func ) {
  // Candidates come from the joined components; each is then rejected if any of
  // the excluded components is present. A component no entity carries rejects
  // nothing, which falls out of the test instead of needing a special case.
  detail::walk_required< components_storage, Ts... >( m_storage, [ & ]( const eid_t id, const Ts&... cs ) {
    if ( m_storage.exists_all< TsEx... >( id ) ) {
      return;
    }

    func( id, cs... );
  } );
}

}
