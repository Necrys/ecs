#include "storage.h"

namespace ecs {

components_storage::~components_storage() {
  for ( auto* s : m_componentStorages ) {
    // Deleting a null pointer is a no-op, so the check would only hide that.
    delete s;
  }
}

void components_storage::remove_all_components( const eid_t id ) {
  for ( auto* const s : m_componentStorages ) {
    if ( s != nullptr ) {
      s->erase( id );
    }
  }
}

}
