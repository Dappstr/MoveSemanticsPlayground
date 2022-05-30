#include <iostream>
#include <utility>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <thread>
#include <array>
#include <chrono>
#include <exception>
#include <type_traits>
#include <map>

namespace
{
	template <typename T, typename = std::is_nothrow_convertible<T, char*>>
	[[maybe_unused]] void mutate(T&& arg) noexcept
	{
		std::string someStr{ std::move(arg) };
	}

	void checkValueCategory(auto expression)
	{
		if (!std::is_reference_v<decltype(expression)>)
			std::cout << "prvalue\n";
		else if (std::is_lvalue_reference_v<decltype(expression)>)
			std::cout << "lvalue\n";
		else if (std::is_rvalue_reference_v<decltype(expression)>)
			std::cout << "xvalue\n";
		else if (!std::is_lvalue_reference_v<decltype(expression)>)
			std::cout << "rvalue\n";
	}

	namespace move_constructible_wrapper
	{
		//is_nothrow_movable wrapper for abstract classes
		template <typename AbstractClass>
		struct Wrapper : AbstractClass
		{
			using AbstractClass::AbstractClass;
			//...
		};

		template <typename T>
		static constexpr inline bool is_nothrow_movable_v = std::is_nothrow_move_constructible_v<Wrapper<T>> ;
	}
}

class String
{
private:
	char* m_str{};
	size_t m_length{};

public:
	String(const char* strLiteral = "")
	{
		m_length = strlen(strLiteral);
		m_str = new char[m_length+1];
		memcpy(m_str, strLiteral, m_length);
		m_str[m_length] = 0;
	}

	String(const String& src) noexcept(noexcept(m_length > 0))
		:m_length(src.m_length)
	{
		std::cout << "Calling copy constructor\n";
		delete[] m_str;
		m_str = new char[m_length+1];
		memcpy(m_str, src.m_str, m_length + 1);
	}

	String(String&& str) noexcept(noexcept(m_length > 0) && std::is_nothrow_constructible_v<String>)
		:m_length(std::move(str.m_length)), m_str(std::move(str.m_str))
	{
		std::cout << "Calling move constructor\n";
		str.m_str = nullptr;
		str.m_length = 0;
	}

	String& operator=(String&& str) noexcept
	{
		std::cout << "Move assignment overload\n";
		m_length = std::move(str.m_length);
		m_str = std::move(str.m_str);
		return *this;
	}

	String& operator=(const String& str) noexcept
	{
		std::cout << "Copy assignment overload\n";
		m_length = str.m_length;
		m_str = str.m_str;
		return *this;
	}

	friend std::ostream& operator << (std::ostream& out, const String& str)
	{
		out << str.m_str;
		return out;
	}

	~String() { delete[] m_str; }
};

class A
{
private:
	std::string str{};

public:
	A() = default;

	A(const std::string& temp)
		:str(temp)
	{}

	template <typename T>
	T& operator=(T&& a) noexcept(noexcept(a.str.size() > 0) && std::is_nothrow_constructible_v<T>)
	{
		str = std::move(a.str);
		return *this;
	}

	friend std::ostream& operator << (std::ostream& out, const A& a)
	{
		out << a.str;
		return out;
	}

	~A() = default;
};

class B
{
private:
	int a{};

public:
	explicit B() = default;
	explicit B(int num)
		:a(num)
	{}

	B(const B& b)
		:a(b.a)
	{}

	B& operator=(const B& num) noexcept
	{
		if (&num == this) return *this;

		std::cout << "Copy Operator\n";
		a = num.a;
		return *this;
	}

	B& operator=(B&& num) noexcept
	{
		if (&num == this) return *this;

		std::cout << "Move Operator\n";
		a = std::move(num.a);
		return *this;
	}

	~B() = default;
};

B makeB(B b)
{
	B newB;

	//Even though newB is almost at end of lifetime, it still passes by copy just in case we do not expressly want to steal b's value.
	newB = b;

	//Nearing end of lifetime. Move automatically rather than returning a copy.
	return newB;
}

class Abstract
{
private:
	size_t num{};

public:
	virtual void func() = 0; //Pure virtual function example
	virtual ~Abstract() = default;

protected:

	//Allow copy/move construction
	Abstract(const Abstract&) = default;
	Abstract(Abstract&&) = default;

	//Disallow copy/move assignment because of object slicing
	Abstract& operator=(const Abstract&) = delete;
	Abstract& operator=(Abstract&&) = delete;

};

//Reference qualifiers
class RefQual
{
private:
	std::string m_name{};

public:
	RefQual(std::string&& name)
		:m_name(std::move(name))
	{}

	std::string getByVal() && //Will handle temporaries
	{
		return std::move(m_name); //Normally returning with an std::move() is bad since it'll stop any attempt at RVO/NRVO, however, in this case, we're not returning a local variable, we're returning a member. So It's perfectly fine.
	}

	const std::string& getByRef() const& //Will handle objects that are not moved
	{
		return m_name;
	}

	friend std::ostream& operator<<(std::ostream& out, const RefQual& instance)
	{
		out << instance.m_name;
		return out;
	}
};

RefQual getName()
{
	RefQual nm { "Super Long String To Avoid SSO" };
	return nm;
}

class Tasks
{
private:

	//std::jthread has better RAII principle and will automatically join rather than relying on std::thread using join() before the destructor is called.
	std::array<std::thread, 10> m_threadTasks{};
	size_t m_numThreads{ 0 };

public:

