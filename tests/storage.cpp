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
  const auto& pref = s.add_entity_component< Position >( 201, 1.0f, 2.0f );

  size_t called = 0;
  s.join< Position, Velocity >( [ & ]( const ecs::eid_t id, const Position& p, const Velocity& v ) {
    ++called;
  } );

  REQUIRE( called == 0 );
}


}