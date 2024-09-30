#include "include/ym-sprite-editor.h"

namespace
{
	class SegaSprite : public ym::sprite_editor::BaseSprite
	{
	public:
		SegaSprite(const ym::sprite_editor::position_t& in_position)
		{
			position = in_position;
		}

		ym::sprite_editor::size_t get_size() const override
		{
			constexpr auto tile_size = 8;
			return size_ * tile_size;
		}

	private:
		ym::sprite_editor::size_t size_{4, 4};
	};

	class SegaSpriteEditor final : public ym::sprite_editor::ISpriteEditor
	{
	public:
		std::shared_ptr<ym::sprite_editor::BaseSprite> create_sprite(const ym::sprite_editor::position_t& in_position) override
		{
			if (auto sprite = creation_function_(in_position)) [[likely]]
			{
				sprites_.push_back(sprite);
				return sprite;
			}
			return nullptr;
		}

		sprite_range sprites() const override
		{
			struct vector_impl : sprite_range::iterator::iterator_impl
			{
				using it = std::vector<sprite_t>::const_iterator;

				explicit vector_impl(it in_begin, it in_end) : begin_(in_begin), end_(in_end) {}

				std::shared_ptr<ym::sprite_editor::BaseSprite> dereference() const override
				{
					return *begin_;
				}
				void increment() override
				{
					++begin_;
				}
				bool not_equal(const iterator_impl& other) const override
				{
					const auto& o = static_cast<const vector_impl&>(other); // TODO
					return begin_ != o.begin_;
				}

			private:
				it begin_;
				it end_;
			};

			struct sprite_range_impl : sprite_range::range_impl
			{
				explicit sprite_range_impl(const std::vector<std::shared_ptr<ym::sprite_editor::BaseSprite>>& sprites)
					: sprites_(sprites) {}

				std::unique_ptr<sprite_range::iterator::iterator_impl> begin() const override
				{
					return std::make_unique<vector_impl>(sprites_.begin(), sprites_.end());
				}
				std::unique_ptr<sprite_range::iterator::iterator_impl> end() const override
				{
					return std::make_unique<vector_impl>(sprites_.end(), sprites_.end());
				}
			private:
				const std::vector<std::shared_ptr<ym::sprite_editor::BaseSprite>>& sprites_;
			};
			return { std::make_unique<sprite_range_impl>(sprites_) };
		}

		size_t sprites_num() const override
		{
			return sprites_.size();
		}

	protected:
		void on_register_sprite(creation_function_t&& in_sprite_creation) override
		{
			creation_function_ = std::move(in_sprite_creation);
		}

	private:
		using sprite_t = std::shared_ptr<ym::sprite_editor::BaseSprite>;

		std::vector<sprite_t> sprites_;
		creation_function_t creation_function_;
		
	};

	std::shared_ptr<ym::sprite_editor::ISpriteEditor> create_sprite_editor_internal()
	{
		if (auto editor = std::make_shared<SegaSpriteEditor>()) [[likely]]
		{
			editor->register_sprite<SegaSprite>();
			return editor;
		}
		return nullptr;
	}
}

namespace ym::sprite_editor
{
	std::shared_ptr<BaseSprite> ISpriteEditor::sprite_range::iterator::operator*() const
	{
		return impl_->dereference();
	}

	ISpriteEditor::sprite_range::iterator& ISpriteEditor::sprite_range::iterator::operator++()
	{
		impl_->increment();
		return *this;
	}

	bool ISpriteEditor::sprite_range::iterator::operator!=(const iterator& other) const
	{
		return impl_->not_equal(*other.impl_);
	}

	ISpriteEditor::sprite_range::iterator ISpriteEditor::sprite_range::begin() const
	{
		return impl_->begin();
	}

	ISpriteEditor::sprite_range::iterator ISpriteEditor::sprite_range::end() const
	{
		return impl_->end();
	}

	std::shared_ptr<ISpriteEditor> create_sprite_editor()
	{
		return create_sprite_editor_internal();
	}
}