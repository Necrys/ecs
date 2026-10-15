#include <catch2/catch_test_macros.hpp>

#include <sparse_vector.h>

#include <map>
#include <utility>
#include <vector>

namespace {

// Counts live instances so tests can prove that elements are destroyed when a
// page or the whole vector goes away. Without that, components with a
// non-trivial destructor leak their resources.
struct Counted {
  static int live;

  int value;

  explicit Counted( const int v ) :
    value( v ) {
    ++live;
  }

  Counted( const Counted& other ) :
    value( other.value ) {
    ++live;
  }

  Counted( Counted&& other ) noexcept :
    value( other.value ) {
    ++live;
  }

  Counted& operator=( const Counted& ) = default;
  Counted& operator=( Counted&& ) noexcept = default;

  ~Counted() {
    --live;
  }
};

int Counted::live = 0;

}

TEST_CASE( "init" ) {
  SECTION( "default" ) {
    ecs::sparse_vector< int > v;

    auto range = v.index_range();
    REQUIRE( range.first == v.bad_index );
    REQUIRE( range.second == v.bad_index );
  }
}

TEST_CASE( "modify" ) {
  SECTION( "insert" ) {
    ecs::sparse_vector< int > v;

    int val( 666 );
    auto& res = v.insert( 0, val );
    REQUIRE( res == val );

    auto range = v.index_range();
    REQUIRE( range.first == 0 );
    REQUIRE( range.second == 0 );

    REQUIRE( v.exist( 0 ) == true );
  }

  SECTION( "emplace" ) {
    ecs::sparse_vector< int > v;

    auto& res = v.emplace( 0, 666 );
    REQUIRE( res == 666 );

    auto range = v.index_range();
    REQUIRE( range.first == 0 );
    REQUIRE( range.second == 0 );

    REQUIRE( v.exist( 0 ) == true );
  }

  SECTION( "erase" ) {
    ecs::sparse_vector< int > v;
    v.emplace( 0, 666 );

    v.erase( 0 );

    auto range = v.index_range();
    REQUIRE( range.first == v.bad_index );
    REQUIRE( range.second == v.bad_index );

    REQUIRE( v.exist( 0 ) == false );
  }

  SECTION( "insert_with_shift" ) {
    ecs::sparse_vector< int > v;
    v.insert( 10, 13 );
    v.insert( 9, 666 );

    auto range = v.index_range();
    REQUIRE( range.first == 9 );
    REQUIRE( range.second == 10 );

    REQUIRE( v.exist( 9 ) == true );
    REQUIRE( v.exist( 10 ) == true );
    REQUIRE( v.get_unsafe( 9 ) == 666 );
    REQUIRE( v.get_unsafe( 10 ) == 13 );
  }

  SECTION( "insert_in_between" ) {
    ecs::sparse_vector< int > v;
    v.insert( 8, 100500 );
    v.insert( 10, 13 );
    v.insert( 9, 666 );

    auto range = v.index_range();
    REQUIRE( range.first == 8 );
    REQUIRE( range.second == 10 );

    REQUIRE( v.exist( 8 ) == true );
    REQUIRE( v.exist( 9 ) == true );
    REQUIRE( v.exist( 10 ) == true );
    REQUIRE( v.get_unsafe( 8 ) == 100500 );
    REQUIRE( v.get_unsafe( 9 ) == 666 );
    REQUIRE( v.get_unsafe( 10 ) == 13 );
  }

  SECTION( "emplace_with_shift" ) {
    ecs::sparse_vector< int > v;
    v.emplace( 10, 13 );
    v.emplace( 9, 666 );

    auto range = v.index_range();
    REQUIRE( range.first == 9 );
    REQUIRE( range.second == 10 );

    REQUIRE( v.exist( 9 ) == true );
    REQUIRE( v.exist( 10 ) == true );
    REQUIRE( v.get_unsafe( 9 ) == 666 );
    REQUIRE( v.get_unsafe( 10 ) == 13 );
  }

  SECTION( "emplace_in_between" ) {
    ecs::sparse_vector< int > v;
    v.emplace( 8, 100500 );
    v.emplace( 10, 13 );
    v.emplace( 9, 666 );

    auto range = v.index_range();
    REQUIRE( range.first == 8 );
    REQUIRE( range.second == 10 );

    REQUIRE( v.exist( 8 ) == true );
    REQUIRE( v.exist( 9 ) == true );
    REQUIRE( v.exist( 10 ) == true );
    REQUIRE( v.get_unsafe( 8 ) == 100500 );
    REQUIRE( v.get_unsafe( 9 ) == 666 );
    REQUIRE( v.get_unsafe( 10 ) == 13 );
  }

  SECTION( "erase_with_shift" ) {
    ecs::sparse_vector< int > v;
    v.insert( 8, 100500 );
    v.insert( 9, 666 );
    v.insert( 10, 13 );
    v.erase( 8 );

    auto range = v.index_range();
    REQUIRE( range.first == 9 );
    REQUIRE( range.second == 10 );

    REQUIRE( v.exist( 8 ) == false );
    REQUIRE( v.exist( 9 ) == true );
    REQUIRE( v.exist( 10 ) == true );
    REQUIRE( v.get_unsafe( 9 ) == 666 );
    REQUIRE( v.get_unsafe( 10 ) == 13 );
  }

  SECTION( "erase_in_between" ) {
    ecs::sparse_vector< int > v;
    v.insert( 8, 100500 );
    v.insert( 9, 666 );
    v.insert( 10, 13 );
    v.erase( 9 );

    auto range = v.index_range();
    REQUIRE( range.first == 8 );
    REQUIRE( range.second == 10 );

    REQUIRE( v.exist( 8 ) == true );
    REQUIRE( v.exist( 9 ) == false );
    REQUIRE( v.exist( 10 ) == true );
    REQUIRE( v.get_unsafe( 8 ) == 100500 );
    REQUIRE( v.get_unsafe( 10 ) == 13 );
  }

  SECTION( "erase_non_existent" ) {
    ecs::sparse_vector< int > v;

    REQUIRE( v.exist( 0 ) == false );

    v.erase( 13 );

    REQUIRE( v.exist( 0 ) == false );
  }

  SECTION( "erase_non_existent_on_existing_page" ) {
    ecs::sparse_vector< int > v;
    v.insert( 14, 666 );

    REQUIRE( v.exist( 0 ) == false );

    v.erase( 13 );

    REQUIRE( v.exist( 0 ) == false );
  }
}

