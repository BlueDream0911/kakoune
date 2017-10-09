#ifndef option_manager_hh_INCLUDED
#define option_manager_hh_INCLUDED

#include "completion.hh"
#include "exception.hh"
#include "hash_map.hh"
#include "option.hh"
#include "ranges.hh"
#include "utils.hh"
#include "vector.hh"
#include "string_utils.hh"

#include <memory>
#include <type_traits>

namespace Kakoune
{

class OptionManager;
class Context;

enum class OptionFlags
{
    None   = 0,
    Hidden = 1,
};

constexpr bool with_bit_ops(Meta::Type<OptionFlags>) { return true; }

class OptionDesc
{
public:
    OptionDesc(String name, String docstring, OptionFlags flags);

    const String& name() const { return m_name; }
    const String& docstring() const { return m_docstring; }

    OptionFlags flags() const { return m_flags; }

private:
    String m_name;
    String m_docstring;
    OptionFlags  m_flags;
};

class Option : public UseMemoryDomain<MemoryDomain::Options>
{
public:
    virtual ~Option() = default;

    template<typename T> const T& get() const;
    template<typename T> T& get_mutable();
    template<typename T> void set(const T& val, bool notify=true);
    template<typename T> bool is_of_type() const;

    virtual String get_as_string() const = 0;
    virtual void   set_from_string(StringView str) = 0;
    virtual void   add_from_string(StringView str) = 0;
    virtual void   update(const Context& context) = 0;

    virtual Option* clone(OptionManager& manager) const = 0;
    OptionManager& manager() const { return m_manager; }

    const String& name() const { return m_desc.name(); }
    const String& docstring() const { return m_desc.docstring(); }
    OptionFlags flags() const { return m_desc.flags(); }

protected:
    Option(const OptionDesc& desc, OptionManager& manager);

    OptionManager& m_manager;
    const OptionDesc& m_desc;
};

class OptionManagerWatcher
{
public:
    virtual void on_option_changed(const Option& option) = 0;
};

class OptionManager : private OptionManagerWatcher
{
public:
    OptionManager(OptionManager& parent);
    ~OptionManager();

    Option& operator[] (StringView name);
    const Option& operator[] (StringView name) const;
    Option& get_local_option(StringView name);

    void unset_option(StringView name);

    using OptionList = Vector<const Option*>;
    OptionList flatten_options() const;

    void register_watcher(OptionManagerWatcher& watcher) const;
    void unregister_watcher(OptionManagerWatcher& watcher) const;

    void on_option_changed(const Option& option) override;
private:
    OptionManager()
        : m_parent(nullptr) {}
    // the only one allowed to construct a root option manager
    friend class Scope;
    friend class OptionsRegistry;

    HashMap<StringView, std::unique_ptr<Option>, MemoryDomain::Options> m_options;
    OptionManager* m_parent;

    mutable Vector<OptionManagerWatcher*, MemoryDomain::Options> m_watchers;
};

template<typename T>
class TypedOption : public Option
{
public:
    TypedOption(OptionManager& manager, const OptionDesc& desc, const T& value)
        : Option(desc, manager), m_value(value) {}

    void set(T value, bool notify = true)
    {
        validate(value);
        if (m_value != value)
        {
            m_value = std::move(value);
            if (notify)
                manager().on_option_changed(*this);
        }
    }
    const T& get() const { return m_value; }
    T& get_mutable() { return m_value; }

    String get_as_string() const override
    {
        return option_to_string(m_value);
    }
    void set_from_string(StringView str) override
    {
        T val;
        option_from_string(str, val);
        set(std::move(val));
    }
    void add_from_string(StringView str) override
    {
        if (option_add(m_value, str))
            m_manager.on_option_changed(*this);
    }
    void update(const Context& context) override
    {
        option_update(m_value, context);
    }
private:
    virtual void validate(const T& value) const {}
    T m_value;
};

template<typename T, void (*validator)(const T&)>
class TypedCheckedOption : public TypedOption<T>
{
    using TypedOption<T>::TypedOption;

    Option* clone(OptionManager& manager) const override
    {
        return new TypedCheckedOption{manager, this->m_desc, this->get()};
    }

    void validate(const T& value) const override { if (validator != nullptr) validator(value); }
};

template<typename T> const T& Option::get() const
{
    auto* typed_opt = dynamic_cast<const TypedOption<T>*>(this);
    if (not typed_opt)
        throw runtime_error(format("option '{}' is not of type '{}'", name(), typeid(T).name()));
    return typed_opt->get();
}

template<typename T> T& Option::get_mutable()
{
    return const_cast<T&>(get<T>());
}

template<typename T> void Option::set(const T& val, bool notify)
{
    auto* typed_opt = dynamic_cast<TypedOption<T>*>(this);
    if (not typed_opt)
        throw runtime_error(format("option '{}' is not of type '{}'", name(), typeid(T).name()));
    return typed_opt->set(val, notify);
}

template<typename T> bool Option::is_of_type() const
{
    return dynamic_cast<const TypedOption<T>*>(this) != nullptr;
}

class OptionsRegistry
{
public:
    OptionsRegistry(OptionManager& global_manager) : m_global_manager(global_manager) {}

    template<typename T, void (*validator)(const T&) = nullptr>
    Option& declare_option(StringView name, StringView docstring,
                           const T& value,
                           OptionFlags flags = OptionFlags::None)
    {
        auto is_not_identifier = [](char c) {
            return (c < 'a' or c > 'z') and
                   (c < 'A' or c > 'Z') and
                   (c < '0' or c > '9') and c != '_';
        };

        if (contains_that(name, is_not_identifier))
            throw runtime_error{format("name '{}' contains char out of [a-zA-Z0-9_]", name)};

        auto& opts = m_global_manager.m_options;
        auto it = opts.find(name);
        if (it != opts.end())
        {
            if (it->value->is_of_type<T>() and it->value->flags() == flags)
                return *it->value;
            throw runtime_error{format("option '{}' already declared with different type or flags", name)};
        }
        String doc =  docstring.empty() ? format("[{}]", option_type_name(Meta::Type<T>{}))
                                        : format("[{}] - {}", option_type_name(Meta::Type<T>{}), docstring);
        m_descs.emplace_back(new OptionDesc{name.str(), std::move(doc), flags});
        return *opts.insert({m_descs.back()->name(),
                             std::make_unique<TypedCheckedOption<T, validator>>(m_global_manager, *m_descs.back(), value)});
    }

    const OptionDesc* option_desc(StringView name) const
    {
        auto it = find_if(m_descs,
                          [&name](const std::unique_ptr<const OptionDesc>& opt)
                          { return opt->name() == name; });
        return it != m_descs.end() ? it->get() : nullptr;
    }

    bool option_exists(StringView name) const { return option_desc(name) != nullptr; }

    CandidateList complete_option_name(StringView prefix, ByteCount cursor_pos) const;
private:
    OptionManager& m_global_manager;
    Vector<std::unique_ptr<const OptionDesc>, MemoryDomain::Options> m_descs;
};

}

#endif // option_manager_hh_INCLUDED
