#include "env_vars.hh"

#include "string.hh"

extern char **environ;

namespace Kakoune
{

EnvVarMap get_env_vars()
{
    EnvVarMap env_vars;
    for (char** it = environ; *it; ++it)
    {
        const char* name = *it;
        const char* value = name;
        while (*value != 0 and *value != '=')
            ++value;
        env_vars.append({{name, value}, (*value == '=') ? value+1 : String{}});
    }
    env_vars.sort();
    return env_vars;
}

}
