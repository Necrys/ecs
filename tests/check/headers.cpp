// Compiles the public headers as their own translation unit and instantiates the
// template surface, so that a warning inside a header is not hidden until
// somebody happens to use that combination.
//
// This is a build check, not a test: it has no assertions and is not registered
// with CTest. It is compiled as an object library so that it needs neither a
// main() nor a link step, and it runs on every build of the project - not only
// when the tests are enabled, because the headers are the library.
//
// It also feeds ECS_WARNINGS_AS_ERRORS: if this target stops compiling, the
// warning set has found something before a user did.

#include "entity.h"
#include "events.h"
#include "registry.h"
#include "sparse_vector.h"
#include "storage.h"
#include "system.h"
#include "type_enumerator.h"
#include "types.h"

namespace {

struct Position {
  float x, y;
  Position() :
    x( 0.0f ),
    y( 0.0f ) {
  }
  Position( const float _x, const float _y ) :
    x( _x ),
    y( _y ) {
  }
};

struct Velocity {
  float x, y;
  Velocity() :
    x( 0.0f ),
    y( 0.0f ) {
  }
  Velocity( const float _x, const float _y ) :
    x( _x ),
    y( _y ) {
  }
};

struct Tag {};

struct Tick {
  int value;
};

class Counter {
public:
  void update() {
  }
  void handle_event( const Tick& ) {
  }
};

// Every public operation on one component storage, including both selection
// forms and the const/non-const halves of the accessors.
float exercise_storage( ecs::components_storage& storage ) {
  // The default constructor as well as the two-argument one: clang reports a
  // constructor nothing calls under -Wunused-member-function, and a header check
  // that leaves part of the surface uninstantiated is checking less than it looks.
  const Velocity defaulted;

  storage.add_entity_component< Position >( 1, 1.0f, 2.0f );
  storage.add_entity_component< Velocity >( 1, 3.0f, 4.0f );
  storage.set_entity_component< Position >( 1, Position{ 5.0f, 6.0f } );

  float sum = 0.0f;
  storage.join< Position >( [ &sum ]( const ecs::eid_t, const Position& p ) { sum += p.x; } );
  storage.join< Position, Velocity >( [ &sum ]( const ecs::eid_t, const Position& p, const Velocity& v ) {
    sum += p.y + v.x;
  } );
  storage.join< Position >().exclude< Velocity >( [ &sum ]( const ecs::eid_t, const Position& p ) { sum += p.y; } );

  const ecs::components_storage& view = storage;
  if ( const auto* p = view.get_entity_component< Position >( 1 ) ) {
    sum += p->x;
  }
  if ( auto* p = storage.get_entity_component< Position >( 1 ) ) {
    p->y += 1.0f;
  }
  if ( storage.get_storage< Position >() != nullptr ) {
    sum += 1.0f;
  }

  storage.remove_entity_component< Velocity >( 1 );
  storage.remove_all_components( 1 );

  return sum + defaulted.x;
}

// The registry surface: entities, systems, events and the storage view.
float exercise_registry() {
  ecs::registry world;

  const ecs::entity handle = world.allocate();
  world.components().add_entity_component< Position >( handle.id(), 1.0f, 1.0f );
  world.components().add_entity_component< Tag >( handle.id(), Tag{} );

  world.add_system< Counter >().handles< Tick >();
  world.push_event< Tick >( Tick{ 1 } );
  world.update();

  const bool active = world.is_system_active( 0 );
  world.get_system< Counter >();
  world.deallocate( handle );

  return active ? 1.0f : 0.0f;
}

// The container underneath, including the page-level paths the selection code
// walks: insert, lookup, erase and clear.
float exercise_sparse_vector() {
  ecs::sparse_vector< Position > vector;

  vector.insert( 200, Position{ 1.0f, 2.0f } );
  vector.emplace( 300, 3.0f, 4.0f );
  vector.emplace( 64, 5.0f, 6.0f );

  float sum = 0.0f;
  if ( vector.exist( 200 ) ) {
    sum += vector.get_unsafe( 200 ).x;
  }
  vector.each( [ &sum ]( const size_t, const Position& p ) {
    sum += p.y;
    return true;
  } );
  vector.erase( 300 );
  vector.erase( 64 );

  vector.reserve( 1000 );
  vector.clear();

  return sum + static_cast< float >( vector.size() );
}

} // namespace

// Declared before the definition, and exported rather than static, so that the
// translation unit is a real one: an unused static function would only trade
// this warning for another.
int ecs_headers_check();

// Kept as a single exported symbol so the translation unit cannot be discarded
// as empty, which would silently turn the check off.
int ecs_headers_check() {
  ecs::components_storage storage;
  float sum = exercise_storage( storage );
  sum += exercise_registry();
  sum += exercise_sparse_vector();
  return sum > 0.0f ? 1 : 0;
}
