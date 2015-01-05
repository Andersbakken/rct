#include <util/build_version.h>

const char* rocksdb_build_git_sha = "rocksdb_build_git_sha:${foobar}";
const char* rocksdb_build_git_datetime = "rocksdb_build_git_datetime:$(foobar)";
const char* rocksdb_build_compile_date = __DATE__;
const char* rocksdb_build_compile_time = __TIME__;
