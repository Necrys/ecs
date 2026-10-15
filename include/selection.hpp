#pragma once

// Walking a set of component storages together.
//
// A join is expressed as a set of required component types. The walk is driven
// by whichever storage holds the fewest elements, and the other storages are
// queried by id for each candidate. The callback receives the components in the
// order the types were named.
//
// The machinery exists for one reason: which storage is walked is a run-time
// decision (whichever is smallest), while reading its elements needs its type at
// compile time. Recovering that type while keeping the component set intact is
// what the visitor below does.

#include <array>
#include <cstddef>
#include <initializer_list>
#include <utility>

namespace ecs {
namespace detail {

  // A pack carried as a value. A bare parameter pack cannot be followed by a
  // deduced template parameter, so wherever the component set has to travel
  // alongside a type that is still to be deduced, it travels wrapped in this.
  template < typename... Ts >
  struct type_list {};

  // The components of one entity, resolved once so that the callback does not
  // look any of them up again. The slot belonging to the storage being walked
  // holds null, because that element is already in hand and is passed through
  // separately.
  template < size_t Count >
  struct component_pack {
    std::array< const void*, Count > pointers;
  };

  // True when every listed component except `Walked` is present on the id. The
  // walked storage is skipped: the entity came from it, so testing it would
  // always succeed and would only cost time.
  template < typename Walked, typename Storage, typename... Cs >
  bool all_present( const Storage& storage, const eid_t id ) {
    bool result = true;

    ( void ) std::initializer_list< int >{ ( result = result && [ & ] {
      if ( component_type< Cs >::id == component_type< Walked >::id ) {
        return true;
      }
      const auto* one = storage.template get_storage< Cs >();
      return one != nullptr && one->exist( id );
    }(), 0 )... };

    return result;
  }

  // The storage holding the fewest elements among the set, with its component
  // id. Null when none of the types holds anything.
  template < typename Storage, typename... Cs >
  std::pair< const sparse_vector_base*, eid_t > smallest_storage( const Storage& storage ) {
    const sparse_vector_base* picked = nullptr;
    size_t picked_size = 0;
    eid_t picked_id = 0;

    ( void ) std::initializer_list< int >{ ( [ & ] {
      const auto* one = storage.template get_storage< Cs >();
      if ( one != nullptr && one->size() != 0 && ( picked == nullptr || one->size() < picked_size ) ) {
        picked = static_cast< const sparse_vector_base* >( one );
        picked_size = one->size();
        picked_id = static_cast< eid_t >( component_type< Cs >::id );
      }
      return 0;
    }() )... };

    return std::make_pair( picked, picked_id );
  }

  // Fills one slot of the pack. The walked component is marked with null; every
  // other component is resolved for this entity.
  template < typename Storage, typename Walked, typename C, size_t Slot, typename Pack >
  void set_slot( Storage& storage, const eid_t id, Pack& pack ) {
    if ( component_type< C >::id == component_type< Walked >::id ) {
      pack.pointers[ Slot ] = nullptr;
    }
    else {
      pack.pointers[ Slot ] = &storage.template get_storage< C >()->get_unsafe( id );
    }
  }

  // Resolves the components of one entity. The slot index comes from an index
  // sequence: as a plain variable it would be expanded identically into every
  // argument, and every component would read the same slot.
  template < typename Storage, typename Walked, typename... Cs, size_t... Is >
  component_pack< sizeof...( Cs ) > resolve_pack_impl( Storage& storage, const eid_t id,
                                                       std::index_sequence< Is... > ) {
    component_pack< sizeof...( Cs ) > pack;
    ( void ) std::initializer_list< int >{
      ( set_slot< Storage, Walked, Cs, Is >( storage, id, pack ), 0 )... };
    return pack;
  }

  template < typename Storage, typename Walked, typename... Cs >
  component_pack< sizeof...( Cs ) > resolve_pack( Storage& storage, const eid_t id ) {
    return resolve_pack_impl< Storage, Walked, Cs... >( storage, id, std::make_index_sequence< sizeof...( Cs ) >() );
  }

  // Rebuilds the callback arguments in the declared order.
  template < typename... Cs >
  struct selection_visitor {
    const components_storage& storage;

    template < typename Ft, typename Walked >
    bool visit( Ft&& callback, const eid_t id, const Walked& walked,
                const component_pack< sizeof...( Cs ) >& pack ) const {
      invoke( std::forward< Ft >( callback ), id, walked, pack,
              std::make_index_sequence< sizeof...( Cs ) >() );
      return true;
    }

  private:
    template < typename Ft, typename Walked, size_t... Is >
    void invoke( Ft&& callback, const eid_t id, const Walked& walked,
                 const component_pack< sizeof...( Cs ) >& pack, std::index_sequence< Is... > ) const {
      callback( id, element< Cs, Is >( walked, pack )... );
    }

    // The slot filled with null is the one the walked element belongs to; every
    // other slot holds a resolved pointer.
    template < typename C, size_t Slot, typename Walked >
    const C& element( const Walked& walked, const component_pack< sizeof...( Cs ) >& pack ) const {
      const void* pointer = pack.pointers[ Slot ];
      if ( pointer == nullptr ) {
        return reinterpret_cast< const C& >( walked );
      }
      return *static_cast< const C* >( pointer );
    }
  };

  // Walks the smallest storage of the set and invokes the callback for every
  // entity carrying all of the listed components. Returns false when the set
  // holds no elements at all.
  //
  // The id came from a storage chosen among the component types, so exactly one
  // comparison below matches; the other branches only run while `found` is still
  // false.
  template < typename Storage, typename... Cs, typename Ft >
  bool walk_required( Storage& storage, Ft&& callback ) {
    const auto picked = smallest_storage< Storage, Cs... >( storage );
    if ( picked.first == nullptr ) {
      return false;
    }

    selection_visitor< Cs... > visitor{ storage };

    bool found = false;
    ( void ) std::initializer_list< int >{ ( found = found || [ & ] {
      if ( component_type< Cs >::id != picked.second ) {
        return false;
      }

      storage.template get_storage< Cs >()->each( [ & ]( const size_t id, const Cs& walked ) -> bool {
        const auto entity = static_cast< eid_t >( id );
        if ( all_present< Cs, Storage, Cs... >( storage, entity ) ) {
          const auto pack = resolve_pack< Storage, Cs, Cs... >( storage, entity );
          visitor.visit( callback, entity, walked, pack );
        }
        return true;
      } );
      return true;
    }(), 0 )... };

    return found;
  }

} // namespace detail
} // namespace ecs
