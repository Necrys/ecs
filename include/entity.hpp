#pragma once

namespace ecs {

template < typename T, typename... Ts >
T& entity::add( Ts&&... ts ) {
  return m_owner->components().add_entity_component< T >( m_id, std::forward< Ts >( ts )... );
}

template < typename T >
T* entity::get() {
  return m_owner->components().get_entity_component< T >( m_id );
}

template < typename T >
const T* entity::get() const {
  return m_owner->components().get_entity_component< T >( m_id );
}

template < typename T, typename... Ts >
T& entity::set( Ts&&... ts ) {
  return m_owner->components().set_entity_component< T >( m_id, std::forward< Ts >( ts )... );
}

template < typename T >
void entity::remove() {
  m_owner->components().remove_entity_component< T >( m_id );
}

}
