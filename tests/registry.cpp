#include "common.h"

#include <catch2/catch_test_macros.hpp>

#include <registry.h>

TEST_CASE( "entity" ) {

SECTION( "allocate" ) {
  ecs::registry r;

  auto e = r.allocate();

  REQUIRE( e.id() == 0 );
  REQUIRE( e.owner() == &r );
}

SECTION( "allocate_with_pooled_id" ) {
  ecs::registry r;

  r.allocate();
  auto e1 = r.allocate(); // id = 1
  auto e2 = r.allocate(); // id = 2
  REQUIRE( e1.id() == 1 );
  REQUIRE( e2.id() == 2 );
  r.deallocate( e1 );
  r.deallocate( e2 );

  auto e = r.allocate();
  REQUIRE( e.id() == 1 );
}

SECTION( "allocate_with_pooled_id" ) {
  ecs::registry r;

  r.allocate();
  auto e1 = r.allocate(); // id = 1
  auto e2 = r.allocate(); // id = 2
  REQUIRE( e1.id() == 1 );
  REQUIRE( e2.id() == 2 );
  r.deallocate( e1 );
  r.deallocate( e2 );

  auto e = r.allocate();
  REQUIRE( e.id() == 1 );
}

}

TEST_CASE( "entity_components" ) {

SECTION( "add" ) {
  ecs::registry r;

  auto e = r.allocate();

  REQUIRE( e.get< Position >() == nullptr );

  const auto& pc = e.add< Position >( 3.1415f, 2.7182f );

  REQUIRE( e.get< Position >() != nullptr );
  REQUIRE( *e.get< Position >() == pc );
}

SECTION( "set" ) {
  ecs::registry r;
  Position pref( 1.4142f, 9.81f );

  auto e = r.allocate();

  REQUIRE( e.get< Position >() == nullptr );

  const auto& pc = e.add< Position >( 3.1415f, 2.7182f );

  REQUIRE( e.get< Position >() != nullptr );
  REQUIRE( *e.get< Position >() == pc );

  e.set< Position >( pref );

  REQUIRE( e.get< Position >() != nullptr );
  REQUIRE( *e.get< Position >() == pref );
}

SECTION( "remove" ) {
  ecs::registry r;
  auto e = r.allocate();
  e.add< Position >( 3.1415f, 2.7182f );

  e.remove< Position >();

  REQUIRE( e.get< Position >() == nullptr );
}

}

TEST_CASE( "systems" ) {

SECTION( "register" ) {

  ecs::registry r;
  r.add_system< System >();

}

}

// Regression coverage for is_system_active(): the bounds check used to be
// `size() < sid` with the element access inside the branch, so it read past the
// end of the vector and reported every registered system as inactive, which
// meant no event ever reached a subscriber.
//
// The checks below go through type_id() rather than hard-coded positions: a
// system id depends on how many system types were registered before it, so
// assuming a particular index makes the test depend on unrelated test cases.
TEST_CASE( "system_activity" ) {

SECTION( "registered_system_is_active" ) {
  ecs::registry r;
  r.add_system< System >();

  const auto sid = ecs::type_collection< ecs::systems >::type_id< System >();

  REQUIRE( r.is_system_active( sid ) == true );
}

SECTION( "disabled_system_is_inactive" ) {
  ecs::registry r;
  r.add_system< System >();

  const auto sid = ecs::type_collection< ecs::systems >::type_id< System >();

  r.get_system< System >().disable();

  REQUIRE( r.is_system_active( sid ) == false );
}

SECTION( "system_after_the_end_is_inactive" ) {
  ecs::registry r;
  r.add_system< System >();

  // Any id at or beyond the vector size used to be read out of bounds:
  // sid == size() took the branch and dereferenced m_systems[size()].
  //
  // CountingSystem is the highest system type id in this binary, so its id plus
  // one is exactly size() here, and anything beyond it is safely outside.
  const auto past_the_end = ecs::type_collection< ecs::systems >::type_id< CountingSystem >() + 1;

  REQUIRE( r.is_system_active( past_the_end ) == false );
  REQUIRE( r.is_system_active( past_the_end + 5 ) == false );
}

}

TEST_CASE( "events" ) {

SECTION( "delivered_to_subscriber" ) {
  ecs::registry r;
  r.add_system< CountingSystem >().handles< Event >();

  r.push_event< Event >( Event{} );
  r.update();

  REQUIRE( CountingSystem::received == 1 );
}

}

int CountingSystem::received = 0;