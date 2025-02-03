#ifndef MVC_IMODEL_HPP
#define MVC_IMODEL_HPP

#include "mvc/IView.hpp"
#include "db/LogEvent.hpp"

#include <vector>
#include <iostream>
#include <mutex>

namespace mvc
{
	class IModel
	{
	public:
		IModel() {};
		virtual ~IModel()
		{
			m_views.clear();
		}
		virtual void RegisterOndDataUpdated(IView *view)
		{
			m_views.push_back(view);
		}

		virtual void NotifyDataChanged()
		{

			for (auto v : m_views)
			{
				v->OnDataUpdated();
			}
		}

		virtual int GetCurrentItemIndex() = 0;
		virtual void SetCurrentItem(const int item) = 0;
		virtual void AddItem(db::LogEvent &&item) = 0;
		virtual db::LogEvent &GetItem(const int index) = 0;
		virtual void Clear() = 0;
		virtual size_t Size() = 0;

	protected:
		std::vector<IView *> m_views;
	};

} // namespace mvc

#endif // MVC_MODEL_HPP
