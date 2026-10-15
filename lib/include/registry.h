#pragma once

#include "storage.h"

#include <list>
#include <set>
#include <memory>

#include <deque>
#include <unordered_set>

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
  registry& operator=( const registry& ) = delete;
  registry& operator=( registry&& ) = delete;

  // Declared because the four above are: the class says how it dies as well as how
  // it refuses to be copied.
  ~registry() = default;

  /* entity management */
  entity allocate();
  void deallocate( const entity& e );
  void deallocate( eid_t id );

  components_storage& components();

  /* systems management */
  template < typename S, typename... Args >
  system_configure< system_wrapper< S > > add_system( Args&&... args );

  template < typename S >
  system_wrapper< S >& get_system();

  template < typename E, typename... Args >
  void push_event( Args&&... args );

  void update();

  [[nodiscard]] bool is_system_active( size_t sid ) const;

private:
  template < typename S, typename E >
  void register_system( system_wrapper< S >& system );

  template < typename S, typename E, typename... Rs >
  std::enable_if_t< ( sizeof...( Rs ) > 0 ) > register_system( system_wrapper< S >& system );

  template < typename T >
  void add_system_dependencies( size_t sid );

  template < typename T, typename... Rs >
  std::enable_if_t< ( sizeof...( Rs ) > 0 ) > add_system_dependencies( size_t sid );

  bool rebuild_system_dependency_tree();

  template < typename E >
  event_handler< E >& get_or_create_handler();

  /* entities */
  // A set answers "is this id already free" in constant time; the queue keeps
  // ids in the order they were released so allocation stays FIFO.
  std::unordered_set< eid_t > m_freeEntityIds;
  std::deque< eid_t > m_freeEntityIdQueue;
  components_storage m_components;
  eid_t m_entityIdCounter{ 0 };

  /* systems */
  std::vector< std::shared_ptr< system_base > > m_systems;
  std::vector< std::set< size_t > > m_systemDependenciesMatrix;
  bool m_rebuildDependencyTree{ false };

  std::vector< std::unique_ptr< event_handler_base > > m_handlers;
};
}

#include "registry.hpp"
