#include <iostream>
#include <utility>
#include <string>
#include <vector>
#include <fstream>

template <typename T>
[[maybe_unused]] void mutate(T&&) noexcept;

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

	String(String&& str) noexcept(noexcept(m_length > 0))
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
	T& operator=(T&& a) noexcept(noexcept(a.str.size() > 0))
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
		if (&num == this)
		{
			return *this;
		}

		std::cout << "Copy Operator\n";
		a = num.a;
		return *this;
	}

	B& operator=(B&& num) noexcept
	{
		if (&num == this)
		{
			return *this;
		}

		std::cout << "Move Operator\n";
		a = std::move(num.a);
		return *this;
	}
};

B makeB(B b)
{
	B newB;

	//Even though newB is almost at end of lifetime, it still passes by copy just in case we do not expressly want to steal b's value.
	newB = b;

	//Nearing end of lifetime. Move automatically rather than returning a copy.
	return newB;
}

int main()
{
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
	{
		std::cout << i << '\n';
	}
}

//Unused at the moment.
template <typename T, typename = std::is_nothrow_convertible<T, char*>>
void mutate(T&& arg) noexcept
{
	std::string someStr{ std::move(arg) };
}