#pragma once

#include <cstring>
#include <initializer_list>
#include <string>

struct Position {
  float x, y;

  Position() {
  }
  Position( const float _x, const float _y ) :
    x( _x ),
    y( _y ) {
  }

  // Bit-exact on purpose, and compared as bytes because of it: what these tests
  // assert is that a component comes back out of a container exactly as it went in,
  // and clang is right that == on floats is a trap in general - an epsilon here
  // would hide a truncating defect instead of catching it.
  bool operator==( const Position& other ) const {
    return std::memcmp( this, &other, sizeof( *this ) ) == 0;
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
    return std::memcmp( this, &other, sizeof( *this ) ) == 0;
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

// The order systems ran in, shared by every tag below, so a test can assert on what
// dependencies did to the sequence.
//
// The registry constructs its systems itself, so a system has nowhere to record but
// a shared object - and a shared container in an inline function is what clang's
// -Wexit-time-destructors and -Wunique-object-duplication are about. A fixed buffer
// with a count is trivially destructible, and a handful of entries is all these
// tests ever record.
class update_log {
public:
  static constexpr size_t capacity = 16;

  void clear() {
    m_count = 0;
  }

  void add( const int tag ) {
    if ( m_count < capacity ) {
      m_entries[ m_count++ ] = tag;
    }
  }

  bool matches( const std::initializer_list< int > expected ) const {
    if ( expected.size() != m_count ) {
      return false;
    }

    size_t index = 0;
    for ( const int tag : expected ) {
      if ( m_entries[ index++ ] != tag ) {
        return false;
      }
    }
    return true;
  }

  // What actually ran: a bare `false` from matches() would not say what the order
  // was instead.
  std::string describe() const {
    std::string text = "[";
    for ( size_t index = 0; index < m_count; ++index ) {
      text += ( index == 0 ? "" : ", " ) + std::to_string( m_entries[ index ] );
    }
    return text + "]";
  }

private:
  int m_entries[ capacity ] = {};
  size_t m_count = 0;
};

// Defined once, in tests/common.cpp: the object is shared by every system a test
// registers, and a definition here would give it vague linkage.
update_log& updates();

// A system that appends its tag to the sequence when it updates. The tag is a
// template parameter rather than a member because a system has to be a distinct
// type for the registry to have a slot for it.
template < int Tag >
class TaggedSystem {
public:
  void update() {
    updates().add( Tag );
  }

  void handle_event( const Event& ) {
  }
};
