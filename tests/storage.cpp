#include "common.h"

#include <catch2/catch_test_macros.hpp>

#include <storage.h>

// Purpose tags double as CTest labels (see tests/CMakeLists.txt):
//   [unit]        one module's contract;
//   [integration] several modules together;
//   [regression]  written because a defect existed, with the defect's number.

TEST_CASE( "init", "[unit]" ) {
  ecs::components_storage s;

  bool called = false;
  s.join< Position >( [ & ]( const ecs::eid_t, const Position& ) { called = true; } );

  REQUIRE( called == false );
}

TEST_CASE( "insert_returns_the_stored_value", "[unit]" ) {
  ecs::components_storage s;
  Position p{ 3.1415f, 2.7182f };

  const auto& v = s.add_entity_component< Position >( 200, p );

  auto* const vv = s.get_entity_component< Position >( 200 );

  REQUIRE( v.x == p.x );
  REQUIRE( v.y == p.y );
  REQUIRE( vv->x == p.x );
  REQUIRE( vv->y == p.y );
  REQUIRE( &v == vv );
}

TEST_CASE( "set_replaces_the_value", "[unit]" ) {
  ecs::components_storage s;
  Position p2{ 3.1415f, 2.7182f };
  s.add_entity_component< Position >( 200, 1.0f, 2.0f );

  const auto& v = s.set_entity_component< Position >( 200, p2 );

  auto* const vv = s.get_entity_component< Position >( 200 );

  REQUIRE( v.x == p2.x );
  REQUIRE( v.y == p2.y );
  REQUIRE( vv->x == p2.x );
  REQUIRE( vv->y == p2.y );
  REQUIRE( &v == vv );
}

TEST_CASE( "remove_all_components_empties_every_storage", "[unit]" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 3.1415f, 2.7182f );
  s.add_entity_component< Velocity >( 200, 1.4142f, 10.0f );

  s.remove_all_components( 200 );

  auto* const pc = s.get_entity_component< Position >( 200 );
  auto* const vc = s.get_entity_component< Velocity >( 200 );
  bool called = false;
  s.join< Position >( [ & ]( const ecs::eid_t, const Position& ) { called = true; } );
  s.join< Velocity >( [ & ]( const ecs::eid_t, const Velocity& ) { called = true; } );

  REQUIRE( called == false );
  REQUIRE( pc == nullptr );
  REQUIRE( vc == nullptr );
}

TEST_CASE( "remove_component", "[unit]" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 3.1415f, 2.7182f );

  s.remove_entity_component< Position >( 200 );

  auto* const pc = s.get_entity_component< Position >( 200 );
  bool called = false;
  s.join< Position >( [ & ]( const ecs::eid_t, const Position& ) { called = true; } );

  REQUIRE( called == false );
  REQUIRE( pc == nullptr );
}

TEST_CASE( "remove_missing_component_is_harmless", "[unit]" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 201, 3.1415f, 2.7182f );

  s.remove_entity_component< Position >( 200 );

  auto* const pc = s.get_entity_component< Position >( 200 );

  REQUIRE( pc == nullptr );
}

TEST_CASE( "get_returns_the_stored_component", "[unit]" ) {
  ecs::components_storage s;
  const auto& ref = s.add_entity_component< Position >( 200, 3.1415f, 2.7182f );

  auto* pc = s.get_entity_component< Position >( 200 );
  REQUIRE( *pc == ref );
}

// The const overload is a separate function, so it needs a const object to be
// called at all: reading through a non-const storage tests the other one.
TEST_CASE( "get_const_returns_the_stored_component", "[unit]" ) {
  ecs::components_storage s;
  const auto& ref = s.add_entity_component< Position >( 200, 3.1415f, 2.7182f );

  const ecs::components_storage& view = s;
  const auto* const pc = view.get_entity_component< Position >( 200 );

  REQUIRE( pc != nullptr );
  REQUIRE( *pc == ref );
}

TEST_CASE( "get_missing_component_returns_null", "[unit]" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 201, 3.1415f, 2.7182f );

  auto* pc = s.get_entity_component< Position >( 200 );
  REQUIRE( pc == nullptr );
}

TEST_CASE( "get_const_missing_component_returns_null", "[unit]" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 201, 3.1415f, 2.7182f );

  const ecs::components_storage& view = s;
  const auto* const pc = view.get_entity_component< Position >( 200 );

  REQUIRE( pc == nullptr );
}

TEST_CASE( "get_const_from_a_storage_that_was_never_used", "[unit]" ) {
  ecs::components_storage s;

  const ecs::components_storage& view = s;
  const auto* const pc = view.get_entity_component< Position >( 200 );

  REQUIRE( pc == nullptr );
}

TEST_CASE( "get_missing_component_without_any_storage_returns_null", "[unit]" ) {
  ecs::components_storage s;

  auto* pc = s.get_entity_component< Position >( 200 );
  REQUIRE( pc == nullptr );
}

