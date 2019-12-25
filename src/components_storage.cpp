#include "storage.h"

namespace ecs {

components_storage::~components_storage() {
  for ( auto s : m_componentStorages ) {
    if ( s ) {
      delete s;
    }
  }
}

void components_storage::remove_all_components( const eid_t id ) {
  for ( const auto s : m_componentStorages ) {
    if ( s ) {
      s->erase( id );
    }
  }
}

}
