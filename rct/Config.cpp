#include "Config.h"

#include <ctype.h>
#include <getopt.h>
#include <stdlib.h>
#include <algorithm>
#include <memory>
#include <vector>

#include "StackBuffer.h"
#include "rct/Path.h"
#include "rct/Rct.h"
#include "rct/Value.h"

List<Config::OptionBase*> Config::sOptions;
bool Config::sAllowsFreeArgs = false;
List<Value> Config::sFreeArgs;

static inline Value createValue(Value::Type type, const char *val, bool *ok)
{
    return Value::create(val).convert(type, ok);
}

bool Config::parse(int argc, char **argv, const List<Path> &rcFiles)
{
    String error;
    Rct::findExecutablePath(argv[0]);
    List<String> args;
    args << argv[0];
    for (size_t i=0; i<rcFiles.size(); ++i) {
        FILE *f = fopen(rcFiles.at(i).constData(), "r");
        if (f) {
            char line[1024];
            int read;
            while ((read = Rct::readLine(f, line, sizeof(line))) != -1) {
                char *ch = line;
                while (isspace(*ch))
                    ++ch;
                if (*ch == '#')
                    continue;
                List<String> split = String(ch).split(' '); // ### quoting?
                if (!split.isEmpty()) {
                    String &first = split.first();
                    if (first.size() == 1 || (first.size() > 2 && first.at(1) == '=')) {
                        first.prepend('-');
                    } else {
                        first.prepend("--");
                    }
                    args += split;
                }
            }
            fclose(f);
        }
    }
    for (int i=1; i<argc; ++i)
        args.append(argv[i]);

    // ::error() << "parsing" << args;

    StackBuffer<128, char *> a(args.size());
    for (size_t i=0; i<args.size(); ++i) {
        a[i] = strdup(args.at(i).constData());
    }
    StackBuffer<128, option> options(sOptions.size() + 1);

    List<OptionBase*> optionPointers;
    for (size_t i=0; i<sOptions.size(); ++i) {
        OptionBase *opt = sOptions.at(i);
        if (opt->name) {
            option &o = options[optionPointers.size()];
            optionPointers.append(opt);
            o.name = opt->name;
            o.has_arg = (opt->defaultValue.type() == Value::Type_Boolean) ? no_argument : required_argument; // ### no optional arg?
            o.val = opt->shortOption;
            o.flag = nullptr;
        }
    }
    memset(&options[optionPointers.size()], 0, sizeof(option));
    const String shortOpts = Rct::shortOptions(options);

    bool ok = true;

    while (true) {
        int idx = -1;
        const int ret = getopt_long(args.size(), a, shortOpts.constData(), options, &idx);
        switch (ret) {
        case -1:
            goto done;
        case '?':
            ok = false;
            goto done;
        default:
            break;
        }
        // error() << optind << ret << optarg;
        // error("%c [%s] [%s]", ret, optarg, a[optind]);

        OptionBase *opt = nullptr;
        if (idx != -1) {
            opt = optionPointers[idx];
        } else {
            for (size_t i=0; i<optionPointers.size(); ++i) {
                if (optionPointers[i]->shortOption == ret) {
                    opt = optionPointers[i];
                    break;
                }
            }
        }
        if (!opt) {
            ok = false;
            goto done;
        }
        ++opt->count;
        if (optarg) {
            Value val;
            const char *arg = optarg;
            if (optarg[0] == '=' && strcmp(a[optind - 1], optarg)) {
                ++arg;
            }
            val = createValue(opt->type, arg, &ok);
            if (!ok) {
                error = String::format<128>("\"%s\" can not be converted to \"%s\" for %s",
                                            arg, Value::typeToString(opt->type),
                                            opt->name);
                goto done;
            }

            if (opt->defaultValue.type() == Value::Type_List) {
                List<Value> vals;
                vals << val;
                while (static_cast<size_t>(optind) < args.size() && a[optind][0] != '-' && (!opt->listCount || vals.size() < opt->listCount)) {
                    vals << createValue(opt->type, a[optind], &ok);
                    if (!ok) {
                        error = String::format<128>("\"%s\" can not be converted to \"%s\" for %s",
                                                    a[optind], Value::typeToString(opt->type),
                                                    opt->name);
                        goto done;
                    }
                    ++optind;
                }
                opt->value = vals;
                if (opt->listCount && vals.size() != opt->listCount) {
                    ok = false;
#ifdef _WIN32
                    error = String::format<128>("Too few values specified for %s. Wanted %Iu, got %Iu",
#else
                    error = String::format<128>("Too few values specified for %s. Wanted %zu, got %zu",
#endif

                                                opt->name, opt->listCount, vals.size());

                    goto done;
                }
            } else {
                opt->value = val;
            }
            if (!opt->validate(error)) {
                ok = false;
                goto done;
            }
        } else {
            assert(opt->defaultValue.type() == Value::Type_Boolean);
            // must be a toggle arg
            opt->value = Value(!opt->defaultValue.toBool());
        }
    }

done:
    while (static_cast<size_t>(optind) < args.size()) {
        sFreeArgs << a[optind++];
    }
    if (!sAllowsFreeArgs && !sFreeArgs.isEmpty()) {
        error = String::format<128>("Unexpected free args");
        ok = false;
    }

    for (size_t i=0; i<args.size(); ++i) {
        free(a[i]);
    }

    if (!ok) {
        if (!error.isEmpty()) {
            showHelp(stderr);
            fprintf(stderr, "%s\n", error.constData());
        }
    }
    return ok;
}

void Config::showHelp(FILE *f)
{
    List<String> out;
    int longest = 0;
    for (size_t i=0; i<sOptions.size(); ++i) {
        const OptionBase *option = sOptions.at(i);
        if (!option->name && !option->shortOption) {
            out.append(String());
        } else {
            out.append(String::format<64>("  %s%s%s%s",
                                          option->name ? String::format<4>("--%s", option-> name).constData() : "",
                                          option->name && option->shortOption ? "|" : "",
                                          option->shortOption ? String::format<2>("-%c", option->shortOption).constData() : "",
                                          option->defaultValue.type() == Value::Type_Boolean ? "" : " [arg] "));
            longest = std::max<int>(out[i].size(), longest);
        }
    }
    fprintf(f, "%s options...\n", Rct::executablePath().fileName());
    const int count = out.size();
    for (int i=0; i<count; ++i) {
        if (out.at(i).isEmpty()) {
            fprintf(f, "%s\n", sOptions.at(i)->description.constData());
        } else {
            fprintf(f, "%s%s %s\n",
                    out.at(i).constData(),
                    String(longest - out.at(i).size(), ' ').constData(),
                    sOptions.at(i)->description.constData());
        }
    }

}

void Config::clear()
{
    sOptions.deleteAll();
    sAllowsFreeArgs = false;
    sFreeArgs.clear();
}

struct Janitor {
    ~Janitor() { Config::clear(); }
} static sJanitor;
