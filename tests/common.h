#pragma once

#include <initializer_list>
#include <string>

struct Position {
  float x{}, y{};

  Position() = default;
  Position( const float _x, const float _y ) :
    x( _x ),
    y( _y ) {
  }

  // Exact on purpose: what these tests assert is that a component comes back out of
  // a container exactly as it went in, and an epsilon here would hide a truncating
  // defect instead of catching it. Clang is right that comparing floats is a trap in
  // general, which is why -Wfloat-equal is turned off for the test targets - see
  // tests/CMakeLists.txt.
  bool operator==( const Position& other ) const {
    return x == other.x && y == other.y;
  }
};

struct Velocity {
  float x{}, y{};

  Velocity() = default;
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
  int value{};

  Tag() = default;
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

// Counts the events delivered to it, so a test can ask the registry for the system
// and read what it saw. An instance member rather than a static one: every test in
// the binary shares the type, and a counter they all write to is state left behind
// by whichever of them ran first.
class CountingSystem {
public:
  int received = 0;

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

  [[nodiscard]] bool matches( const std::initializer_list< int > expected ) const {
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
  [[nodiscard]] std::string describe() const {
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
