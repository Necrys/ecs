#include "common.h"

#include <catch2/catch_test_macros.hpp>

#include <registry.h>

#include <vector>

namespace {

// Never added as a system anywhere: it exists so that a dependency can name a type
// the registry has no slot for. It deliberately has no system interface - a type
// that is only ever named as a dependency is not a system, and giving it an update()
// nothing calls would be exactly the dead code clang reports.
struct UnregisteredSystem {};

}

TEST_CASE( "entity", "[unit]" ) {
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

TEST_CASE( "entity_components", "[integration]" ) {
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

// Regression coverage for the free-list lookup: it used to walk a growing list
// on every deallocate, which made releasing N entities quadratic.
TEST_CASE( "mass_deallocate_and_reallocate", "[integration][regression][d4]" ) {
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
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): ids holds count entries
    REQUIRE( r.allocate().id() == ids[ static_cast< size_t >( i ) ] );
  }
}

TEST_CASE( "double_release_is_ignored", "[integration]" ) {
  ecs::registry r;

  const auto first = r.allocate();
  const auto second = r.allocate();

  r.deallocate( first );
  r.deallocate( first ); // must not queue the id twice
  r.deallocate( second );

  REQUIRE( r.allocate().id() == first.id() );
  REQUIRE( r.allocate().id() == second.id() );
}

TEST_CASE( "released_entities_lose_their_components", "[integration]" ) {
  ecs::registry r;

  auto e = r.allocate();
  e.add< Position >( 1.0f, 2.0f );

  r.deallocate( e );

  const ecs::components_storage* cs = &r.components();
  REQUIRE( cs->get_entity_component< Position >( e.id() ) == nullptr );
}

TEST_CASE( "systems", "[integration]" ) {
  SECTION( "register" ) {
    ecs::registry r;
    r.add_system< System >();
  }
}

