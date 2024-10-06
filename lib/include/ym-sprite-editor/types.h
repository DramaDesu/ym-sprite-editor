#pragma once

#include <string>

namespace ym::sprite_editor::types
{
	template <typename T> requires std::is_class_v<T>
	std::string_view type_name()
	{
#ifdef _MSC_VER
		std::string_view name = __FUNCSIG__;
		constexpr std::string_view prefix = "type_name<";
		constexpr std::string_view suffix = ">(void)";
#elif defined(__clang__) || defined(__GNUC__)
		std::string_view name = __PRETTY_FUNCTION__;
		constexpr std::string_view prefix = "std::string_view type_name() [T = ";
		constexpr std::string_view suffix = "]";
#endif

#ifdef _MSC_VER
		if (auto prefix_size = name.find(prefix); prefix_size != std::string_view::npos)
		{
			name.remove_prefix(prefix_size + prefix.size());
		}
#else
		name.remove_prefix(prefix.size());
#endif

		constexpr std::string_view class_prefix = "class ";
		constexpr std::string_view struct_prefix = "struct ";

		if (name.starts_with(class_prefix))
		{
			name.remove_prefix(class_prefix.size());
		}
		else if (name.starts_with(struct_prefix))
		{
			name.remove_prefix(struct_prefix.size());
		}

		name.remove_suffix(suffix.size());
		return name;
	}

	template <typename T>
	size_t type_id()
	{
		return std::hash<std::string_view>{}(type_name<T>());
	}
}

