#ifndef QT_UTIL_H
#define QT_UTIL_H

#include <QCoreApplication>
#include <QEvent>
#include <QThread>
#include <QVariant>
#include <cassert>
#include <future>
#include <utility>

class QTabWidget;

namespace Utility {
	inline QVariant make_qvariant(void *p) {
		return QVariant::fromValue(p);
	}
	template <class T>
	inline T *from_qvariant(QVariant &qv) {
		return static_cast<T *>(qv.value<void *>());
	}
	template <class T>
	inline T *from_qvariant(QVariant &&qv) {
		return static_cast<T *>(qv.value<void *>());
	}

	template <typename Fun>
	void thread_call(QObject *obj, Fun &&fun) { //calls fun in the thread that owns obj
		assert(obj->thread() || (qApp && (qApp->thread() == QThread::currentThread())));
		if (obj->thread() == QThread::currentThread()) {
			return fun();
		}
		struct Event : public QEvent {
			Fun fun;
			Event(Fun &&fun)
				: QEvent(QEvent::User)
				, fun(std::forward<Fun>(fun)) {}
			~Event() {
				fun();
			}
		};
		QCoreApplication::postEvent(obj->thread() ? obj : qApp, new Event(std::forward<Fun>(fun)));
	}

	template <class Fun>
	auto promised_thread_call(QObject *object, Fun &&f) -> decltype(f()) { //calls f in the thread that owns obj and waits for the function to get processed
		std::promise<decltype(f())> promise;
		auto future = promise.get_future();
		Utility::thread_call(object, [&f, &promise] {
			try {
				promise.set_value(f());
			} catch (...) {
				promise.set_exception(std::current_exception());
			}
		});
		return future.get();
	}

	QWidget *replace_tab_widget(QTabWidget *tabs, int index, QWidget *new_widget, const QString &title);
}

#endif // QT_UTIL_H
