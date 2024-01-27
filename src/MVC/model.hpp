#ifndef MVC_MODEL_HPP
#define MVC_MODEL_HPP

#include "view.hpp"

#include <vector>

namespace mvc
{

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

	private:
		std::vector<View*> m_views;
	};

} // namespace mvc

#endif // MVC_MODEL_HPP
