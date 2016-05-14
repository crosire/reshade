#pragma once

#include <list>
#include <algorithm>
#include "syntax_tree_nodes.hpp"

namespace reshadefx
{
	class syntax_tree
	{
		syntax_tree(const syntax_tree &) = delete;
		syntax_tree &operator=(const syntax_tree &) = delete;

	public:
		syntax_tree() { }

		template <typename T>
		T *make_node(const location &location)
		{
			const auto node = _pool.add<T>();
			node->location = location;

			return node;
		}

		std::vector<nodes::struct_declaration_node *> structs;
		std::vector<nodes::variable_declaration_node *> variables;
		std::vector<nodes::function_declaration_node *> functions;
		std::vector<nodes::technique_declaration_node *> techniques;

	private:
		class memory_pool
		{
		public:
			~memory_pool()
			{
				clear();
			}

			template <typename T>
			T *add()
			{
				auto size = sizeof(nodeinfo) - sizeof(node) + sizeof(T);
				auto page = std::find_if(_pages.begin(), _pages.end(),
					[size](const struct page &page)
				{
					return page.cursor + size < page.memory.size();
				});

				if (page == _pages.end())
				{
					_pages.emplace_back(std::max(size_t(4096), size));

					page = std::prev(_pages.end());
				}

				const auto node = new (&page->memory.at(page->cursor)) nodeinfo;
				const auto node_data = new (&node->data) T();
				node->size = size;
				node->dtor = [](void *object) { reinterpret_cast<T *>(object)->~T(); };

				page->cursor += node->size;

				return node_data;
			}
			void clear()
			{
				for (auto &page : _pages)
				{
					auto node = reinterpret_cast<nodeinfo *>(&page.memory.front());

					do
					{
						node->dtor(node->data);
						node = reinterpret_cast<nodeinfo *>(reinterpret_cast<unsigned char *>(node) + node->size);
					}
					while (node->size > 0 && (page.cursor -= node->size) > 0);
				}
			}

		private:
			struct page
			{
				explicit page(size_t size) : cursor(0), memory(size, '\0') { }

				size_t cursor;
				std::vector<unsigned char> memory;
			};
			struct nodeinfo
			{
				size_t size;
				void(*dtor)(void *);
				unsigned char data[sizeof(node)];
			};

			std::list<page> _pages;
		} _pool;
	};
}
