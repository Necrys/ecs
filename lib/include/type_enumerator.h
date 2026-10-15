#pragma once

#include <type_traits>
#include <cstddef>
#include <cstdint>

namespace ecs {

template < typename... >
class type_collection {
public:
  static size_t type_collection_enumerator;

  template < typename... >
  static size_t type_collection_id() noexcept {
    static size_t cid = type_collection_enumerator++;
    return cid;
  }

  // noexcept because that is the truth, and because component_type<T>::id is
  // initialised by a call to this: a static initialiser that can throw is a trap
  // (bugprone-throwing-static-initialization), and this one cannot.
  template < typename... Ts >
  static size_t type_id() noexcept {
    static size_t tid = type_collection_id< std::decay_t< Ts >... >();
    return tid;
  }
};

template < typename... Ts >
size_t type_collection< Ts... >::type_collection_enumerator;

}
