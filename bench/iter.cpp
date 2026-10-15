// Iteration benchmarks: the component walk is the operation the whole library is
// built around, so this is the set that future work has to keep honest.
//
// Three questions are asked separately:
//   * what does one component cost (the floor);
//   * what does each additional component cost (the join overhead);
//   * how does the cost depend on the spread of entity ids rather than on the
//     number of live entities (the defect the iteration rewrite has to remove).

#include "harness.hpp"

#include <registry.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

struct Position {
  float x, y;
  Position( const float a, const float b ) :
    x( a ),
    y( b ) {
  }
};

struct Velocity {
  float x, y;
  Velocity( const float a, const float b ) :
    x( a ),
    y( b ) {
  }
};

struct Health {
  int value;
  explicit Health( const int v ) :
    value( v ) {
  }
};

// 64 bytes: exactly one cache line, so the payload stops being a rounding
// error and the traversal is no longer purely about metadata.
struct Transform {
  float m[ 16 ];
  Transform() {
    for ( int i = 0; i < 16; ++i ) {
      m[ i ] = 0.0f;
    }
  }
};

// 256 bytes: several cache lines per component.
struct Blob {
  float data[ 64 ];
  Blob() {
    for ( int i = 0; i < 64; ++i ) {
      data[ i ] = 0.0f;
    }
  }
};

// `count` entities taking ids 0..count-1, each carrying Position and Velocity,
// plus Health when asked.
std::unique_ptr< ecs::registry > make_dense( const size_t count, const bool with_health = false ) {
  auto world = std::make_unique< ecs::registry >();
  for ( size_t i = 0; i < count; ++i ) {
    auto entity = world->allocate();
    entity.add< Position >( static_cast< float >( i ), static_cast< float >( i ) );
    entity.add< Velocity >( 1.0f, 1.0f );
    if ( with_health ) {
      entity.add< Health >( static_cast< int >( i ) );
    }
  }
  return world;
}

// Live entities spread over `span` ids. The live count is fixed while the span
// grows, which is the shape that exposes a traversal cost driven by the largest
// id rather than by the entity count.
//
// Ids are written straight into the storages: pages nobody occupies are never
// allocated, so a wide span costs the index vector and nothing else. This is
// what a sparse container is for, and it keeps the scenario about iteration
// rather than about allocation.
std::unique_ptr< ecs::registry > make_sparse( const size_t live, const size_t span ) {
  auto world = std::make_unique< ecs::registry >();
  const size_t stride = span / live;

  for ( size_t i = 0; i < live; ++i ) {
    const auto id = static_cast< ecs::eid_t >( i * stride );
    world->components().add_entity_component< Position >( id, static_cast< float >( id ), 0.0f );
    world->components().add_entity_component< Velocity >( id, 1.0f, 1.0f );
  }

  return world;
}

// Fills `count` entities and then destroys all but every `keep_every`-th one.
// This is the shape a world takes after a wave of enemies dies: few live
// entities, a large id span.
std::unique_ptr< ecs::registry > make_thinned( const size_t count, const size_t keep_every ) {
  auto world = std::make_unique< ecs::registry >();
  std::vector< ecs::entity > entities;
  entities.reserve( count );

  for ( size_t i = 0; i < count; ++i ) {
    auto entity = world->allocate();
    entity.add< Position >( static_cast< float >( i ), 0.0f );
    entity.add< Velocity >( 1.0f, 1.0f );
    entities.push_back( entity );
  }

  for ( size_t i = 0; i < count; ++i ) {
    if ( i % keep_every != 0 ) {
      world->deallocate( entities[ i ] );
    }
  }

  return world;
}

// One-component traversal over an already built world.
template < typename T >
void walk_one( ecs::registry& world ) {
  uint64_t sum = 0;
  world.components().join< T >( [ & ]( const ecs::eid_t, const T& component ) {
    sum += static_cast< uint64_t >( component.x );
  } );
  bench::mix_into_checksum( sum );
}

} // namespace

