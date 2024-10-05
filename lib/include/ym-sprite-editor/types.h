#pragma once

#define NTEST
#include "reflect.h"

namespace
{
	namespace reflect_lib = reflect;
}

namespace ym::types::internal
{
	template <typename T>
	size_t compute_id()
	{
		return reflect_lib::type_id<T>();
	}
}

namespace ym::sprite_editor::types
{
	template <typename T>
	size_t type_id()
	{
		return ym::types::internal::compute_id<T>();
	}
}