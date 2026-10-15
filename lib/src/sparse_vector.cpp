// The non-template half of sparse_vector: the polymorphic base that lets a
// storage of one component type be held without knowing the type.
//
// The destructor is defined here, out of line, because a class whose virtual
// members are all inline has no key function: clang then emits the vtable as a weak
// symbol in every translation unit that uses the class, and says so under
// -Wweak-vtables. This definition is that key function.
#include "sparse_vector.h"

namespace ecs {

sparse_vector_base::~sparse_vector_base() = default;

} // namespace ecs
