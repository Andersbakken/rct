#include "rct/Config.h"

List<Config::Option> Config::sOptions;
bool Config::parse(int argc, char **argv)
{
    Rct::findExecutablePath(argv[0]);
    List<String> args;
    FILE *f = fopen((Path::home() + ".gelatorc").constData(), "r");
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
    }
    for (int i=0; i<argc; ++i)
        args.append(argv[i]);

    char **a = new char*[args.size() + 1];
    a[args.size()] = 0;
    for (int i=0; i<args.size(); ++i) {
        a[i + 1] = strdup(args.at(i).constData());
    }
    option *options = new option[sOptions.size() + 1];

    List<Option*> optionPointers;
    for (int i=0; i<sOptions.size(); ++i) {
        Option &opt = sOptions.at(i);
        if (opt.name) {
            option &o = options[optionPointers.size()];
            optionPointers.append(&opt);
            o.name = opt.name;
            o.has_arg = opt.defaultValue.type() == Value::Type_Boolean ? no_argument : required_argument; // ### no optional arg?
            o.val = opt.shortOption;
            o.flag = 0;
        }
    }
    memset(&options[optionPointers.size()], 0, sizeof(option));
    const String shortOpts = Rct::shortOptions(options);

    while (true) {
        int idx = -1;
        (void)getopt_long(argc, argv, shortOpts.constData(), options, &idx);
        if (idx == -1) {
            showHelp(stderr);
            return false;
        }
        Option *opt = optionPointers[idx];
        if (optarg) {
            opt->value = String(optarg);
        } else { // must be a toggle arg
            assert(opt->defaultValue.type() == Value::Type_Boolean);
            opt->value = Value(!opt->defaultValue.toBool());
        }
    }
    for (int i=0; i<args.size(); ++i) {
        free(a[i + 1]);
    }
    
    delete[] options;
    delete[] a;
}

void Config::showHelp(FILE *f)
{
    List<String> out;
    int longest = 0;
    for (int i=0; i<sOptions.size(); ++i) {
        const Option &option = sOptions.at(i);
        if (!option.name && !option.shortOption) {
            out.append(String());
        } else {
            out.append(String::format<64>("  %s%s%s%s",
                                          option.name ? String::format<4>("--%s", option. name).constData() : "",
                                          option.name && option.shortOption ? "|" : "",
                                          option.shortOption ? String::format<2>("-%c", option.shortOption).constData() : "",
                                          option.defaultValue.type() == Value::Type_Boolean ? "" : " [arg] "));
            longest = std::max<int>(out[i].size(), longest);
        }
    }
    fprintf(f, "%s options...\n", Rct::executablePath().fileName());
    const int count = out.size();
    for (int i=0; i<count; ++i) {
        if (out.at(i).isEmpty()) {
            fprintf(f, "%s\n", sOptions.at(i).description.constData());
        } else {
            fprintf(f, "%s%s %s\n",
                    out.at(i).constData(),
                    String(longest - out.at(i).size(), ' ').constData(),
                    sOptions.at(i).description.constData());
        }
    }

}
