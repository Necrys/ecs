#include "system.h"

namespace ecs {

system_base::system_base():
  m_updated( false ),
  m_enabled( true ) {
}

bool system_base::updated() {
  return m_updated;
}

void system_base::reset_update() {
  m_updated = false;
}

bool system_base::enabled() const {
  return m_enabled;
}

void system_base::enable() {
  m_enabled = true;
}

void system_base::disable() {
  m_enabled = false;
}

void system_base::clear_dependencies() {
  m_dependencies.clear();
}

void system_base::add_dependency( std::shared_ptr< system_base > dep ) {
  m_dependencies.push_back( dep );
}

bool system_base::ready() const {
  bool result = true;
  for ( const auto& d : m_dependencies ) {
    if ( !d ) {
      continue;
    }

    if ( d->enabled() && !d->updated() ) {
      result = false;
      break;
    }

    if ( !d->enabled() && !d->ready() ) {
      result = false;
      break;
    }
  }

  return result;
}

}