TEST_CASE( "access" ) {
  SECTION( "get_unsafe" ) {
    ecs::sparse_vector< int > v;
    v.emplace( 0, 666 );

    auto res = v.get_unsafe( 0 );
    REQUIRE( res == 666 );
  }

  SECTION( "get_unsafe_const" ) {
    ecs::sparse_vector< int > v;
    v.emplace( 0, 666 );
    const auto& vc = v;

    const auto res = vc.get_unsafe( 0 );
    REQUIRE( res == 666 );
  }

  SECTION( "get_operator" ) {
    ecs::sparse_vector< int > v;
    v.emplace( 0, 666 );

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): operator[] is what is under test
    auto res = v[ 0 ];
    REQUIRE( res == 666 );
  }

  SECTION( "get_operator_create" ) {
    ecs::sparse_vector< int > v;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): operator[] is what is under test
    auto res = v[ 0 ];
    REQUIRE( v.exist( 0 ) == true );
    REQUIRE( res == 0 );
  }

  SECTION( "get_operator_not_first_page" ) {
    ecs::sparse_vector< int > v;
    v.emplace( 66, 666 );

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): operator[] is what is under test
    auto res = v[ 66 ];
    REQUIRE( res == 666 );
  }
}

TEST_CASE( "utility" ) {
  SECTION( "reserve" ) {
    ecs::sparse_vector< int > v;
    v.reserve( 1000 );

    auto range = v.index_range();

    REQUIRE( range.first == v.bad_index );
    REQUIRE( range.second == v.bad_index );
  }

  SECTION( "reserve_2" ) {
    ecs::sparse_vector< int > v;
    v.reserve( 512 );

    auto range = v.index_range();

    REQUIRE( range.first == v.bad_index );
    REQUIRE( range.second == v.bad_index );
  }

  SECTION( "reserve_non_empty" ) {
    ecs::sparse_vector< int > v;
    v.insert( 32, 666 );

    v.reserve( 512 );

    auto range = v.index_range();

    REQUIRE( range.first == 32 );
    REQUIRE( range.second == 32 );
  }

  SECTION( "index_range" ) {
    ecs::sparse_vector< int > v;
    v.emplace( 0, 666 );
    v.emplace( 16, 666 );

    auto range = v.index_range();

    REQUIRE( range.first == 0 );
    REQUIRE( range.second == 16 );
  }

  SECTION( "index_range_not_first_page" ) {
    ecs::sparse_vector< int > v;
    v.emplace( 66, 666 );
    v.emplace( 70, 500 );

    auto range = v.index_range();

    REQUIRE( range.first == 66 );
    REQUIRE( range.second == 70 );
  }

  SECTION( "index_range_different_pages" ) {
    ecs::sparse_vector< int > v;
    v.emplace( 66, 666 );
    v.emplace( 13, 666 );

    auto range = v.index_range();

    REQUIRE( range.first == 13 );
    REQUIRE( range.second == 66 );
  }
}

