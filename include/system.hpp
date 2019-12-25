#pragma once

#include "registry.h"

namespace ecs {

//=============================================================================
//
// system_wrapper
//
//=============================================================================
template < typename T, typename... Args >
system_wrapper< T, Args... >::system_wrapper( Args&& ...args ):
  system_base(),
  m_system( std::forward< Args >( args )... ) {
}

template < typename T, typename... Args >
T& system_wrapper< T, Args... >::get() {
  return m_system;
}

template < typename T, typename... Args >
void system_wrapper< T, Args... >::update() {
  m_system.update();
  m_updated = true;
}

//=============================================================================
//
// system_configure
//
//=============================================================================
template < typename T >
system_configure< T >::system_configure( T& system_wrap, registry& r ):
  m_system( system_wrap ),
  m_registry( r ) {
}

template < typename T >
template < typename... Events >
system_configure< T >& system_configure< T >::handles() {
  m_registry.register_system< typename T::system, Events... >( m_system );
  return *this;
}

template < typename T >
template < typename... Dependencies >
system_configure< T >& system_configure< T >::depends_on() {
  auto sid = type_collection< systems >::type_id< typename T::system >();
  if ( m_registry.m_systemDependenciesMatrix.size() <= sid ) {
    m_registry.m_systemDependenciesMatrix.resize( sid + 1 );
  }

  m_registry.add_system_dependencies< Dependencies... >( sid );

  return *this;
}

}
