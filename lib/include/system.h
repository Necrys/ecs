#pragma once

#include "type_enumerator.h"

#include <vector>
#include <memory>

namespace ecs {

struct systems {};
class registry;

//=============================================================================
//
// system_base
//
//=============================================================================
class system_base {
public:
  system_base();

  // Out of line, in lib/src/system_base.cpp, for the same reason as the other
  // polymorphic bases: the vtable gets one home instead of a weak copy per
  // translation unit.
  virtual ~system_base();

  bool updated() const;

  virtual void update() = 0;
  void reset_update();

  bool enabled() const;

  void enable();
  void disable();

  void clear_dependencies();

  // By reference: the dependency is copied into the list anyway, and taking the
  // shared_ptr by value would bump the count on every registration.
  void add_dependency( const std::shared_ptr< system_base >& dep );

  bool ready() const;

protected:
  bool m_updated;
  bool m_enabled;

  std::vector< std::shared_ptr< system_base > > m_dependencies;
};

//=============================================================================
//
// system_wrapper
//
//=============================================================================
template < typename T, typename... Args >
class system_wrapper : public system_base {
public:
  using system = T;

  system_wrapper( Args&&... args );

  T& get();

  void update() override;

private:
  T m_system;
};

//=============================================================================
//
// system_configure
//
//=============================================================================
template < typename T >
class system_configure {
public:
  system_configure( T& system_wrap, registry& r );

  template < typename... Events >
  system_configure& handles();

  template < typename... Dependencies >
  system_configure& depends_on();

private:
  T& m_system;
  registry& m_registry;
};
}

#include "system.hpp"
