#include "DB.h"
#include <rct/Log.h>
#include <string>
#include <memory>

int main(int argc, char **argv)
{
    const char *dbName = "db";
    bool read = true;
    for (int i=1; i<argc; ++i) {
        if (!strncmp("--db=", argv[i], 5)) {
            dbName = argv[i] + 5;
        } else if (!strcmp("--write", argv[i]) || !strcmp("-w", argv[i])) {
            read = false;
        } else if (!strcmp("--read", argv[i]) || !strcmp("-r", argv[i])) {
            read = true;
        } else {
            error() << "Unknown option" << argv[i];
            return 1;
        }
    }

    DB<String, std::shared_ptr<int> > db;
    db.open(dbName, 1);
    if (read) {
        std::shared_ptr<int> val = db["value"];
        printf("Read %d items (%d)\n", db.size(), val ? *val : -1);
    } else {
        auto scope = db.createWriteScope(100);
        db.set("value", std::make_shared<int>(12));
    }
    return 0;
};
