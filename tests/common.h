#pragma once

#include <vector>

struct Position {
  float x, y;

  Position() {
  }
  Position( const float _x, const float _y ) :
    x( _x ),
    y( _y ) {
  }

  bool operator==( const Position& other ) const {
    return x == other.x && y == other.y;
  }
};

struct Velocity {
  float x, y;

  Velocity() {
  }
  Velocity( const float _x, const float _y ) :
    x( _x ),
    y( _y ) {
  }

  bool operator==( const Velocity& other ) const {
    return x == other.x && y == other.y;
  }
};

struct Event {};

// A second value component, used where a test needs two independent storages.
struct Tag {
  int value;

  Tag() {
  }
  Tag( const int v ) :
    value( v ) {
  }
};

class System {
public:
  void update() {
  }

  void handle_event( const Event& ) {
  }
};

// Counts delivered events so tests can assert on event delivery.
class CountingSystem {
public:
  static int received;

  void update() {
  }

  void handle_event( const Event& ) {
    ++received;
  }
};

// The order systems ran in, shared by every tag below, so a test can assert on
// what dependencies did to the sequence.
inline std::vector< int >& update_sequence() {
  static std::vector< int > sequence;
  return sequence;
}

// A system that appends its tag to the sequence when it updates. The tag is a
// template parameter rather than a member because a system has to be a distinct
// type for the registry to have a slot for it.
template < int Tag >
class TaggedSystem {
public:
  void update() {
    update_sequence().push_back( Tag );
  }

  void handle_event( const Event& ) {
  }
};