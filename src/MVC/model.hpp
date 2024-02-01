#ifndef MVC_MODEL_HPP
#define MVC_MODEL_HPP

#include "view.hpp"

#include <vector>
#include <iostream>

namespace mvc
{
	template <typename Container>
	class Model
	{
	public:
		Model(){};
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

		int GetCurrentItemIndex() const
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

	protected:
		Container m_data;
		int m_currentItem;

	private:
		std::vector<View *> m_views;
	};

} // namespace mvc

#endif // MVC_MODEL_HPP
