#include "common.h"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

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

}
