#pragma once

struct Position {
  float x, y;
  
  Position() {}
  Position( const float _x, const float _y ):
    x( _x ), y( _y ) {}
};

struct Velocity {
  float x, y;
  
  Velocity() {}
  Velocity( const float _x, const float _y ):
    x( _x ), y( _y ) {}
};
