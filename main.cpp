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

namespace
{
	template <typename T, typename = std::is_nothrow_convertible<T, char*>>
	[[maybe_unused]] void mutate(T&& arg) noexcept
	{
		std::string someStr{ std::move(arg) };
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

int main()
{
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
