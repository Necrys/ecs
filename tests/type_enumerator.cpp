#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <type_enumerator.h>

TEST_CASE( "init" ) {

SECTION( "default" ) {
  struct component_storages {};
  struct system_storages {};

  const auto c1t1 = ecs::type_collection < component_storages >::type_id< int >();
  const auto c1t2 = ecs::type_collection < component_storages >::type_id< float >();
  const auto c2t1 = ecs::type_collection < system_storages >::type_id< float >();
  const auto c2t2 = ecs::type_collection < system_storages >::type_id< int >();

  REQUIRE( c1t1 == 0 );
  REQUIRE( c1t2 == 1 );
  REQUIRE( c2t1 == 0 );
  REQUIRE( c2t2 == 1 );
}

}
