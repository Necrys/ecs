#pragma once

namespace ecs {

template < typename E >
event_handler< E >::event_handler( registry& r ):
  m_registry( r ),
  m_current( 0 ) {
}

template < typename E >
template < typename F >
void event_handler< E >::subscribe( F&& f, const size_t sid ) {
  m_callbacks.emplace_back( system_event_handler_callback< E >{ f, sid } );
}

template < typename E >
template < typename... Args >
void event_handler< E >::enqueue( Args&& ...args ) {
  m_events[ ( m_current + 1 ) % 2 ].emplace_back( std::forward< Args >( args )... );
}

template < typename E >
void event_handler< E >::handle_all() {
  m_current = m_current == 0 ? m_current + 1 : 0;

  for ( const auto& e : m_events[ m_current ] ) {
    for ( auto& cb : m_callbacks ) {
      if ( m_registry.is_system_active( cb.sid ) )
        cb.f( e );
    }
  }

  m_events[ m_current ].clear();
}

}
