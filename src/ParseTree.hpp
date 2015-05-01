#pragma once

#include "ParseTreeNodes.hpp"

#include <list>
#include <algorithm>

namespace ReShade
{
	namespace FX
	{
		class Tree
		{
			friend class Parser;

		public:
			template <typename T>
			inline T *CreateNode(const Location &location)
			{
				T *const node = this->mMemoryPool.Add<T>();
				node->Location = location;

				return node;
			}

			std::vector<Nodes::Struct *> Types;
			std::vector<Nodes::Variable *> Uniforms;
			std::vector<Nodes::Function *> Functions;
			std::vector<Nodes::Technique *> Techniques;

		private:
			class MemoryPool
			{
			public:
				~MemoryPool()
				{
					Cleanup();
				}

				template <typename T>
				T *Add()
				{
					const std::size_t size = sizeof(Node) + sizeof(T);
					std::list<Page>::iterator page = std::find_if(this->mPages.begin(), this->mPages.end(), [size](const Page &page) { return page.Cursor + size < page.Nodes.size(); });

					if (page == this->mPages.end())
					{
						this->mPages.push_back(Page(std::max(static_cast<std::size_t>(4096), size)));

						page = std::prev(this->mPages.end());
					}

					Node *node = new (&page->Nodes.at(page->Cursor)) Node;
					node->Size = size;
					node->Destructor = &Node::DestructorDelegate<T>;
					T *nodeData = new (&node->Data) T();

					page->Cursor += size;

					return nodeData;
				}
				void Cleanup()
				{
					for (Page &page : this->mPages)
					{
						Node *node = reinterpret_cast<Node *>(&page.Nodes.front());

						do
						{
							node->Destructor(node->Data);
							node = reinterpret_cast<Node *>(reinterpret_cast<unsigned char *>(node) + node->Size);
						}
						while (node->Size > 0 && (page.Cursor -= node->Size) > 0);
					}
				}

			private:
				struct Page
				{
					Page(std::size_t size) : Cursor(0), Nodes(size, 0)
					{
					}

					std::size_t Cursor;
					std::vector<unsigned char> Nodes;
				};
				struct Node
				{
					template <typename T>
					static void DestructorDelegate(void *object)
					{
						reinterpret_cast<T *>(object)->~T();
					}

					std::size_t Size;
					void (*Destructor)(void *);
					unsigned char Data[1];
				};

				std::list<Page> mPages;
			};

			MemoryPool mMemoryPool;
		};
	}
}