// Structural benchmarks: creating and destroying entities, adding and removing
// components, and reusing ids.
//
// These are the operations a game calls whenever the world changes shape - a
// wave spawns, a projectile expires - so their absolute cost matters more than
// their scaling. They are also the operations that used to be quadratic:
// releasing entities walked a growing free list.

#include "harness.hpp"

#include <registry.h>

#include <cstdio>
#include <memory>
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

struct Health {
  int value;
  Health() :
    value( 0 ) {
  }
  explicit Health( const int v ) :
    value( v ) {
  }
};

// Seven components: exercises the loop that removes everything an entity owns.
struct Tag1 {
  int v;
  Tag1() :
    v( 1 ) {
  }
};
struct Tag2 {
  int v;
  Tag2() :
    v( 2 ) {
  }
};
struct Tag3 {
  int v;
  Tag3() :
    v( 3 ) {
  }
};
struct Tag4 {
  int v;
  Tag4() :
    v( 4 ) {
  }
};
struct Tag5 {
  int v;
  Tag5() :
    v( 5 ) {
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
  const bench::FrequencyCheck frequency = bench::begin_frequency_check();

  const size_t budget = bench::memory_budget( machine );
  // Structural operations scale linearly, so a very large count adds no shape to
  // the curve and only makes each point slow.
  const size_t largest = bench::entities_for_budget( budget, 48u, 1048576u );

  std::printf( "# workload\n" );
  std::printf( "#   largest run      : %zu entities\n", largest );
  std::printf( "#\n" );

  bench::RunContext context;
  context.run_id = "structure";
  context.config = machine.arch + "-release";
  context.machine = machine;

  char timestamp[ 64 ] = "unset";
  std::snprintf( timestamp, sizeof( timestamp ), "%s %s", __DATE__, __TIME__ );

  std::vector< bench::Result > results;
  const auto record = [ & ]( bench::Result& result ) {
    bench::print_result( result );
    results.push_back( result );
  };

  // ---------------------------------------------------------------------------
  // Allocate: entities created one after another, nothing destroyed.
  // ---------------------------------------------------------------------------
  for ( const size_t count : bench::size_ladder( largest ) ) {
    auto world = std::make_unique< ecs::registry >();

    bench::Result result;
    result.scenario = "allocate";
    result.components = "-";
    result.entity_count = count;
    result.operations = count;
    result.stats = bench::measure(
      config,
      count,
      [ & ] { world = std::make_unique< ecs::registry >(); },
      [ & ] {
        uint64_t sum = 0;
        for ( size_t i = 0; i < count; ++i ) {
          sum += world->allocate().id();
        }
        bench::mix_into_checksum( sum );
      }
    );
    record( result );
  }

  // ---------------------------------------------------------------------------
  // Deallocate immediately after allocating: the free list stays small.
  // ---------------------------------------------------------------------------
  for ( const size_t count : bench::size_ladder( largest ) ) {
    bench::Result result;
    result.scenario = "alloc_free_pairs";
    result.components = "-";
    result.entity_count = count;
    result.operations = count;
    result.stats = bench::measure(
      config,
      count,
      [] {},
      [ & ] {
        ecs::registry world;
        uint64_t sum = 0;
        for ( size_t i = 0; i < count; ++i ) {
          auto entity = world.allocate();
          sum += entity.id();
          world.deallocate( entity );
        }
        bench::mix_into_checksum( sum );
      }
    );
    record( result );
  }

  // ---------------------------------------------------------------------------
  // Deallocate a full world: the shape that used to be quadratic, because every
  // release searched a list that only grew.
  // ---------------------------------------------------------------------------
  for ( const size_t count : bench::size_ladder( largest ) ) {
    bench::Result result;
    result.scenario = "deallocate_all";
    result.components = "-";
    result.entity_count = count;
    result.operations = count;
    result.stats = bench::measure(
      config,
      count,
      [] {},
      [ & ] {
        ecs::registry world;
        std::vector< ecs::entity > entities;
        entities.reserve( count );
        for ( size_t i = 0; i < count; ++i ) {
          entities.push_back( world.allocate() );
        }
        uint64_t sum = 0;
        for ( auto& entity : entities ) {
          sum += entity.id();
          world.deallocate( entity );
        }
        bench::mix_into_checksum( sum );
      }
    );
    record( result );
  }

  // ---------------------------------------------------------------------------
  // Reuse: ids handed back and taken again, which is what a steady-state game
  // does continuously.
  // ---------------------------------------------------------------------------
  for ( const size_t count : bench::size_ladder( largest ) ) {
    bench::Result result;
    result.scenario = "id_reuse";
    result.components = "-";
    result.entity_count = count;
    result.operations = count;
    result.stats = bench::measure(
      config,
      count,
      [] {},
      [ & ] {
        ecs::registry world;
        std::vector< ecs::entity > entities;
        entities.reserve( count );
        for ( size_t i = 0; i < count; ++i ) {
          entities.push_back( world.allocate() );
        }
        for ( auto& entity : entities ) {
          world.deallocate( entity );
        }

        uint64_t sum = 0;
        for ( size_t i = 0; i < count; ++i ) {
          sum += world.allocate().id();
        }
        bench::mix_into_checksum( sum );
      }
    );
    record( result );
  }

  // ---------------------------------------------------------------------------
  // Component add and remove on an existing entity.
  // ---------------------------------------------------------------------------
  for ( const size_t count : bench::size_ladder( largest ) ) {
    auto world = std::make_unique< ecs::registry >();
    std::vector< ecs::entity > entities;
    entities.reserve( count );
    for ( size_t i = 0; i < count; ++i ) {
      entities.push_back( world->allocate() );
    }

    {
      bench::Result result;
      result.scenario = "add_component";
      result.components = "Position";
      result.entity_count = count;
      result.component_bytes = sizeof( Position );
      result.operations = count;
      result.stats = bench::measure(
        config,
        count,
        [ & ] {
          for ( auto& entity : entities ) {
            entity.remove< Position >();
          }
        },
        [ & ] {
          uint64_t sum = 0;
          for ( auto& entity : entities ) {
            sum += static_cast< uint64_t >( entity.add< Position >( 1.0f, 2.0f ).x );
          }
          bench::mix_into_checksum( sum );
        }
      );
      record( result );
    }

    {
      bench::Result result;
      result.scenario = "remove_component";
      result.components = "Position";
      result.entity_count = count;
      result.component_bytes = sizeof( Position );
      result.operations = count;
      result.stats = bench::measure(
        config,
        count,
        [ & ] {
          for ( auto& entity : entities ) {
            entity.add< Position >( 1.0f, 2.0f );
          }
        },
        [ & ] {
          uint64_t sum = 0;
          for ( auto& entity : entities ) {
            sum += entity.id();
            entity.remove< Position >();
          }
          bench::mix_into_checksum( sum );
        }
      );
      record( result );
    }
  }

  // ---------------------------------------------------------------------------
  // Destroying an entity that owns several components: the release loop has to
  // visit every storage.
  // ---------------------------------------------------------------------------
  {
    // Eight storages are touched per entity, so the point is kept smaller: the
    // cost per unit is what this scenario reports, not the scale.
    const size_t count = bench::entities_for_budget( budget, 64u, 1048576u );
    bench::Result result;
    result.scenario = "deallocate_many_components";
    result.components = "7 components";
    result.entity_count = count;
    result.operations = count;
    result.stats = bench::measure(
      config,
      count,
      [] {},
      [ & ] {
        ecs::registry world;
        std::vector< ecs::entity > entities;
        entities.reserve( count );
        for ( size_t i = 0; i < count; ++i ) {
          auto entity = world.allocate();
          entity.add< Position >( 1.0f, 2.0f );
          entity.add< Velocity >( 3.0f, 4.0f );
          entity.add< Health >( 5 );
          entity.add< Tag1 >();
          entity.add< Tag2 >();
          entity.add< Tag3 >();
          entity.add< Tag4 >();
          entity.add< Tag5 >();
          entities.push_back( entity );
        }

        uint64_t sum = 0;
        for ( auto& entity : entities ) {
          sum += entity.id();
          world.deallocate( entity );
        }
        bench::mix_into_checksum( sum );
      }
    );
    record( result );
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

  bench::write_samples_file( csv_path, context, results );
  bench::end_frequency_check( frequency );

  std::printf( "#\n# checksum %llu\n", static_cast< unsigned long long >( bench::checksum() ) );
  return 0;
}
