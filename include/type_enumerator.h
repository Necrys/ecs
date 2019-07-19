#pragma once

#include <type_traits>

namespace ecs {

template < typename... >
class type_collection {
public:
  static size_t type_collection_enumerator;

  template < typename... >
  inline static size_t type_collection_id() {
    static size_t cid = type_collection_enumerator++;
    return cid;
  }

public:
  template < typename... Ts >
  inline static size_t type_id() {
    static size_t tid = type_collection_id < typename std::decay< Ts >::type... >();
    return tid;
  }
};

template < typename... Ts >
size_t type_collection< Ts... >::type_collection_enumerator;

}
