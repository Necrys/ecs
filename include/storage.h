#pragma once

#include "types.h"
#include "type_enumerator.h"
#include "sparse_vector.h"

#include <type_traits>
#include <map>

namespace ecs {
  struct component_storages {};
  struct component_type_base {};

  template < typename >
  struct component_type: public component_type_base {
    static const size_t id;
  };

  template < typename T >
  const size_t component_type< T >::id = type_collection< component_storages >::type_id< T >();

  class components_storage {
  public:
    ~components_storage();

    /* modify */
    template < typename T, typename... Ts >
    T& add_entity_component( const eid_t id, Ts&&... ts );

    template < typename T, typename... Ts >
    T& set_entity_component( const eid_t id, const T& t );

    template < typename T >
    void remove_entity_component( const eid_t id );

    void remove_all_components( const eid_t id );

    /* access */
    template < typename T >
    T* get_entity_component( const eid_t id );

    template < typename T >
    const T* get_entity_component( const eid_t id ) const;

    template < typename... Cs, typename Ft >
    void join( Ft&& func );

  private:
    template < typename T >
    sparse_vector< T >* get_storage() const;

    template < typename T >
    sparse_vector< T >& get_or_create_storage();

    template < class Ft, class... Rs >
    void join_impl( const eid_t id, Ft&& f, Rs&&... rs );

    template < class T, class... Ts, class Ft, class... Rs >
    void join_impl( const eid_t id, Ft&& f, Rs&&... rs );

    template < class T >
    void get_index_range( eid_t& min_index, eid_t& max_index );

    template < class T, class... Rs >
    typename std::enable_if< sizeof...( Rs ) != 0 >::type get_index_range( eid_t& min_index, eid_t& max_index );

    std::vector< sparse_vector_base* > m_componentStorages;
  };
}

#include "storage.hpp"
