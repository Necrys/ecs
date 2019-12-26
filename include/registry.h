#pragma once

#include "storage.h"

#include <list>
#include <set>
#include <memory>

namespace ecs {
  class entity;

  class system_base;

  template < typename S, typename... Args >
  class system_wrapper;

  template < typename T >
  class system_configure;

  class event_handler_base;

  template < typename E >
  class event_handler;

  class registry {
    template < typename T >
    friend class system_configure;

  public:
    registry();

    registry( const registry& ) = delete;
    registry( registry&& ) = delete;
    registry& operator= ( const registry& ) = delete;
    registry& operator= ( registry&& ) = delete;

    /* entity management */
    entity allocate();
    void deallocate( const entity& e );
    void deallocate( const eid_t id );

    components_storage& components();

    /* systems management */
    template < typename S, typename... Args >
    system_configure< system_wrapper< S > > add_system( Args&& ...args );

    template < typename S >
    system_wrapper< S >& get_system();

    template < typename E, typename... Args >
    void push_event( Args&& ...args );

    void update();

    bool is_system_active( const size_t sid ) const;

  private:
    template < typename S, typename E >
    void register_system( system_wrapper< S >& system );

    template < typename S, typename E, typename... Rs >
    typename std::enable_if< ( sizeof...( Rs ) > 0 ) >::type register_system( system_wrapper< S >& system );

    template < typename T >
    void add_system_dependencies( const size_t sys_id );

    template < typename T, typename... Rs >
    typename std::enable_if< ( sizeof...( Rs ) > 0 ) >::type add_system_dependencies( const size_t sys_id );

    bool rebuild_system_dependency_tree();

    template < typename E >
    event_handler< E >& get_or_create_handler();

    /* entities */
    std::list< eid_t > m_freeEntityIds;
    components_storage m_components;
    eid_t              m_entityIdCounter;

    /* systems */
    std::vector< std::shared_ptr< system_base > > m_systems;
    std::vector< std::set< size_t > >             m_systemDependenciesMatrix;
    bool                                          m_rebuildDependencyTree;

    std::vector< std::unique_ptr< event_handler_base > > m_handlers;
  };
}

#include "registry.hpp"
