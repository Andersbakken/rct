#ifndef Config_h
#define Config_h

#include <stdio.h>
#include <rct/Path.h>
#include <rct/Rct.h>
#include <rct/Value.h>
#include <rct/String.h>
#include <getopt.h>

class Config
{
public:
    static bool parse(int argc, char **argv, const List<String> &rcFiles = List<String>());
    template <typename T> static void registerOption(const char *name, const String &description, const char shortOpt = '\0', const T &defaultValue = T())
    {
        const Value def = Value::create(defaultValue);
        const Option option = { name, shortOpt, description, def, Value() };
        sOptions.append(option);
    }

    static void registerOption(const char *name, const String &description, const char shortOpt = '\0')
    {
        const Option option = { name, shortOpt, description, Value(false), Value() };
        sOptions.append(option);
    }


    static bool isEnabled(const char *name, int *count = 0) // ### should return an int count of enabled-ness
    {
        const Option *opt = findOption(name);
        if (count)
            *count = opt ? opt->count : 0;
        return opt && opt->value.toBool();
    }

    template <typename T> static T value(const char *name, const T & defaultValue, bool *ok = 0)
    {
        const Option *opt = findOption(name);
        if (opt && !opt->value.isNull()) {
            if (ok)
                *ok = true;
            return opt->value.convert<T>();
        }
        if (ok)
            *ok = true;
        return defaultValue;
    }

    template <typename T> static T value(const char *name, bool *ok = 0)
    {
        const Option *opt = findOption(name);
        if (ok)
            *ok = false;
        if (opt) {
            if (opt->value.isNull()) {
                return opt->defaultValue.convert<T>(ok);
            } else {
                if (ok)
                    *ok = true;
                return opt->value.convert<T>();
            }
        }
        return T();
    }
    static void showHelp(FILE *f);
private:
    Config();
    ~Config();
    struct Option {
        const char *name;
        char shortOption;
        String description;
        Value defaultValue;
        Value value;
        int count;
    };
    static List<Option> sOptions;
    static const Option *findOption(const char *name)
    {
        assert(name);
        const int len = strlen(name);
        for (int i=0; i<sOptions.size(); ++i) {
            if (!strcmp(sOptions.at(i).name, name) || (len == 1 && *name == sOptions.at(i).shortOption)) {
                return &sOptions.at(i);
            }
        }
        return 0;
    }
};

#endif
