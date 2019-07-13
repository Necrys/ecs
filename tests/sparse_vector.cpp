#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <sparse_vector.h>

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

    auto res = v[ 0 ];
    REQUIRE( res == 666 );
}

SECTION( "get_operator_create" ) {
    ecs::sparse_vector< int > v;

    auto res = v[ 0 ];
    REQUIRE( v.exist( 0 ) == true );
    REQUIRE( res == 0 );
}

SECTION( "get_operator_not_first_page" ) {
    ecs::sparse_vector< int > v;
    v.emplace( 66, 666 );

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
