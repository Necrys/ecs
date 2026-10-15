#include "common.h"

#include <catch2/catch_test_macros.hpp>

#include <storage.h>

TEST_CASE( "init" ) {

SECTION( "default" ) {
  ecs::components_storage s;

  bool called = false;
  s.join< Position >( [ & ]( const ecs::eid_t, const Position& ) {
    called = true;
  } );

  REQUIRE( called == false );
}

}

TEST_CASE( "modify" ) {

SECTION( "insert" ) {
  ecs::components_storage s;
  Position p{ 3.1415f, 2.7182f };

  const auto& v = s.add_entity_component< Position >( 200, p );

  const auto vv = s.get_entity_component< Position >( 200 );

  REQUIRE( v.x == p.x );
  REQUIRE( v.y == p.y );
  REQUIRE( vv->x == p.x );
  REQUIRE( vv->y == p.y );
  REQUIRE( &v == vv );
}

SECTION( "set" ) {
  ecs::components_storage s;
  Position p{ 1.0f, 2.0f };
  Position p2{ 3.1415f, 2.7182f };
  s.add_entity_component< Position >( 200, 1.0f, 2.0f );

  const auto& v = s.set_entity_component< Position >( 200, p2 );

  const auto vv = s.get_entity_component< Position >( 200 );

  REQUIRE( v.x == p2.x );
  REQUIRE( v.y == p2.y );
  REQUIRE( vv->x == p2.x );
  REQUIRE( vv->y == p2.y );
  REQUIRE( &v == vv );
}

SECTION( "remove_all" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 3.1415f, 2.7182f );
  s.add_entity_component< Velocity >( 200, 1.4142f, 10.0f );

  s.remove_all_components( 200 );

  const auto pc = s.get_entity_component< Position >( 200 );
  const auto vc = s.get_entity_component< Velocity >( 200 );
  bool called = false;
  s.join< Position >( [ & ]( const ecs::eid_t, const Position& ) {
    called = true;
  } );
  s.join< Velocity >( [ & ]( const ecs::eid_t, const Velocity& ) {
    called = true;
  } );

  REQUIRE( called == false );
  REQUIRE( pc == nullptr );
  REQUIRE( vc == nullptr );
}

SECTION( "remove" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 3.1415f, 2.7182f );

  s.remove_entity_component< Position >( 200 );

  const auto pc = s.get_entity_component< Position >( 200 );
  bool called = false;
  s.join< Position >( [ & ]( const ecs::eid_t, const Position& ) {
    called = true;
  } );

  REQUIRE( called == false );
  REQUIRE( pc == nullptr );
}

SECTION( "remove_noexist" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 201, 3.1415f, 2.7182f );

  s.remove_entity_component< Position >( 200 );

  const auto pc = s.get_entity_component< Position >( 200 );

  REQUIRE( pc == nullptr );
}

}

TEST_CASE( "access" ) {

SECTION( "get" ) {
  ecs::components_storage s;
  const auto& ref = s.add_entity_component< Position >( 200, 3.1415f, 2.7182f );
  
  auto pc = s.get_entity_component< Position >( 200 );
  REQUIRE( *pc == ref );
}

SECTION( "get_const" ) {
  ecs::components_storage s;
  const auto& ref = s.add_entity_component< Position >( 200, 3.1415f, 2.7182f );
  
  const auto pc = s.get_entity_component< Position >( 200 );
  REQUIRE( *pc == ref );
}

SECTION( "get_not_exist" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 201, 3.1415f, 2.7182f );
  
  auto pc = s.get_entity_component< Position >( 200 );
  REQUIRE( pc == nullptr );
}

SECTION( "get_not_exist_const" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 201, 3.1415f, 2.7182f );

  const auto pc = s.get_entity_component< Position >( 200 );
  REQUIRE( pc == nullptr );
}

SECTION( "get_not_exist_2" ) {
  ecs::components_storage s;

  auto pc = s.get_entity_component< Position >( 200 );
  REQUIRE( pc == nullptr );
}