// Regression coverage for the dependency tree. A dependency naming a type that
// was never added as a system has an id the vector of systems never grew to, so
// building the tree used to index past its end. The dependency is ignored, and
// the update must complete.
TEST_CASE( "system_dependency_on_unregistered_system", "[integration][regression]" ) {
  SECTION( "ignored" ) {
    ecs::registry r;
    r.add_system< System >().depends_on< UnregisteredSystem >();

    const auto sid = ecs::type_collection< ecs::systems >::type_id< System >();

    r.update();

    REQUIRE( r.is_system_active( sid ) == true );
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
TEST_CASE( "registered_system_is_active", "[integration][regression][d2]" ) {
  ecs::registry r;
  r.add_system< System >();

  const auto sid = ecs::type_collection< ecs::systems >::type_id< System >();

  REQUIRE( r.is_system_active( sid ) == true );
}

TEST_CASE( "disabled_system_is_inactive", "[integration][regression][d2]" ) {
  ecs::registry r;
  r.add_system< System >();

  const auto sid = ecs::type_collection< ecs::systems >::type_id< System >();

  r.get_system< System >().disable();

  REQUIRE( r.is_system_active( sid ) == false );
}

TEST_CASE( "system_after_the_end_is_inactive", "[integration][regression][d2]" ) {
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

TEST_CASE( "events", "[integration]" ) {
  SECTION( "delivered_to_subscriber" ) {
    ecs::registry r;
    r.add_system< CountingSystem >().handles< Event >();

    // The counter is static, so the test asserts on the change it caused rather
    // than on an absolute value: sections do not run in a promised order.
    const int before = CountingSystem::received;

    r.push_event< Event >( Event{} );
    r.update();

    REQUIRE( CountingSystem::received == before + 1 );
  }

  // Nothing subscribes, so there is no queue to put the event in. Documented
  // behaviour today is to drop it silently - see D11 in the plan's registry for
  // making that visible - and the test pins the part that matters meanwhile:
  // publishing without a subscriber must not crash or reach anybody later.
  SECTION( "no_subscriber_is_not_an_error" ) {
    ecs::registry r;
    const int before = CountingSystem::received;

    r.push_event< Event >( Event{} );
    r.update();

    REQUIRE( CountingSystem::received == before );
  }
}

// Coverage for the dependency tree itself: a system that depends on another must
// run after it, which is the observable effect of the graph the registry builds.
TEST_CASE( "system_dependencies", "[integration]" ) {
  SECTION( "dependency_runs_first" ) {
    ecs::registry r;
    r.add_system< TaggedSystem< 1 > >().depends_on< TaggedSystem< 2 > >();
    r.add_system< TaggedSystem< 2 > >();
    updates().clear();

    r.update();

    INFO( updates().describe() );
    REQUIRE( updates().matches( { 2, 1 } ) );
  }

  // A disabled dependency never updates, so the dependent system must not wait
  // for it: the wait would deadlock the frame instead of skipping the system.
  SECTION( "disabled_dependency_does_not_block" ) {
    ecs::registry r;
    r.add_system< TaggedSystem< 1 > >().depends_on< TaggedSystem< 2 > >();
    r.add_system< TaggedSystem< 2 > >().depends_on< TaggedSystem< 3 > >();
    r.add_system< TaggedSystem< 3 > >();
    updates().clear();

    r.get_system< TaggedSystem< 2 > >().disable();
    r.update();

    // 2 is disabled, so it neither waits for 3 nor blocks 1: 3 runs because it
    // is ready, then 1 runs because a disabled dependency counts as satisfied.
    INFO( updates().describe() );
    REQUIRE( updates().matches( { 3, 1 } ) );
  }

  SECTION( "re_enabled_dependency_runs_again" ) {
    ecs::registry r;
    r.add_system< TaggedSystem< 1 > >().depends_on< TaggedSystem< 2 > >();
    r.add_system< TaggedSystem< 2 > >();

    r.get_system< TaggedSystem< 2 > >().disable();
    updates().clear();
    r.update();
    INFO( updates().describe() );
    REQUIRE( updates().matches( { 1 } ) );

    r.get_system< TaggedSystem< 2 > >().enable();
    updates().clear();
    r.update();

    INFO( updates().describe() );
    REQUIRE( updates().matches( { 2, 1 } ) );
  }

  // A dependency whose id is inside the vector but whose slot is empty: the type
  // was given an id by the type collection, yet was never added as a system to
  // this registry. The dependency is skipped rather than dereferenced.
  SECTION( "empty_dependency_slot_is_skipped" ) {
    // Ids are handed out on first use and shared across the whole binary, so the
    // order the test needs cannot be assumed: the empty slot's id is taken first,
    // then a second type's id, which is necessarily higher, and only then is the
    // vector grown past the first one.
    const auto empty_sid = ecs::type_collection< ecs::systems >::type_id< UnregisteredSystem >();
    const auto registered_sid = ecs::type_collection< ecs::systems >::type_id< TaggedSystem< 4 > >();
    REQUIRE( empty_sid < registered_sid );

    ecs::registry r;
    r.add_system< TaggedSystem< 4 > >();
    r.add_system< TaggedSystem< 1 > >().depends_on< UnregisteredSystem >();
    updates().clear();

    r.update();

    // An empty slot imposes no order: the dependency is skipped, so the system
    // that named it is free to run in its own place - here slot 0 comes before
    // slot 4.
    INFO( updates().describe() );
    REQUIRE( updates().matches( { 1, 4 } ) );
  }
}

// A handle is copyable and movable, and every copy refers to the same entity in
// the same registry. The operations themselves are defaulted, so what is worth
// pinning is the behaviour a user sees.
TEST_CASE( "entity_handles", "[unit]" ) {
  SECTION( "copies_refer_to_the_same_entity" ) {
    ecs::registry r;
    auto original = r.allocate();
    original.add< Position >( 1.0f, 2.0f );

    const auto copy = original;
    const auto assigned = original;
    auto move_source = original;
    const auto moved = std::move( move_source );
    auto assign_source = original;
    auto move_assigned = r.allocate();
    move_assigned = std::move( assign_source );

    REQUIRE( copy.id() == original.id() );
    REQUIRE( assigned.id() == original.id() );
    REQUIRE( moved.id() == original.id() );
    REQUIRE( move_assigned.id() == original.id() );

    REQUIRE( copy.owner() == original.owner() );
    REQUIRE( moved.owner() == original.owner() );

    // The same entity, not a copy of its components.
    REQUIRE( copy.get< Position >() == original.get< Position >() );
  }

  SECTION( "self_assignment_keeps_the_handle" ) {
    ecs::registry r;
    auto handle = r.allocate();
    const auto id = handle.id();

    // Through an alias on purpose: `handle = handle` is what the test means, and
    // it is also the pattern -Wself-assign-overloaded exists to catch, which the
    // build turns into an error.
    auto& alias = handle;
    handle = alias;

    REQUIRE( handle.id() == id );
    REQUIRE( handle.owner() == &r );
  }
}

// Exhausting the id space is the only way to reach the throw in allocate(), and
// it is reachable: eid_t is 32 bits, so the counter hands out 2^32 - 1 ids before
// it runs out, at a few nanoseconds per call. Measured on the development machine:
// 3.63 ns per allocate() in Release, about 15 s for the full space; MSVC Debug
// takes 65 s, and the boards will be slower still.
//
// It costs no memory - measured, not assumed: the working set moves by less than
// a tenth of a megabyte while the whole space is handed out, because allocate()
// only advances a counter. Nothing may be remembered per entity here, though: a
// vector of the handles would be 8 bytes each, which is 34 GB for the full space.
//
// The case runs in the ordinary set, tagged [slow] so that it can be left out when
// the minute matters - `ctest -LE slow` is the tight loop, and it is the only way
// to skip it. Being in the ordinary set is what keeps library line coverage at
// 100%: a flag that hides the throw would also hide the fact that nothing tests it.
TEST_CASE( "entity_id_space_exhaustion", "[unit][slow]" ) {
  ecs::registry r;

  const auto limit = static_cast< size_t >( ecs::entity::bad_id );
  for ( size_t i = 0; i < limit; ++i ) {
    r.allocate();
  }

  REQUIRE_THROWS_AS( r.allocate(), std::runtime_error );
}

int CountingSystem::received = 0;
