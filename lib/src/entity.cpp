#include "entity.h"

namespace ecs {

entity::entity( const eid_t id, registry* owner ) :
  m_owner( owner ),
  m_id( id ) {
}

// A handle is two trivial members: the defaulted operations are member-wise and
// therefore identical to the explicit ones, and they also answer the
// self-assignment question by construction. The move operations are noexcept
// because nothing here can throw, and a container that knows it will move rather
// than copy.
entity::entity( const entity& ) = default;

entity::entity( entity&& ) noexcept = default;

entity& entity::operator=( const entity& ) = default;

entity& entity::operator=( entity&& ) noexcept = default;

eid_t entity::id() const {
  return m_id;
}

registry* entity::owner() const {
  return m_owner;
}

}
