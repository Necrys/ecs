#include "common.h"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

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