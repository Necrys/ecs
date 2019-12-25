#include "registry.h"
#include "entity.h"
#include "system.h"
#include "events.h"

#include <exception>
#include <memory>

namespace ecs {

template < typename S, typename... Args >
system_configure< system_wrapper< S > > registry::add_system( Args&& ...args ) {
  const auto sid = type_collection< systems >::type_id< S >();
  if ( m_systems.size() <= sid ) {
    m_systems.resize( sid + 1 );
  }

  if ( !m_systems[ sid ] ) {
    m_systems[ sid ] = std::make_shared< system_wrapper< S, Args... > >( std::forward< Args >( args )... );
  }

  return system_configure< system_wrapper< S > >( static_cast< system_wrapper< S >& >( *m_systems[ sid ] ), *this );
}

template < typename S >
system_wrapper< S >& registry::get_system() {
  const auto sid = type_collection< systems >::type_id< S >();
  return static_cast< system_wrapper< S >& >( *m_systems[ sid ] );
}

template < typename S, typename E >
void registry::register_system( system_wrapper< S >& system ) {
  const auto sid = type_collection< systems >::type_id< S >();
  auto& h = get_or_create_handler< E >();
  h.subscribe( [ &system ]( const E& e ){
    system.get().handle_event( e );
  }, sid );
}

template < typename S, typename E, typename... Rs,
  class = typename std::enable_if< ( sizeof...( Rs ) > 0 ) >::type >
void registry::register_system( system_wrapper< S >& system ) {
  const auto sid = type_collection< systems >::type_id< S >();
  auto& h = get_or_create_handler< E >();
  h.subscribe( [ &system ]( const E& e ){
    system.get().handle_event( e );
  }, sid );

  register_system< S, Rs... >( system );
}

template < typename S >
void registry::add_system_dependencies( const size_t sid ) {
  const auto dep_sid = type_collection< systems >::type_id< S >();
  m_systemDependenciesMatrix[ sid ].emplace( dep_sid );
  m_rebuildDependencyTree = true;
}

template < typename S, typename... Rs,
  class = typename std::enable_if< ( sizeof...( Rs ) > 0 ) >::type >
void registry::add_system_dependencies( const size_t sid ) {
  const auto dep_sid = type_collection< systems >::type_id< S >();
  m_systemDependenciesMatrix[ sid ].emplace( dep_sid );
  add_system_dependencies< Rs... >( sid );
}

template < typename E >
event_handler< E >& registry::get_or_create_handler() {
  const auto eid = type_collection< events >::type_id< E >();
  if ( m_handlers.size() <= eid ) {
    m_handlers.resize( eid + 1 );
  }

  if ( !m_handlers[ eid ] ) {
    m_handlers[ eid ] = std::unique_ptr< event_handler< E > >( new event_handler< E >( *this ) );
  }

  return static_cast< event_handler< E >& >( *m_handlers[ eid ] );
}

template < typename E, typename... Args >
void registry::push_event( Args&& ...args ) {
  auto eid = type_collection< events >::type_id< E >();
  if ( m_handlers.size() <= eid || !m_handlers[ eid ] ) {
    return;
  }

  static_cast< event_handler< E >& >( *m_handlers[ eid ] ).enqueue( std::forward< Args >( args )... );
}

}
