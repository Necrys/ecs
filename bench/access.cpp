// Point access: reading one component of a known entity.
//
// This is the other half of what the storage design claims - "the id is the
// index" - so the traversal being fast must not come at the cost of a lookup
// that degrades with the size of the world. Access is measured in random id
// order, which is the case that defeats prefetching and shows what a lookup
// really costs.

#include "harness.hpp"

#include <registry.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace {

struct Position {
  float x, y;
  Position() :
    x( 0.0f ),
    y( 0.0f ) {
  }
  Position( const float a, const float b ) :
    x( a ),
    y( b ) {
  }
};

struct Velocity {
  float x, y;
  Velocity() :
    x( 0.0f ),
    y( 0.0f ) {
  }
  Velocity( const float a, const float b ) :
    x( a ),
    y( b ) {
  }
};

// Present in the world type set but carried by no entity, so that the miss
// path can be measured against the same world as the hit path.
struct Health {
  int value;
  Health() :
    value( 0 ) {
  }
};

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

  const size_t budget = bench::memory_budget( machine );
  const size_t largest = bench::entities_for_budget( budget, 24u );

  // One probe per world size, matching the traversal ladder so that the two
  // numbers can be read against each other.
  const size_t probes = 1000000u;

  std::printf( "# workload\n" );
  std::printf( "#   largest world    : %zu entities\n", largest );
  std::printf( "#   probes per point : %zu random lookups\n", probes );
  std::printf( "#\n" );

  bench::RunContext context;
  context.run_id = "access";
  context.config = machine.arch + "-release";
  context.machine = machine;

  char timestamp[ 64 ] = "unset";
  std::snprintf( timestamp, sizeof( timestamp ), "%s %s", __DATE__, __TIME__ );

  std::vector< bench::Result > results;
  const auto record = [ & ]( bench::Result& result ) {
    bench::print_result( result );
    results.push_back( result );
  };

  for ( const size_t count : bench::size_ladder( largest ) ) {
    auto world = std::make_unique< ecs::registry >();
    for ( size_t i = 0; i < count; ++i ) {
      auto entity = world->allocate();
      entity.add< Position >( static_cast< float >( i ), 0.0f );
      entity.add< Velocity >( 1.0f, 1.0f );
    }

    // A fixed shuffle of the ids, generated once so that every sample reads the
    // same order. Re-generating it inside the measurement would time the random
    // number generator rather than the lookup.
    std::vector< ecs::eid_t > order( count );
    for ( size_t i = 0; i < count; ++i ) {
      order[ i ] = static_cast< ecs::eid_t >( i );
    }
    std::mt19937 generator( 20260115 );
    std::shuffle( order.begin(), order.end(), generator );

    const size_t lookups = std::min( probes, count * 4u );

    {
      bench::Result result;
      result.scenario = "get_component";
      result.components = "Position";
      result.entity_count = count;
      result.component_bytes = sizeof( Position );
      result.operations = lookups;
      result.stats = bench::measure(
        config,
        lookups,
        [] {},
        [ & ] {
          uint64_t sum = 0;
          for ( size_t i = 0; i < lookups; ++i ) {
            const auto* component = world->components().get_entity_component< Position >( order[ i % count ] );
            sum += static_cast< uint64_t >( component->x );
          }
          bench::mix_into_checksum( sum );
        }
      );
      record( result );
    }

    {
      // The same lookup for a component type no entity carries: the miss path.
      // It must not be cheaper than a hit for the wrong reason, and it is the
      // path that runs whenever a query asks for something the world does not
      // have.
      bench::Result result;
      result.scenario = "get_component_missing";
      result.components = "Health";
      result.entity_count = count;
      result.operations = lookups;
      result.stats = bench::measure(
        config,
        lookups,
        [] {},
        [ & ] {
          uint64_t sum = 0;
          for ( size_t i = 0; i < lookups; ++i ) {
            const auto* component = world->components().get_entity_component< Health >( order[ i % count ] );
            sum += component ? 1u : 0u;
          }
          bench::mix_into_checksum( sum );
        }
      );
      record( result );
    }
  }

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
    }
  }

  std::printf( "#\n# checksum %llu\n", static_cast< unsigned long long >( bench::checksum() ) );
  return 0;
}