	Tasks() = default;
	Tasks(Tasks&&) = default;
	Tasks& operator=(Tasks&&) = default;

	template <typename T>
	void startTask(T op)
	{
		m_threadTasks[m_numThreads] = std::thread{ std::move(op) };
		++m_numThreads;
	}

	~Tasks()
	{
		for (size_t i = 0; i < m_numThreads; ++i)
		{
			if (m_threadTasks[i].joinable())
				m_threadTasks[i].join();
			else
				std::cout << "Fatal error! Cannot join thread!\n" ;
		}
	}
};

struct S
{

	//std::move() copies or moves static or reference members depending on how the object/member is passed.
	inline static std::string m_statStr;
	std::string& m_str = m_statStr;
};

struct Foo
{
	Foo() = default;
	
	Foo(int i, bool b)
		:m_i(i), m_b(b)
	{}
		
	Foo(int i, bool b, float f)
		:m_i(i), m_b(b), m_f(f)
	{}

	Foo(int i, bool b, float f, const std::vector<int>& v)
		:m_i(i), m_b(b), m_f(f), m_v(v)
	{}

	Foo(int i, bool b, float f, std::vector<int>&& v)
		:m_i(i), m_b(b), m_f(f), m_v(std::move(v))
	{}

private:
	int m_i					{};
	bool m_b				{};
	float m_f				{};
	std::vector<int> m_v	{};
};

struct FrwdMe
{
	template <typename... Args>
	void addFoo(Args... args)
	{
		v.emplace_back(std::forward<Args>(args)...); //std::forward will ensure that our arguments keep their value categories so that we may pass in both lvalues and rvalues.
	}

private:
	std::vector<Foo> v{};
};


template <typename T>
auto make_pair(T&& t)
{
	return std::make_tuple(std::forward<T>(t), std::forward<T>(t)); // BAD
}

template <typename T>
auto processValue(T& arg)
{
	std::cout << "Non const reference\n";
}

template <typename T>
auto processValue(const T& arg)
{
	std::cout << "Const reference\n";
}

template <typename T>
auto processValue(T&& arg)
{
	std::cout << "Forwarding reference\n";
}


template <typename T>
void passForward(T&& arg)
{
	processValue(std::forward<T>(arg));
}

int main()
{
	{
		auto perfReturnLambda = [](auto f, auto&&... args) -> decltype(auto)
		{
			decltype(auto ret) = f(std::forward<decltype(args)>(args)...);
			
			//...

			if constexpr (std::is_rvalue_reference_v<decltype(args))
			{
				return std::move(ret);
			}
			else
			{
				return ret;
			}
		};
	}

	{
		int x{};
		const int y{};

		passForward(x);
		passForward(y);
		passForward(Foo());
		passForward(std::move(x));
		passForward(std::move(y));

	}

	int x{4};
	std::tuple<int, int> tp{make_pair(std::move(x))};

	std::cout << std::get<0>(tp) << std::get<1>(tp) << '\n';
	
	FrwdMe f;
	std::vector<int> v{ 2, 4, 6, 8 ,10 };
	
	f.addFoo(1, true, 3.1415, std::move(v));
	f.addFoo(Foo(2, true, 5.5457123));
	f.addFoo(std::move(Foo(3, false, 7.235431)));

	S sObject;

	std::vector<std::string> Ss{};

	Ss.push_back(std::move(sObject).m_str); //Copies
	Ss.push_back(std::move(sObject).m_statStr); //Copies

	Ss.push_back(std::move(sObject.m_str)); //Moves
	Ss.push_back(std::move(sObject.m_statStr)); //Moves

	std::vector<std::string> refs{};
	RefQual obj{ "Testeringtestingtesterthatistesting" };
	refs.push_back(obj.getByRef()); // const &
	//Or even
	refs.push_back(std::move(obj).getByVal()); // &&

	for (auto c : getName().getByVal()) //Will prefer rvalue reference since a temporary is being used
	{
		if (c == ' ')
			std::cout << "Space";
		else
			std::cout << c;
	}
	std::cout << '\n';

	for (const auto& i : refs) //Will prefer const lvalue reference for object
	{
		std::cout << i << '\n';
	}

	B bTwo;
	{
		B bObj(1);

		//Lifetime about to end, passed by value, so it WILL be moved since makeB creates/copies a value.
		bObj = makeB(std::move(bObj));
	}

	std::vector<std::string> names{};
	std::string name{};

	std::ifstream inFile{"test.txt"};

	while (!inFile.eof())
	{
		//MSVC doesn't like this even though it's perfectly logical. We do not need to create a copy every time we want to move a value from inFile into the vector.
		while (std::getline(inFile, name))
		{
			names.push_back(std::move(name));
		}
	}

	for (auto& i : names)
	{ std::cout << i << '\n'; }

	try
	{
		Tasks task{};
		task.startTask(
			[] {
				std::this_thread::sleep_for(std::chrono::seconds{ 2 });
				std::cout << "\nThread: " << std::this_thread::get_id() << " done\n";
			});

		task.startTask(
			[] {
				std::cout << "\nThread: " << std::this_thread::get_id() << " done\n";
			});

		Tasks other{ std::move(task) };

	}
	catch (const std::exception& e) { std::cout << "CAUGHT EXCEPTION: " << e.what() << '\n'; }

}
