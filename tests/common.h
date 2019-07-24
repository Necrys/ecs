#pragma once

struct Position {
  float x, y;
  
  Position() {}
  Position( const float _x, const float _y ):
    x( _x ), y( _y ) {}

  bool operator== ( const Position& other ) const {
    return x == other.x && y == other.y;
  }
};

struct Velocity {
  float x, y;
  
  Velocity() {}
  Velocity( const float _x, const float _y ):
    x( _x ), y( _y ) {}

  bool operator== ( const Velocity& other ) const {
    return x == other.x && y == other.y;
  }
};

struct Event {
};

class System {
public:
  void update() {
  }

  void handle_event( const Event& ) {
  }
};