#ifndef MVC_MODEL_HPP
#define MVC_MODEL_HPP

#include "mvc/View.hpp"

#include <vector>
#include <iostream>
#include <mutex>

namespace mvc
{
	template <typename Container>

	class Model
	{
	public:
		Model() {};
		virtual ~Model() {}
		void RegisterOndDataUpdated(View *view)
		{
			m_views.push_back(view);
		}

		void NotifyDataChanged()
		{

			for (auto v : m_views)
			{
				v->OnDataUpdated();
			}
		}

		int GetCurrentItemIndex()
		{
			return m_currentItem;
		}

		void SetCurrentItem(const int item)
		{
			m_currentItem = item;
			for (auto v : m_views)
			{
				v->OnCurrentIndexUpdated(item);
			}
		}

		void AddItem(auto &&item)
		{
			m_data.push_back(std::forward<decltype(item)>(item));
			this->NotifyDataChanged();
		}

		auto &GetItem(const int index)
		{
			return m_data.at(index);
		}

		void Clear()
		{
			m_data.clear();
			this->NotifyDataChanged();
		}

		auto Size()
		{
			return m_data.size();
		}

	protected:
		Container m_data;
		int m_currentItem;

	private:
		std::vector<View *> m_views;
	};

} // namespace mvc

#endif // MVC_MODEL_HPP
