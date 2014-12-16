#ifndef utils_hh_INCLUDED
#define utils_hh_INCLUDED

#include "assert.hh"
#include "exception.hh"

#include <algorithm>
#include <memory>
#include <vector>

namespace Kakoune
{

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// *** Singleton ***
//
// Singleton helper class, every singleton type T should inherit
// from Singleton<T> to provide a consistent interface.
template<typename T>
class Singleton
{
public:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    static T& instance()
    {
        kak_assert (ms_instance);
        return *ms_instance;
    }

    static bool has_instance()
    {
        return ms_instance != nullptr;
    }

protected:
    Singleton()
    {
        kak_assert(not ms_instance);
        ms_instance = static_cast<T*>(this);
    }

    ~Singleton()
    {
        kak_assert(ms_instance == this);
        ms_instance = nullptr;
    }

private:
    static T* ms_instance;
};

template<typename T>
T* Singleton<T>::ms_instance = nullptr;

// *** Containers helpers ***

template<typename Container>
struct ReversedContainer
{
    ReversedContainer(Container& container) : container(container) {}
    Container& container;

    decltype(container.rbegin()) begin() { return container.rbegin(); }
    decltype(container.rend())   end()   { return container.rend(); }
};

template<typename Container>
auto begin(ReversedContainer<Container>& c) -> decltype(c.begin())
{
    return c.begin();
}

template<typename Container>
auto end(ReversedContainer<Container>& c) -> decltype(c.end())
{
    return c.end();
}

template<typename Container>
ReversedContainer<Container> reversed(Container&& container)
{
    return ReversedContainer<Container>(container);
}

// Todo: move that into the following functions once we can remove the decltype
//       return type.
using std::begin;
using std::end;

template<typename Container, typename T>
auto find(Container&& container, const T& value) -> decltype(begin(container))
{
    return std::find(begin(container), end(container), value);
}

template<typename Container, typename T>
auto find_if(Container&& container, T op) -> decltype(begin(container))
{
    return std::find_if(begin(container), end(container), op);
}

template<typename Container, typename T>
bool contains(Container&& container, const T& value)
{
    return find(container, value) != end(container);
}

template<typename T, typename U>
void unordered_erase(std::vector<T>& vec, U&& value)
{
    auto it = find(vec, std::forward<U>(value));
    if (it != vec.end())
    {
        std::swap(vec.back(), *it);
        vec.pop_back();
    }
}

template<typename Iterator, typename EndIterator, typename T>
void skip_while(Iterator& it, const EndIterator& end, T condition)
{
    while (it != end and condition(*it))
        ++it;
}

template<typename Iterator, typename BeginIterator, typename T>
void skip_while_reverse(Iterator& it, const BeginIterator& begin, T condition)
{
    while (it != begin and condition(*it))
        --it;
}

// *** On scope end ***
//
// on_scope_end provides a way to register some code to be
// executed when current scope closes.
//
// usage:
// auto cleaner = on_scope_end([]() { ... });
//
// This permits to cleanup c-style resources without implementing
// a wrapping class
template<typename T>
class OnScopeEnd
{
public:
    OnScopeEnd(T func) : m_func(std::move(func)) {}
    ~OnScopeEnd() { m_func(); }
private:
    T m_func;
};

template<typename T>
OnScopeEnd<T> on_scope_end(T t)
{
    return OnScopeEnd<T>(t);
}

// *** Misc helper functions ***

template<typename T>
bool operator== (const std::unique_ptr<T>& lhs, T* rhs)
{
    return lhs.get() == rhs;
}

template<typename T>
const T& clamp(const T& val, const T& min, const T& max)
{
    return (val < min ? min : (val > max ? max : val));
}

template<typename T>
bool is_in_range(const T& val, const T& min, const T& max)
{
    return min <= val and val <= max;
}

}

#endif // utils_hh_INCLUDED
