/*

The MIT License (MIT)

Copyright (c) 2015 Ievgen Polyvanyi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#ifndef LSIGNAL_H
#define LSIGNAL_H

#include <functional>
#include <list>
#include <memory>
#include <vector>

namespace std
{
	// custom std::placeholders

	template<int>
	struct placeholder_lsignal
	{
	};

	template<int N>
	struct is_placeholder<std::placeholder_lsignal<N>>
		: integral_constant<int, N+1>
	{
	};
}

namespace lsignal
{
	// std::integer_sequence for C++11

	template<int... Ns>
	struct int_sequence
	{
	};

	template<int N, int... Ns>
	struct make_int_sequence
		: make_int_sequence<N-1, N-1, Ns...>
	{
	};

	template<int... Ns>
	struct make_int_sequence<0, Ns...>
		: int_sequence<Ns...>
	{
	};

	// connection

	struct connection_data
	{
		bool locked;
	};

	class connection
	{
		template<typename>
		friend class signal;

	public:
		connection(std::shared_ptr<connection_data>&& data);
		virtual ~connection();

		bool is_locked() const;
		void set_lock(const bool lock);

		void disconnect();

	private:
		std::shared_ptr<connection_data> _data;
		std::function<void(std::shared_ptr<connection_data>)> _deleter;

	};

	connection::connection(std::shared_ptr<connection_data>&& data)
		: _data(std::move(data))
	{
	}

	connection::~connection()
	{
	}

	bool connection::is_locked() const
	{
		return _data->locked;
	}

	void connection::set_lock(const bool lock)
	{
		_data->locked = lock;
	}

	void connection::disconnect()
	{
		if (_deleter)
		{
			_deleter(_data);
		}
	}

	// slot

	class slot
		: public connection
	{
	public:
		slot();
		~slot() override;

	};

	slot::slot()
		: connection(std::shared_ptr<connection_data>())
	{
	}

	slot::~slot()
	{
		disconnect();
	}

	// signal

	template<typename>
	class signal;

	template<typename R, typename... Args>
	class signal<R(Args...)>
	{
	public:
		using result_type = R;
		using callback_type = std::function<R(Args...)>;

		signal();
		~signal();

		bool is_locked() const;
		void set_lock(const bool lock);

		connection connect(const callback_type& fn, slot *owner = nullptr);
		connection connect(callback_type&& fn, slot *owner = nullptr);

		template<typename T, typename U>
		connection connect(T *p, const U& fn, slot *owner = nullptr);

		void disconnect(const connection& connection);
		void disconnect(slot *owner);

		void disconnect_all();

		R operator() (Args... args);

		template<typename T>
		R operator() (Args... args, const T& agg);

	private:
		struct joint
		{
			callback_type callback;
			std::shared_ptr<connection_data> connection;
			slot *owner;
		};

		bool _locked;
		std::list<joint> _callbacks;

		template<typename T, typename U, int... Ns>
		callback_type construct_mem_fn(const T& fn, U *p, int_sequence<Ns...>) const;

		std::shared_ptr<connection_data> create_connection(callback_type&& fn, slot *owner);
		void destroy_connection(std::shared_ptr<connection_data> connection);

		connection prepare_connection(connection&& conn);

	};

	template<typename R, typename... Args>
	signal<R(Args...)>::signal()
		: _locked(false)
	{
	}

	template<typename R, typename... Args>
	signal<R(Args...)>::~signal()
	{
		for (auto iter = _callbacks.begin(); iter != _callbacks.end(); ++iter)
		{
			const joint& jnt = *iter;

			if (jnt.owner != nullptr)
			{
				jnt.owner->_data = nullptr;
				jnt.owner->_deleter = std::function<void(std::shared_ptr<connection_data>)>();
			}
		}
	}

	template<typename R, typename... Args>
	bool signal<R(Args...)>::is_locked() const
	{
		return _locked;
	}

	template<typename R, typename... Args>
	void signal<R(Args...)>::set_lock(const bool lock)
	{
		_locked = lock;
	}

	template<typename R, typename... Args>
	connection signal<R(Args...)>::connect(const callback_type& fn, slot *owner)
	{
		connection conn = connection(create_connection(static_cast<callback_type>(fn), owner));

		return prepare_connection(std::move(conn));
	}

	template<typename R, typename... Args>
	connection signal<R(Args...)>::connect(callback_type&& fn, slot *owner)
	{
		connection conn = connection(create_connection(std::move(fn), owner));

		return prepare_connection(std::move(conn));
	}

	template<typename R, typename... Args>
	template<typename T, typename U>
	connection signal<R(Args...)>::connect(T *p, const U& fn, slot *owner)
	{
		auto mem_fn = std::move(construct_mem_fn(fn, p, make_int_sequence<sizeof...(Args)>{}));

		connection conn = create_connection(std::move(mem_fn), owner);

		return prepare_connection(std::move(conn));
	}

	template<typename R, typename... Args>
	void signal<R(Args...)>::disconnect(const connection& connection)
	{
		destroy_connection(connection._data);
	}

	template<typename R, typename... Args>
	void signal<R(Args...)>::disconnect(slot *owner)
	{
		if (owner != nullptr)
		{
			destroy_connection(owner->_data);
		}
	}

	template<typename R, typename... Args>
	void signal<R(Args...)>::disconnect_all()
	{
		for (auto iter = _callbacks.begin(); iter != _callbacks.end(); ++iter)
		{
			const joint& jnt = *iter;

			if (jnt.owner != nullptr)
			{
				jnt.owner->_data = nullptr;
				jnt.owner->_deleter = std::move(std::function<void(std::shared_ptr<connection_data>)>());
			}
		}
	}

	template<typename R, typename... Args>
	R signal<R(Args...)>::operator() (Args... args)
	{
		if (!_locked)
		{
			auto iter = _callbacks.cbegin();
			auto last = --_callbacks.cend();

			for ( ; iter != last; ++iter)
			{
				const joint& jnt = *iter;

				if (!jnt.connection->locked)
				{
					jnt.callback(std::forward<Args>(args)...);
				}
			}

			if (iter != _callbacks.end())
			{
				const joint& jnt = *iter;

				if (!jnt.connection->locked)
				{
					return jnt.callback(std::forward<Args>(args)...);
				}
			}
		}

		return R();
	}

	template<typename R, typename... Args>
	template<typename T>
	R signal<R(Args...)>::operator() (Args... args, const T& agg)
	{
		std::vector<R> result;

		if (!_locked)
		{
			result.reserve(_callbacks.size());

			for (auto iter = _callbacks.cbegin(); iter != _callbacks.cend(); ++iter)
			{
				const joint& jnt = *iter;

				if (!jnt.connection->locked)
				{
					result.push_back(std::move(jnt.callback(std::forward<Args>(args)...)));
				}
			}
		}

		return agg(std::move(result));
	}

	template<typename R, typename... Args>
	template<typename T, typename U, int... Ns>
	typename signal<R(Args...)>::callback_type signal<R(Args...)>::construct_mem_fn(const T& fn, U *p, int_sequence<Ns...>) const
	{
		return std::bind(fn, p, std::placeholder_lsignal<Ns>{}...);
	}

	template<typename R, typename... Args>
	std::shared_ptr<connection_data> signal<R(Args...)>::create_connection(callback_type&& fn, slot *owner)
	{
		std::shared_ptr<connection_data> connection = std::make_shared<connection_data>();

		if (owner != nullptr)
		{
			auto deleter = [this](std::shared_ptr<connection_data> connection)
			{
				destroy_connection(connection);
			};

			owner->_data = connection;
			owner->_deleter = std::move(deleter);
		}

		joint jnt;

		jnt.callback = std::move(fn);
		jnt.connection = connection;
		jnt.owner = owner;

		_callbacks.push_back(std::move(jnt));

		return connection;
	}

	template<typename R, typename... Args>
	void signal<R(Args...)>::destroy_connection(std::shared_ptr<connection_data> connection)
	{
		for (auto iter = _callbacks.begin(); iter != _callbacks.end(); ++iter)
		{
			const joint& jnt = *iter;

			if (jnt.connection == connection)
			{
				if (jnt.owner != nullptr)
				{
					jnt.owner->_data = nullptr;
					jnt.owner->_deleter = std::move(std::function<void(std::shared_ptr<connection_data>)>());
				}

				_callbacks.erase(iter);

				break;
			}
		}
	}

	template<typename R, typename... Args>
	connection signal<R(Args...)>::prepare_connection(connection&& conn)
	{
		conn._deleter = [this](std::shared_ptr<connection_data> connection)
		{
			destroy_connection(connection);
		};

		return conn;
	}
}

#endif // LSIGNAL_H

