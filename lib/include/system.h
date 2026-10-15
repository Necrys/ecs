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

  // The rest of the set, declared for the same reason as in the other bases: the
  // class says how it is copied and moved as well as how it dies. Systems are held
  // by shared_ptr, so this is about being able to say it, not about being copied.
  system_base( const system_base& ) = default;
  system_base( system_base&& ) = default;
  system_base& operator=( const system_base& ) = default;
  system_base& operator=( system_base&& ) = default;

  [[nodiscard]] bool updated() const;

  virtual void update() = 0;
  void reset_update();

  [[nodiscard]] bool enabled() const;

  void enable();
  void disable();

  void clear_dependencies();

  // By reference: the dependency is copied into the list anyway, and taking the
  // shared_ptr by value would bump the count on every registration.
  void add_dependency( const std::shared_ptr< system_base >& dep );

  [[nodiscard]] bool ready() const;

protected:
  bool m_updated{ false };
  bool m_enabled{ true };

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
