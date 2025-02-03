#ifndef MVC_VIEW_HPP
#define MVC_VIEW_HPP

namespace mvc
{
	class IView
	{
	public:
		IView() {}
		virtual ~IView() {}
		virtual void OnDataUpdated() = 0;
		virtual void OnCurrentIndexUpdated(const int index) = 0;
	};

} // namespace mvc

#endif // MVC_VIEW_HPP
