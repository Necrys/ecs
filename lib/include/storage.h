#pragma once

#include "types.h"
#include "type_enumerator.h"
#include "sparse_vector.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace ecs {
struct component_storages {};
struct component_type_base {};

template < typename >
struct component_type : public component_type_base {
  static const size_t id;
};

template < typename T >
const size_t component_type< T >::id = type_collection< component_storages >::type_id< T >();

class components_storage;

class components_storage {
  // Presents one selection step: the components to walk, and the components to
  // reject. Returned by join(), so that the two read as one expression.
  template < typename... Ts >
  class join_exclude_wrapper {
  public:
    explicit join_exclude_wrapper( components_storage& storage );

    template < typename... TsEx, typename Ft >
    void exclude( Ft func );

  private:
    components_storage& m_storage;
  };

public:
  components_storage() = default;
  ~components_storage();

  // Owns the component storages behind the pointers it hands out, so a copy would
  // be two owners of them. Deleted explicitly: the destructor is declared, and the
  // class should say what that means for copying rather than leave it implicit.
  components_storage( const components_storage& ) = delete;
  components_storage( components_storage&& ) = delete;
  components_storage& operator=( const components_storage& ) = delete;
  components_storage& operator=( components_storage&& ) = delete;

  /* modify */
  template < typename T, typename... Ts >
  T& add_entity_component( eid_t id, Ts&&... ts );

  template < typename T, typename... Ts >
  T& set_entity_component( eid_t id, const T& t );

  template < typename T >
  void remove_entity_component( eid_t id );

  void remove_all_components( eid_t id );

  /* access */
  template < typename T >
  T* get_entity_component( eid_t id );

  template < typename T >
  const T* get_entity_component( eid_t id ) const;

  // The typed view of one component's storage, or null when the type has never
  // been used. Public because a selection has to reach it from outside the
  // class, and because it is the honest description of what the container
  // holds: one storage per component type.
  template < typename T >
  sparse_vector< T >* get_storage() const;

  /* selection */
  template < typename... Cs, typename Ft >
  void join( Ft func );

  template < typename... Ts >
  join_exclude_wrapper< Ts... > join();

private:
  template < typename T >
  sparse_vector< T >& get_or_create_storage();

  // True when every listed component is present on the id.
  template < class... Cs >
  [[nodiscard]] bool exists_all( eid_t id ) const;

  std::vector< sparse_vector_base* > m_componentStorages;
};
}

#include "storage.hpp"