TEST_CASE( "element_lifetime" ) {
  SECTION( "destructor_destroys_elements" ) {
    REQUIRE( Counted::live == 0 );

    {
      ecs::sparse_vector< Counted > v;
      v.emplace( 0, 1 );
      v.emplace( 1, 2 );
      v.emplace( 100, 3 ); // second page, so both pages are covered

      REQUIRE( Counted::live == 3 );
    }

    REQUIRE( Counted::live == 0 );
  }

  SECTION( "clear_destroys_elements" ) {
    ecs::sparse_vector< Counted > v;
    v.emplace( 0, 1 );
    v.emplace( 100, 2 );

    REQUIRE( Counted::live == 2 );

    v.clear();

    REQUIRE( Counted::live == 0 );
  }

  SECTION( "erase_last_element_of_page_releases_page" ) {
    ecs::sparse_vector< Counted > v;
    v.emplace( 0, 1 );

    REQUIRE( Counted::live == 1 );

    v.erase( 0 );

    // The page becomes empty and is deleted, which used to lose the element.
    REQUIRE( Counted::live == 0 );
    REQUIRE( v.exist( 0 ) == false );
  }
}

// The page keeps its slots in a fixed order and records where each position
// lives, so inserting or removing inside a page shuffles both the objects and
// that record. Getting the two out of step used to surface as a position holding
// a value that belonged to another one, and as two objects destroyed for a
// single removal.
//
// The check below is driven purely through the public interface and asserts what
// a caller can observe: which positions exist and what they hold.
TEST_CASE( "page_slot_bookkeeping" ) {
  SECTION( "positions_stay_paired_with_their_values" ) {
    struct Step {
      bool insert;
      size_t pos;
      int value;
    };

    // Ascending first, then filling the gaps backwards, which lands insertions
    // and removals in the middle of the page rather than at its end.
    const std::vector< Step > steps = {
      { true, 8, 100 }, { true, 10, 300 }, { true, 9, 200 }, { false, 8, 0 },   { true, 12, 500 }, { false, 10, 0 },
      { true, 8, 400 }, { true, 11, 600 }, { false, 9, 0 },  { true, 10, 700 }, { false, 12, 0 },  { true, 13, 800 },
    };

    const size_t highest = 16;

    ecs::sparse_vector< Counted > v;
    std::map< size_t, int > expected;

    for ( const auto& step : steps ) {
      if ( step.insert ) {
        v.emplace( step.pos, step.value );
        expected[ step.pos ] = step.value;
      } else {
        v.erase( step.pos );
        expected.erase( step.pos );
      }

      for ( size_t pos = 0; pos < highest; ++pos ) {
        CAPTURE( pos, step.pos );
        REQUIRE( v.exist( pos ) == ( expected.count( pos ) != 0 ) );
        if ( expected.count( pos ) != 0 ) {
          REQUIRE( v.get_unsafe( pos ).value == expected.at( pos ) );
        }
      }

      REQUIRE( Counted::live == static_cast< int >( expected.size() ) );
    }
  }

  SECTION( "empty_page_after_removals" ) {
    ecs::sparse_vector< Counted > v;

    v.emplace( 3, 10 );
    v.emplace( 5, 20 );
    v.emplace( 4, 30 );

    REQUIRE( Counted::live == 3 );

    v.erase( 3 );
    v.erase( 4 );
    v.erase( 5 );

    // Every position gone, so the page is released and nothing survives it.
    REQUIRE( Counted::live == 0 );
    REQUIRE( v.exist( 3 ) == false );
    REQUIRE( v.exist( 4 ) == false );
    REQUIRE( v.exist( 5 ) == false );
  }
}