TEST_CASE( "join_visits_the_entities_carrying_every_component", "[unit]" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 3.1415f, 2.7182f );
  const auto& pref = s.add_entity_component< Position >( 201, 1.0f, 2.0f );
  const auto& vref = s.add_entity_component< Velocity >( 201, 1.4142f, 9.81f );
  s.add_entity_component< Velocity >( 202, 20.0f, 3.0f );

  size_t called = 0;
  s.join< Position, Velocity >( [ & ]( const ecs::eid_t id, const Position& p, const Velocity& v ) {
    ++called;
    REQUIRE( id == 201 );
    REQUIRE( p == pref );
    REQUIRE( v == vref );
  } );

  REQUIRE( called == 1 );
}

TEST_CASE( "join_with_a_component_nobody_carries_visits_nothing", "[unit]" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 3.1415f, 2.7182f );
  s.add_entity_component< Position >( 201, 1.0f, 2.0f );

  size_t called = 0;
  s.join< Position, Velocity >( [ & ]( const ecs::eid_t, const Position&, const Velocity& ) { ++called; } );

  REQUIRE( called == 0 );
}

TEST_CASE( "exclude_skips_the_entities_carrying_the_excluded_component", "[unit]" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 3.1415f, 2.7182f );
  s.add_entity_component< Position >( 201, 1.0f, 2.0f );
  s.add_entity_component< Position >( 202, 3.1415f, 2.7182f );
  s.add_entity_component< Position >( 203, 3.1415f, 2.7182f );

  s.add_entity_component< Velocity >( 201, 1.4142f, 9.81f );
  s.add_entity_component< Velocity >( 203, 1.4142f, 9.81f );

  size_t called = 0;
  s.join< Position >().exclude< Velocity >( [ & ]( const ecs::eid_t, const Position& ) { ++called; } );

  REQUIRE( called == 2 );
}

// Regression coverage for the join/exclude bounds handling.
//
// index_range() reports "no index" with a size_t sentinel, which does not
// survive the narrowing to eid_t used by the iteration bounds. The sentinel used
// to turn into UINT32_MAX, the early-out check stopped matching, and the loop
// `for ( eid_t i = UINT32_MAX; i <= 0; ++i )` wrapped around and ran for
// billions of iterations. A storage that exists but is empty is a normal state:
// it happens as soon as every entity with that component is destroyed.
TEST_CASE( "join_with_a_component_never_registered", "[unit][regression][d1]" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 1.0f, 2.0f );
  s.add_entity_component< Position >( 201, 3.0f, 4.0f );

  size_t called = 0;
  s.join< Position, Velocity >( [ & ]( const ecs::eid_t, const Position&, const Velocity& ) { ++called; } );

  REQUIRE( called == 0 );
}

TEST_CASE( "join_with_a_storage_that_exists_but_is_empty", "[unit][regression][d1]" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 1.0f, 2.0f );
  s.add_entity_component< Velocity >( 5, 1.0f, 1.0f );

  // Emptying the storage deletes its page, so index_range() reports no index
  // while the storage itself still exists. This is the case that used to hang.
  s.remove_entity_component< Velocity >( 5 );

  size_t called = 0;
  s.join< Position, Velocity >( [ & ]( const ecs::eid_t, const Position&, const Velocity& ) { ++called; } );

  REQUIRE( called == 0 );
}

TEST_CASE( "an_empty_storage_does_not_disturb_a_valid_join", "[unit][regression][d1]" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 1.0f, 2.0f );
  s.add_entity_component< Position >( 201, 3.0f, 4.0f );
  s.add_entity_component< Tag >( 200, 5 );

  s.add_entity_component< Velocity >( 7, 1.0f, 1.0f );
  s.remove_entity_component< Velocity >( 7 );

  size_t called = 0;
  s.join< Position, Tag >( [ & ]( const ecs::eid_t, const Position&, const Tag& ) { ++called; } );

  REQUIRE( called == 1 );
}

TEST_CASE( "exclude_with_an_empty_storage_terminates", "[unit][regression][d1]" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 1.0f, 2.0f );

  s.add_entity_component< Velocity >( 7, 1.0f, 1.0f );
  s.remove_entity_component< Velocity >( 7 );

  size_t called = 0;
  s.join< Position >().exclude< Velocity >( [ & ]( const ecs::eid_t, const Position& ) { ++called; } );

  // Nothing is excluded because the Velocity storage is empty, so the single
  // Position entity must be visited.
  REQUIRE( called == 1 );
}

TEST_CASE( "exclude_on_an_empty_base_storage", "[unit][regression][d1]" ) {
  ecs::components_storage s;

  size_t called = 0;
  s.join< Position >().exclude< Velocity >( [ & ]( const ecs::eid_t, const Position& ) { ++called; } );

  REQUIRE( called == 0 );
}
