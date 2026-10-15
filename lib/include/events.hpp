#pragma once

#include <utility>

namespace ecs {

template < typename E >
event_handler< E >::event_handler( registry& r ) :
  m_registry( r ) {
}

template < typename E >
template < typename F >
void event_handler< E >::subscribe( F&& f, size_t sid ) {
  // Forwarded, not copied: the callback is stored, so an rvalue can be moved into
  // the std::function rather than copied into it.
  m_callbacks.emplace_back( system_event_handler_callback< E >{ std::forward< F >( f ), sid } );
}

template < typename E >
template < typename... Args >
void event_handler< E >::enqueue( Args&&... args ) {
  m_events[ ( m_current + 1 ) % 2 ].emplace_back( std::forward< Args >( args )... );
}

template < typename E >
void event_handler< E >::handle_all() {
  m_current = m_current == 0 ? m_current + 1 : 0;

  for ( const auto& e : m_events[ m_current ] ) {
    for ( auto& cb : m_callbacks ) {
      if ( detail::system_is_active( m_registry, cb.sid ) ) {
        cb.f( e );
      }
    }
  }

  m_events[ m_current ].clear();
}

}