SECTION( "join" ) {
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

SECTION( "join_no_storage" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 3.1415f, 2.7182f );
  s.add_entity_component< Position >( 201, 1.0f, 2.0f );

  size_t called = 0;
  s.join< Position, Velocity >( [ & ]( const ecs::eid_t, const Position&, const Velocity& ) {
    ++called;
  } );

  REQUIRE( called == 0 );
}

SECTION( "join_exclude" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 3.1415f, 2.7182f );
  s.add_entity_component< Position >( 201, 1.0f, 2.0f );
  s.add_entity_component< Position >( 202, 3.1415f, 2.7182f );
  s.add_entity_component< Position >( 203, 3.1415f, 2.7182f );
  
  s.add_entity_component< Velocity >( 201, 1.4142f, 9.81f );
  s.add_entity_component< Velocity >( 203, 1.4142f, 9.81f );

  size_t called = 0;
  s
    .join< Position >()
    .exclude< Velocity >( [ & ]( const ecs::eid_t, const Position& ) {
    ++called;
  } );

  REQUIRE( called == 2 );
}


}

// Regression coverage for the join/exclude bounds handling.
//
// index_range() reports "no index" with a size_t sentinel, which does not
// survive the narrowing to eid_t used by the iteration bounds. The sentinel used
// to turn into UINT32_MAX, the early-out check stopped matching, and the loop
// `for ( eid_t i = UINT32_MAX; i <= 0; ++i )` wrapped around and ran for
// billions of iterations. A storage that exists but is empty is a normal state:
// it happens as soon as every entity with that component is destroyed.
TEST_CASE( "join_bounds" ) {

SECTION( "component_never_registered" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 1.0f, 2.0f );
  s.add_entity_component< Position >( 201, 3.0f, 4.0f );

  size_t called = 0;
  s.join< Position, Velocity >( [ & ]( const ecs::eid_t, const Position&, const Velocity& ) {
    ++called;
  } );

  REQUIRE( called == 0 );
}

SECTION( "component_storage_exists_but_is_empty" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 1.0f, 2.0f );
  s.add_entity_component< Velocity >( 5, 1.0f, 1.0f );

  // Emptying the storage deletes its page, so index_range() reports no index
  // while the storage itself still exists. This is the case that used to hang.
  s.remove_entity_component< Velocity >( 5 );

  size_t called = 0;
  s.join< Position, Velocity >( [ & ]( const ecs::eid_t, const Position&, const Velocity& ) {
    ++called;
  } );

  REQUIRE( called == 0 );
}

SECTION( "empty_storage_does_not_disturb_a_valid_join" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 1.0f, 2.0f );
  s.add_entity_component< Position >( 201, 3.0f, 4.0f );
  s.add_entity_component< Tag >( 200, 5 );

  s.add_entity_component< Velocity >( 7, 1.0f, 1.0f );
  s.remove_entity_component< Velocity >( 7 );

  size_t called = 0;
  s.join< Position, Tag >( [ & ]( const ecs::eid_t, const Position&, const Tag& ) {
    ++called;
  } );

  REQUIRE( called == 1 );
}

SECTION( "exclude_with_empty_storage_terminates" ) {
  ecs::components_storage s;
  s.add_entity_component< Position >( 200, 1.0f, 2.0f );

  s.add_entity_component< Velocity >( 7, 1.0f, 1.0f );
  s.remove_entity_component< Velocity >( 7 );

  size_t called = 0;
  s.join< Position >().exclude< Velocity >( [ & ]( const ecs::eid_t, const Position& ) {
    ++called;
  } );

  // Nothing is excluded because the Velocity storage is empty, so the single
  // Position entity must be visited.
  REQUIRE( called == 1 );
}

SECTION( "exclude_on_empty_base_storage" ) {
  ecs::components_storage s;

  size_t called = 0;
  s.join< Position >().exclude< Velocity >( [ & ]( const ecs::eid_t, const Position& ) {
    ++called;
  } );

  REQUIRE( called == 0 );
}

}