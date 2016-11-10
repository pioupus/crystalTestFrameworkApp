#ifndef UTILITY_H
#define UTILITY_H

#include <sstream>
#include <QString>

namespace Utility{
	template<class T>
	bool convert(const std::wstring &ws, T &t){
		return !(std::wistringstream(ws) >> t).fail();
	}
	template<class T>
	bool convert(const std::string &s, T &t){
		return !(std::istringstream(s) >> t).fail();
	}
	template<class T>
	bool convert(const QString &qs, T &t){
		return convert(qs.toStdWString(), t);
	}

	namespace impl{
		template<typename F>
		struct RAII_Helper{
			template<typename InitFunction>
			RAII_Helper(InitFunction &&init, F &&exit) : f_(std::forward<F>(exit)), canceled(false){
				init();
			}
			RAII_Helper(F &&f) : f_(f), canceled(false){
			}
			~RAII_Helper(){
				if (!canceled)
					f_();
			}
			void cancel(){
				canceled = true;
			}
		private:
			F f_;
			bool canceled;
		};
	}
	template<class F>
	impl::RAII_Helper<F> RAII_do(F &&f){
		return impl::RAII_Helper<F>(std::forward<F>(f));
	}

	template<class Init, class Exit>
	impl::RAII_Helper<Exit> RAII_do(Init &&init, Exit &&exit){
		return impl::RAII_Helper<Exit>(std::forward<Init>(init), std::forward<Exit>(exit));
	}

}

#endif // UTILITY_H