int main( int argc, char** argv ) {
  const bench::CommandLine line = bench::parse_command_line( argc, argv );
  const bench::RunConfig config = line.config;
  const std::string csv_path = line.csv_path;

  const bench::MachineInfo machine = bench::detect_machine();
  bench::print_machine( machine );

  // Pinning comes after the machine report, which describes the whole system
  // rather than the subset this process is allowed on.
  const bench::AffinityState scheduling = bench::apply_scheduling( line );
  ( void )scheduling;
  const bench::FrequencyCheck frequency = bench::begin_frequency_check();

  const size_t budget = bench::memory_budget( machine );
  const size_t cache = bench::cache_budget( machine );

  // Position (8) + Velocity (8) plus the page metadata kept per slot: two size_t
  // for the index and the reverse index. That is what has to fit, not just the
  // component payload.
  const size_t bytes_per_entity = 24u;
  const size_t dense_largest = bench::entities_for_budget( budget, bytes_per_entity );

  std::printf( "# workload\n" );
  std::printf( "#   memory budget    : %s\n", bench::human_bytes( budget ).c_str() );
  std::printf( "#   cache budget     : %s\n", bench::human_bytes( cache ).c_str() );
  std::printf(
    "#   largest dense run: %zu entities (~%s)\n",
    dense_largest,
    bench::human_bytes( dense_largest * bytes_per_entity ).c_str()
  );
  std::printf( "#\n" );

  bench::RunContext context;
  context.run_id = "iter";
  context.config = machine.arch + "-release";
  context.machine = machine;

  const std::string timestamp = bench::run_timestamp();

  std::vector< bench::Result > results;

  // One untimed traversal to make the pages resident. A sparse layout touches a
  // new page every stride, so the very first walk over a wide span is dominated
  // by page faults; without this the first timed sample carries them and the
  // spread of the point stops meaning anything.
  const auto touch = []( ecs::registry& world ) {
    uint64_t sum = 0;
    world.components().join< Position >( [ & ]( const ecs::eid_t, const Position& p ) {
      sum += static_cast< uint64_t >( p.x );
    } );
    bench::mix_into_checksum( sum );
  };

  const auto record = [ & ]( bench::Result& result ) {
    bench::print_result( result );
    bench::print_spread_hint( result.stats.spread_pct );
    results.push_back( result );
  };

  // ---------------------------------------------------------------------------
  // One component: the floor.
  // ---------------------------------------------------------------------------
  for ( const size_t count : bench::size_ladder( dense_largest ) ) {
    auto world = make_dense( count );

    bench::Result result;
    result.scenario = "join1";
    result.components = "Position";
    result.entity_count = count;
    result.component_bytes = sizeof( Position );
    result.operations = count;
    result.stats = bench::measure( config, count, [] {}, [ & ] { walk_one< Position >( *world ); } );
    record( result );
  }

  // ---------------------------------------------------------------------------
  // Several components: the join overhead.
  // ---------------------------------------------------------------------------
  for ( const size_t count : bench::size_ladder( dense_largest ) ) {
    auto world = make_dense( count, true );

    {
      bench::Result result;
      result.scenario = "join2";
      result.components = "Position,Velocity";
      result.entity_count = count;
      result.component_bytes = sizeof( Position ) + sizeof( Velocity );
      result.operations = count;
      result.stats = bench::measure(
        config,
        count,
        [] {},
        [ & ] {
          uint64_t sum = 0;
          world->components().join< Position, Velocity >( [ & ](
                                                            const ecs::eid_t, const Position& p, const Velocity& v
                                                          ) { sum += static_cast< uint64_t >( p.x + v.x ); } );
          bench::mix_into_checksum( sum );
        }
      );
      record( result );
    }

    {
      bench::Result result;
      result.scenario = "join3";
      result.components = "Position,Velocity,Health";
      result.entity_count = count;
      result.component_bytes = sizeof( Position ) + sizeof( Velocity ) + sizeof( Health );
      result.operations = count;
      result.stats = bench::measure(
        config,
        count,
        [] {},
        [ & ] {
          uint64_t sum = 0;
          world->components().join< Position, Velocity, Health >(
            [ & ]( const ecs::eid_t, const Position& p, const Velocity& v, const Health& h ) {
              sum += static_cast< uint64_t >( p.x + v.x ) + static_cast< uint64_t >( h.value );
            }
          );
          bench::mix_into_checksum( sum );
        }
      );
      record( result );
    }
  }

  // ---------------------------------------------------------------------------
  // Component payload: does the traversal cost follow the data?
  // ---------------------------------------------------------------------------
  {
    const size_t count = bench::entities_for_budget( budget, sizeof( Transform ) + 24u );
    auto world = std::make_unique< ecs::registry >();
    for ( size_t i = 0; i < count; ++i ) {
      world->allocate().add< Transform >();
    }

    bench::Result result;
    result.scenario = "join1";
    result.components = "Transform";
    result.notes = std::to_string( sizeof( Transform ) ) + " byte component";
    result.entity_count = count;
    result.component_bytes = sizeof( Transform );
    result.operations = count;
    result.stats = bench::measure(
      config,
      count,
      [] {},
      [ & ] {
        uint64_t sum = 0;
        world->components().join< Transform >( [ & ]( const ecs::eid_t, const Transform& t ) {
          sum += static_cast< uint64_t >( t.m[ 0 ] );
        } );
        bench::mix_into_checksum( sum );
      }
    );
    record( result );
  }

  {
    const size_t count = bench::entities_for_budget( budget, sizeof( Blob ) + 24u );
    auto world = std::make_unique< ecs::registry >();
    for ( size_t i = 0; i < count; ++i ) {
      world->allocate().add< Blob >();
    }

    bench::Result result;
    result.scenario = "join1";
    result.components = "Blob";
    result.notes = std::to_string( sizeof( Blob ) ) + " byte component";
    result.entity_count = count;
    result.component_bytes = sizeof( Blob );
    result.operations = count;
    result.stats = bench::measure(
      config,
      count,
      [] {},
      [ & ] {
        uint64_t sum = 0;
        world->components().join< Blob >( [ & ]( const ecs::eid_t, const Blob& b ) {
          sum += static_cast< uint64_t >( b.data[ 0 ] );
        } );
        bench::mix_into_checksum( sum );
      }
    );
    record( result );
  }

  // ---------------------------------------------------------------------------
  // Id spread: the same amount of live work over a growing id range.
  // ---------------------------------------------------------------------------
  std::printf( "#\n" );
  {
    const size_t live = 100000u;
    // The widest span is bounded by the index vector: one pointer per 64 slots.
    // Written without std::min and std::max because mixing size_t with unsigned
    // literals makes template deduction ambiguous.
    size_t widest = budget / sizeof( void* ) * 64u / 2u;
    if ( widest > 100000000u ) {
      widest = 100000000u;
    }
    if ( widest < 1000000u ) {
      widest = 1000000u;
    }

    for ( size_t span = 100000u; span <= widest; span *= 10u ) {
      auto world = make_sparse( live, span );
      touch( *world );

      bench::Result result;
      result.scenario = "join_sparse";
      result.components = "Position";
      result.live_count = live;
      result.id_span = span;
      result.component_bytes = sizeof( Position );
      result.operations = live;
      result.notes = "density 1:" + std::to_string( span / live );
      result.stats = bench::measure( config, live, [] {}, [ & ] { walk_one< Position >( *world ); } );
      record( result );
    }
  }

  // ---------------------------------------------------------------------------
  // A world that filled up and then emptied out.
  // ---------------------------------------------------------------------------
  {
    const size_t created = std::min< size_t >( 1000000u, std::max< size_t >( 100000u, budget / 48u ) );
    const size_t keep_every = 100u;
    auto world = make_thinned( created, keep_every );
    touch( *world );

    bench::Result result;
    result.scenario = "join_thinned";
    result.components = "Position";
    result.entity_count = created;
    result.live_count = created / keep_every;
    result.id_span = created;
    result.component_bytes = sizeof( Position );
    result.operations = result.live_count;
    result.notes = "99% destroyed, ids left wide";
    result.stats = bench::measure( config, result.live_count, [] {}, [ & ] { walk_one< Position >( *world ); } );
    record( result );
  }

  // ---------------------------------------------------------------------------
  // Output
  // ---------------------------------------------------------------------------
  // The CSV path is given on the command line so that run data lands wherever the
  // caller keeps it. Nothing is written into the repository by default: the
  // measurements belong with the run history, not with the code.
  if ( !csv_path.empty() ) {
    const bool needs_header = !bench::file_exists( csv_path );
    if ( std::FILE* out = bench::open_for_append( csv_path ) ) {
      if ( needs_header ) {
        bench::print_csv_header( out );
      }
      for ( const auto& result : results ) {
        bench::print_csv_row( out, context, result, timestamp );
      }
      std::fclose( out );
      std::printf( "#\n# appended %zu rows to %s\n", results.size(), csv_path.c_str() );
    } else {
      std::printf( "#\n# could not open %s for writing\n", csv_path.c_str() );
    }
  }

  bench::write_samples_file( csv_path, context, results );
  bench::end_frequency_check( frequency );

  std::printf( "#\n# checksum %llu\n", static_cast< unsigned long long >( bench::checksum() ) );
  return 0;
}
