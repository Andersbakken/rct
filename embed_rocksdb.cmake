cmake_minimum_required(VERSION 2.8)

include_directories(SYSTEM ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/)
include_directories(SYSTEM ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/include/)
add_definitions(-DROCKSDB_PLATFORM_POSIX)
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  add_definitions(-DOS_MACOSX)
endif ()

if (NOT snappy_ROOT)
    find_path(snappy_INCLUDE_DIRS snappy.h)
    find_library(snappy_LIBRARIES NAMES snappy)
else ()
    find_path(snappy_INCLUDE_DIRS snappy.h NO_DEFAULT_PATH PATHS ${snappy_ROOT})
    find_library(snappy_LIBRARIES NAMES snappy NO_DEFAULT_PATH PATHS ${snappy_ROOT})
endif ()

if (snappy_INCLUDE_DIRS AND snappy_LIBRARIES)
  add_definitions(-DSNAPPY)
  set(DB_LIBS bz2 z pthread ${snappy_LIBRARIES})
  include_directories(SYSTEM ${snappy_INCLUDE_DIRS})
else ()
  set(DB_LIBS bz2 z pthread)
endif ()

set(RCT_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/rocksdb_build_version.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/plain_table_builder.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/block_hash_index.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/table_properties.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/plain_table_key_coding.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/cuckoo_table_reader.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/format.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/cuckoo_table_factory.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/mock_table.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/table_reader_bench.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/bloom_block.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/plain_table_reader.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/block_based_table_factory.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/adaptive_table_factory.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/block_based_filter_block.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/block_prefix_index.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/get_context.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/meta_blocks.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/plain_table_factory.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/block.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/cuckoo_table_builder.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/full_filter_block.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/iterator.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/block_builder.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/flush_block_policy.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/two_level_iterator.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/block_based_table_builder.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/merger.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/plain_table_index.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/table/block_based_table_reader.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/port/port_posix.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/port/stack_trace.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/db_impl_debug.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/version_edit.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/log_reader.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/write_controller.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/wal_manager.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/memtable_list.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/flush_scheduler.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/db_bench.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/db_impl.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/dbformat.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/memtable_allocator.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/compaction.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/merge_helper.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/db_iter.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/log_and_apply_bench.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/column_family.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/write_batch.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/c.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/builder.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/version_set.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/file_indexer.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/flush_job.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/merge_operator.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/compaction_picker.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/repair.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/filename.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/log_writer.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/write_thread.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/db_impl_readonly.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/compaction_job.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/internal_stats.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/version_builder.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/db_filesnapshot.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/transaction_log_impl.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/memtable.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/forward_iterator.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/table_cache.cc
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/db/table_properties_collector.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/utilities/backupable/backupable_db.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/utilities/spatialdb/spatial_db.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/utilities/geodb/geodb_impl.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/utilities/merge_operators/uint64add.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/utilities/merge_operators/put.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/utilities/merge_operators/string_append/stringappend.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/utilities/merge_operators/string_append/stringappend2.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/utilities/write_batch_with_index/write_batch_with_index.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/utilities/ttl/db_ttl_impl.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/utilities/leveldb_options/leveldb_options.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/utilities/checkpoint/checkpoint.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/utilities/redis/redis_lists.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/utilities/compacted_db/compacted_db_impl.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/utilities/document/json_document.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/utilities/document/document_db.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/hash_cuckoo_rep.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/coding.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/options_helper.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/blob_store.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/slice.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/thread_status_updater.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/bloom.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/string_util.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/env_posix.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/ldb_tool.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/benchharness.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/histogram.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/perf_context.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/crc32c.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/thread_status_util.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/arena.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/logging.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/hash_skiplist_rep.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/file_util.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/env_hdfs.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/sst_dump_tool.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/status.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/sync_point.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/cache.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/hash.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/thread_local.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/mutable_cf_options.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/options.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/murmurhash.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/skiplistrep.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/dynamic_bloom.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/auto_roll_logger.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/hash_linklist_rep.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/cache_bench.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/vectorrep.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/env.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/rate_limiter.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/log_buffer.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/iostats_context.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/xxhash.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/comparator.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/mock_env.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/log_write_bench.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/statistics.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/filter_policy.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/options_builder.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/ldb_cmd.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/db_info_dumper.cc
${CMAKE_CURRENT_LIST_DIR}/3rdparty/rocksdb/util/thread_status_updater_debug.cc)

