#include "common.h"

#include <catch2/catch_test_macros.hpp>

#include <registry.h>

#include <vector>

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

TEST_CASE( "entity_bulk_reuse" ) {

// Regression coverage for the free-list lookup: it used to walk a growing list
// on every deallocate, which made releasing N entities quadratic.
SECTION( "mass_deallocate_and_reallocate" ) {
  const int count = 5000;

  ecs::registry r;
  std::vector< ecs::eid_t > ids;
  ids.reserve( count );

  for ( int i = 0; i < count; ++i ) {
    ids.push_back( r.allocate().id() );
  }

  for ( const auto id : ids ) {
    r.deallocate( id );
  }

  // Releasing everything must hand the ids back in FIFO order, because the
  // allocation order is part of the observable behaviour.
  for ( int i = 0; i < count; ++i ) {
    REQUIRE( r.allocate().id() == ids[ static_cast< size_t >( i ) ] );
  }
}

SECTION( "double_release_is_ignored" ) {
  ecs::registry r;

  const auto first = r.allocate();
  const auto second = r.allocate();

  r.deallocate( first );
  r.deallocate( first );   // must not queue the id twice
  r.deallocate( second );

  REQUIRE( r.allocate().id() == first.id() );
  REQUIRE( r.allocate().id() == second.id() );
}

SECTION( "released_components_are_cleared" ) {
  ecs::registry r;

  auto e = r.allocate();
  e.add< Position >( 1.0f, 2.0f );

  r.deallocate( e );

  const ecs::components_storage* cs = &r.components();
  REQUIRE( cs->get_entity_component< Position >( e.id() ) == nullptr );
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

  // Every id at or above the vector size used to be read out of bounds: the
  // check was `size() < sid`, so `sid == size()` took the branch and
  // dereferenced m_systems[size()]. The size is not part of the public surface,
  // so the test asserts the observable contract instead: ids far beyond any
  // registered system are inactive, and asking does not read past the end.
  REQUIRE( r.is_system_active( 64 ) == false );
  REQUIRE( r.is_system_active( 1024 ) == false );
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