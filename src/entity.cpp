#include "entity.h"

namespace ecs {

entity::entity( const eid_t id, registry* owner ):
  m_owner( owner ),
  m_id( id ) {
}

entity::entity( const entity& e ):
  m_owner( e.m_owner ),
  m_id( e.m_id ) {
}

entity::entity( entity&& e ):
  m_owner( e.m_owner ),
  m_id( e.m_id ) {
}

entity& entity::operator=( const entity& e ) {
  m_owner = e.m_owner;
  m_id = e.m_id;
  return *this;
}

entity& entity::operator=( entity&& e ) {
  m_owner = e.m_owner;
  m_id = e.m_id;
  return *this;
}

eid_t entity::id() const {
  return m_id;
}

registry* entity::owner() {
  return m_owner;
}

}
