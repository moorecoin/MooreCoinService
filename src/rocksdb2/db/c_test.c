/* copyright (c) 2011 the leveldb authors. all rights reserved.
   use of this source code is governed by a bsd-style license that can be
   found in the license file. see the authors file for names of contributors. */

#include "rocksdb/c.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

const char* phase = "";
static char dbname[200];

static void startphase(const char* name) {
  fprintf(stderr, "=== test %s\n", name);
  phase = name;
}

static const char* gettempdir(void) {
    const char* ret = getenv("test_tmpdir");
    if (ret == null || ret[0] == '\0')
        ret = "/tmp";
    return ret;
}

#define checknoerror(err)                                               \
  if ((err) != null) {                                                  \
    fprintf(stderr, "%s:%d: %s: %s\n", __file__, __line__, phase, (err)); \
    abort();                                                            \
  }

#define checkcondition(cond)                                            \
  if (!(cond)) {                                                        \
    fprintf(stderr, "%s:%d: %s: %s\n", __file__, __line__, phase, #cond); \
    abort();                                                            \
  }

static void checkequal(const char* expected, const char* v, size_t n) {
  if (expected == null && v == null) {
    // ok
  } else if (expected != null && v != null && n == strlen(expected) &&
             memcmp(expected, v, n) == 0) {
    // ok
    return;
  } else {
    fprintf(stderr, "%s: expected '%s', got '%s'\n",
            phase,
            (expected ? expected : "(null)"),
            (v ? v : "(null"));
    abort();
  }
}

static void free(char** ptr) {
  if (*ptr) {
    free(*ptr);
    *ptr = null;
  }
}

static void checkget(
    rocksdb_t* db,
    const rocksdb_readoptions_t* options,
    const char* key,
    const char* expected) {
  char* err = null;
  size_t val_len;
  char* val;
  val = rocksdb_get(db, options, key, strlen(key), &val_len, &err);
  checknoerror(err);
  checkequal(expected, val, val_len);
  free(&val);
}

static void checkgetcf(
    rocksdb_t* db,
    const rocksdb_readoptions_t* options,
    rocksdb_column_family_handle_t* handle,
    const char* key,
    const char* expected) {
  char* err = null;
  size_t val_len;
  char* val;
  val = rocksdb_get_cf(db, options, handle, key, strlen(key), &val_len, &err);
  checknoerror(err);
  checkequal(expected, val, val_len);
  free(&val);
}


static void checkiter(rocksdb_iterator_t* iter,
                      const char* key, const char* val) {
  size_t len;
  const char* str;
  str = rocksdb_iter_key(iter, &len);
  checkequal(key, str, len);
  str = rocksdb_iter_value(iter, &len);
  checkequal(val, str, len);
}

// callback from rocksdb_writebatch_iterate()
static void checkput(void* ptr,
                     const char* k, size_t klen,
                     const char* v, size_t vlen) {
  int* state = (int*) ptr;
  checkcondition(*state < 2);
  switch (*state) {
    case 0:
      checkequal("bar", k, klen);
      checkequal("b", v, vlen);
      break;
    case 1:
      checkequal("box", k, klen);
      checkequal("c", v, vlen);
      break;
  }
  (*state)++;
}

// callback from rocksdb_writebatch_iterate()
static void checkdel(void* ptr, const char* k, size_t klen) {
  int* state = (int*) ptr;
  checkcondition(*state == 2);
  checkequal("bar", k, klen);
  (*state)++;
}

static void cmpdestroy(void* arg) { }

static int cmpcompare(void* arg, const char* a, size_t alen,
                      const char* b, size_t blen) {
  int n = (alen < blen) ? alen : blen;
  int r = memcmp(a, b, n);
  if (r == 0) {
    if (alen < blen) r = -1;
    else if (alen > blen) r = +1;
  }
  return r;
}

static const char* cmpname(void* arg) {
  return "foo";
}

// custom filter policy
static unsigned char fake_filter_result = 1;
static void filterdestroy(void* arg) { }
static const char* filtername(void* arg) {
  return "testfilter";
}
static char* filtercreate(
    void* arg,
    const char* const* key_array, const size_t* key_length_array,
    int num_keys,
    size_t* filter_length) {
  *filter_length = 4;
  char* result = malloc(4);
  memcpy(result, "fake", 4);
  return result;
}
static unsigned char filterkeymatch(
    void* arg,
    const char* key, size_t length,
    const char* filter, size_t filter_length) {
  checkcondition(filter_length == 4);
  checkcondition(memcmp(filter, "fake", 4) == 0);
  return fake_filter_result;
}

// custom compaction filter
static void cfilterdestroy(void* arg) {}
static const char* cfiltername(void* arg) { return "foo"; }
static unsigned char cfilterfilter(void* arg, int level, const char* key,
                                   size_t key_length,
                                   const char* existing_value,
                                   size_t value_length, char** new_value,
                                   size_t* new_value_length,
                                   unsigned char* value_changed) {
  if (key_length == 3) {
    if (memcmp(key, "bar", key_length) == 0) {
      return 1;
    } else if (memcmp(key, "baz", key_length) == 0) {
      *value_changed = 1;
      *new_value = "newbazvalue";
      *new_value_length = 11;
      return 0;
    }
  }
  return 0;
}

static void cfilterfactorydestroy(void* arg) {}
static const char* cfilterfactoryname(void* arg) { return "foo"; }
static rocksdb_compactionfilter_t* cfiltercreate(
    void* arg, rocksdb_compactionfiltercontext_t* context) {
  return rocksdb_compactionfilter_create(null, cfilterdestroy, cfilterfilter,
                                         cfiltername);
}

static rocksdb_t* checkcompaction(rocksdb_t* db, rocksdb_options_t* options,
                                  rocksdb_readoptions_t* roptions,
                                  rocksdb_writeoptions_t* woptions) {
  char* err = null;
  db = rocksdb_open(options, dbname, &err);
  checknoerror(err);
  rocksdb_put(db, woptions, "foo", 3, "foovalue", 8, &err);
  checknoerror(err);
  checkget(db, roptions, "foo", "foovalue");
  rocksdb_put(db, woptions, "bar", 3, "barvalue", 8, &err);
  checknoerror(err);
  checkget(db, roptions, "bar", "barvalue");
  rocksdb_put(db, woptions, "baz", 3, "bazvalue", 8, &err);
  checknoerror(err);
  checkget(db, roptions, "baz", "bazvalue");

  // force compaction
  rocksdb_compact_range(db, null, 0, null, 0);
  // should have filtered bar, but not foo
  checkget(db, roptions, "foo", "foovalue");
  checkget(db, roptions, "bar", null);
  checkget(db, roptions, "baz", "newbazvalue");
  return db;
}

// custom compaction filter v2.
static void compactionfilterv2destroy(void* arg) { }
static const char* compactionfilterv2name(void* arg) {
  return "testcompactionfilterv2";
}
static void compactionfilterv2filter(
    void* arg, int level, size_t num_keys,
    const char* const* keys_list, const size_t* keys_list_sizes,
    const char* const* existing_values_list, const size_t* existing_values_list_sizes,
    char** new_values_list, size_t* new_values_list_sizes,
    unsigned char* to_delete_list) {
  size_t i;
  for (i = 0; i < num_keys; i++) {
    // if any value is "gc", it's removed.
    if (existing_values_list_sizes[i] == 2 && memcmp(existing_values_list[i], "gc", 2) == 0) {
      to_delete_list[i] = 1;
    } else if (existing_values_list_sizes[i] == 6 && memcmp(existing_values_list[i], "gc all", 6) == 0) {
      // if any value is "gc all", all keys are removed.
      size_t j;
      for (j = 0; j < num_keys; j++) {
        to_delete_list[j] = 1;
      }
      return;
    } else if (existing_values_list_sizes[i] == 6 && memcmp(existing_values_list[i], "change", 6) == 0) {
      // if value is "change", set changed value to "changed".
      size_t len;
      len = strlen("changed");
      new_values_list[i] = malloc(len);
      memcpy(new_values_list[i], "changed", len);
      new_values_list_sizes[i] = len;
    } else {
      // otherwise, no keys are removed.
    }
  }
}

// custom prefix extractor for compaction filter v2 which extracts first 3 characters.
static void cfv2prefixextractordestroy(void* arg) { }
static char* cfv2prefixextractortransform(void* arg, const char* key, size_t length, size_t* dst_length) {
  // verify keys are maximum length 4; this verifies fix for a
  // prior bug which was passing the rocksdb-encoded key with
  // logical timestamp suffix instead of parsed user key.
  if (length > 4) {
    fprintf(stderr, "%s:%d: %s: key %s is not user key\n", __file__, __line__, phase, key);
    abort();
  }
  *dst_length = length < 3 ? length : 3;
  return (char*)key;
}
static unsigned char cfv2prefixextractorindomain(void* state, const char* key, size_t length) {
  return 1;
}
static unsigned char cfv2prefixextractorinrange(void* state, const char* key, size_t length) {
  return 1;
}
static const char* cfv2prefixextractorname(void* state) {
  return "testcfv2prefixextractor";
}

// custom compaction filter factory v2.
static void compactionfilterfactoryv2destroy(void* arg) {
  rocksdb_slicetransform_destroy((rocksdb_slicetransform_t*)arg);
}
static const char* compactionfilterfactoryv2name(void* arg) {
  return "testcompactionfilterfactoryv2";
}
static rocksdb_compactionfilterv2_t* compactionfilterfactoryv2create(
    void* state, const rocksdb_compactionfiltercontext_t* context) {
  return rocksdb_compactionfilterv2_create(state, compactionfilterv2destroy,
                                           compactionfilterv2filter,
                                           compactionfilterv2name);
}

// custom merge operator
static void mergeoperatordestroy(void* arg) { }
static const char* mergeoperatorname(void* arg) {
  return "testmergeoperator";
}
static char* mergeoperatorfullmerge(
    void* arg,
    const char* key, size_t key_length,
    const char* existing_value, size_t existing_value_length,
    const char* const* operands_list, const size_t* operands_list_length,
    int num_operands,
    unsigned char* success, size_t* new_value_length) {
  *new_value_length = 4;
  *success = 1;
  char* result = malloc(4);
  memcpy(result, "fake", 4);
  return result;
}
static char* mergeoperatorpartialmerge(
    void* arg,
    const char* key, size_t key_length,
    const char* const* operands_list, const size_t* operands_list_length,
    int num_operands,
    unsigned char* success, size_t* new_value_length) {
  *new_value_length = 4;
  *success = 1;
  char* result = malloc(4);
  memcpy(result, "fake", 4);
  return result;
}

int main(int argc, char** argv) {
  rocksdb_t* db;
  rocksdb_comparator_t* cmp;
  rocksdb_cache_t* cache;
  rocksdb_env_t* env;
  rocksdb_options_t* options;
  rocksdb_block_based_table_options_t* table_options;
  rocksdb_readoptions_t* roptions;
  rocksdb_writeoptions_t* woptions;
  char* err = null;
  int run = -1;

  snprintf(dbname, sizeof(dbname),
           "%s/rocksdb_c_test-%d",
           gettempdir(),
           ((int) geteuid()));

  startphase("create_objects");
  cmp = rocksdb_comparator_create(null, cmpdestroy, cmpcompare, cmpname);
  env = rocksdb_create_default_env();
  cache = rocksdb_cache_create_lru(100000);

  options = rocksdb_options_create();
  rocksdb_options_set_comparator(options, cmp);
  rocksdb_options_set_error_if_exists(options, 1);
  rocksdb_options_set_env(options, env);
  rocksdb_options_set_info_log(options, null);
  rocksdb_options_set_write_buffer_size(options, 100000);
  rocksdb_options_set_paranoid_checks(options, 1);
  rocksdb_options_set_max_open_files(options, 10);
  table_options = rocksdb_block_based_options_create();
  rocksdb_block_based_options_set_block_cache(table_options, cache);
  rocksdb_options_set_block_based_table_factory(options, table_options);

  rocksdb_options_set_compression(options, rocksdb_no_compression);
  rocksdb_options_set_compression_options(options, -14, -1, 0);
  int compression_levels[] = {rocksdb_no_compression, rocksdb_no_compression,
                              rocksdb_no_compression, rocksdb_no_compression};
  rocksdb_options_set_compression_per_level(options, compression_levels, 4);

  roptions = rocksdb_readoptions_create();
  rocksdb_readoptions_set_verify_checksums(roptions, 1);
  rocksdb_readoptions_set_fill_cache(roptions, 0);

  woptions = rocksdb_writeoptions_create();
  rocksdb_writeoptions_set_sync(woptions, 1);

  startphase("destroy");
  rocksdb_destroy_db(options, dbname, &err);
  free(&err);

  startphase("open_error");
  db = rocksdb_open(options, dbname, &err);
  checkcondition(err != null);
  free(&err);

  startphase("open");
  rocksdb_options_set_create_if_missing(options, 1);
  db = rocksdb_open(options, dbname, &err);
  checknoerror(err);
  checkget(db, roptions, "foo", null);

  startphase("put");
  rocksdb_put(db, woptions, "foo", 3, "hello", 5, &err);
  checknoerror(err);
  checkget(db, roptions, "foo", "hello");

  startphase("compactall");
  rocksdb_compact_range(db, null, 0, null, 0);
  checkget(db, roptions, "foo", "hello");

  startphase("compactrange");
  rocksdb_compact_range(db, "a", 1, "z", 1);
  checkget(db, roptions, "foo", "hello");

  startphase("writebatch");
  {
    rocksdb_writebatch_t* wb = rocksdb_writebatch_create();
    rocksdb_writebatch_put(wb, "foo", 3, "a", 1);
    rocksdb_writebatch_clear(wb);
    rocksdb_writebatch_put(wb, "bar", 3, "b", 1);
    rocksdb_writebatch_put(wb, "box", 3, "c", 1);
    rocksdb_writebatch_delete(wb, "bar", 3);
    rocksdb_write(db, woptions, wb, &err);
    checknoerror(err);
    checkget(db, roptions, "foo", "hello");
    checkget(db, roptions, "bar", null);
    checkget(db, roptions, "box", "c");
    int pos = 0;
    rocksdb_writebatch_iterate(wb, &pos, checkput, checkdel);
    checkcondition(pos == 3);
    rocksdb_writebatch_destroy(wb);
  }

  startphase("writebatch_rep");
  {
    rocksdb_writebatch_t* wb1 = rocksdb_writebatch_create();
    rocksdb_writebatch_put(wb1, "baz", 3, "d", 1);
    rocksdb_writebatch_put(wb1, "quux", 4, "e", 1);
    rocksdb_writebatch_delete(wb1, "quux", 4);
    size_t repsize1 = 0;
    const char* rep = rocksdb_writebatch_data(wb1, &repsize1);
    rocksdb_writebatch_t* wb2 = rocksdb_writebatch_create_from(rep, repsize1);
    checkcondition(rocksdb_writebatch_count(wb1) ==
                   rocksdb_writebatch_count(wb2));
    size_t repsize2 = 0;
    checkcondition(
        memcmp(rep, rocksdb_writebatch_data(wb2, &repsize2), repsize1) == 0);
    rocksdb_writebatch_destroy(wb1);
    rocksdb_writebatch_destroy(wb2);
  }

  startphase("iter");
  {
    rocksdb_iterator_t* iter = rocksdb_create_iterator(db, roptions);
    checkcondition(!rocksdb_iter_valid(iter));
    rocksdb_iter_seek_to_first(iter);
    checkcondition(rocksdb_iter_valid(iter));
    checkiter(iter, "box", "c");
    rocksdb_iter_next(iter);
    checkiter(iter, "foo", "hello");
    rocksdb_iter_prev(iter);
    checkiter(iter, "box", "c");
    rocksdb_iter_prev(iter);
    checkcondition(!rocksdb_iter_valid(iter));
    rocksdb_iter_seek_to_last(iter);
    checkiter(iter, "foo", "hello");
    rocksdb_iter_seek(iter, "b", 1);
    checkiter(iter, "box", "c");
    rocksdb_iter_get_error(iter, &err);
    checknoerror(err);
    rocksdb_iter_destroy(iter);
  }

  startphase("approximate_sizes");
  {
    int i;
    int n = 20000;
    char keybuf[100];
    char valbuf[100];
    uint64_t sizes[2];
    const char* start[2] = { "a", "k00000000000000010000" };
    size_t start_len[2] = { 1, 21 };
    const char* limit[2] = { "k00000000000000010000", "z" };
    size_t limit_len[2] = { 21, 1 };
    rocksdb_writeoptions_set_sync(woptions, 0);
    for (i = 0; i < n; i++) {
      snprintf(keybuf, sizeof(keybuf), "k%020d", i);
      snprintf(valbuf, sizeof(valbuf), "v%020d", i);
      rocksdb_put(db, woptions, keybuf, strlen(keybuf), valbuf, strlen(valbuf),
                  &err);
      checknoerror(err);
    }
    rocksdb_approximate_sizes(db, 2, start, start_len, limit, limit_len, sizes);
    checkcondition(sizes[0] > 0);
    checkcondition(sizes[1] > 0);
  }

  startphase("property");
  {
    char* prop = rocksdb_property_value(db, "nosuchprop");
    checkcondition(prop == null);
    prop = rocksdb_property_value(db, "rocksdb.stats");
    checkcondition(prop != null);
    free(&prop);
  }

  startphase("snapshot");
  {
    const rocksdb_snapshot_t* snap;
    snap = rocksdb_create_snapshot(db);
    rocksdb_delete(db, woptions, "foo", 3, &err);
    checknoerror(err);
    rocksdb_readoptions_set_snapshot(roptions, snap);
    checkget(db, roptions, "foo", "hello");
    rocksdb_readoptions_set_snapshot(roptions, null);
    checkget(db, roptions, "foo", null);
    rocksdb_release_snapshot(db, snap);
  }

  startphase("repair");
  {
    // if we do not compact here, then the lazy deletion of
    // files (https://reviews.facebook.net/d6123) would leave
    // around deleted files and the repair process will find
    // those files and put them back into the database.
    rocksdb_compact_range(db, null, 0, null, 0);
    rocksdb_close(db);
    rocksdb_options_set_create_if_missing(options, 0);
    rocksdb_options_set_error_if_exists(options, 0);
    rocksdb_repair_db(options, dbname, &err);
    checknoerror(err);
    db = rocksdb_open(options, dbname, &err);
    checknoerror(err);
    checkget(db, roptions, "foo", null);
    checkget(db, roptions, "bar", null);
    checkget(db, roptions, "box", "c");
    rocksdb_options_set_create_if_missing(options, 1);
    rocksdb_options_set_error_if_exists(options, 1);
  }

  startphase("filter");
  for (run = 0; run < 2; run++) {
    // first run uses custom filter, second run uses bloom filter
    checknoerror(err);
    rocksdb_filterpolicy_t* policy;
    if (run == 0) {
      policy = rocksdb_filterpolicy_create(
          null, filterdestroy, filtercreate, filterkeymatch, null, filtername);
    } else {
      policy = rocksdb_filterpolicy_create_bloom(10);
    }

    rocksdb_block_based_options_set_filter_policy(table_options, policy);

    // create new database
    rocksdb_close(db);
    rocksdb_destroy_db(options, dbname, &err);
    rocksdb_options_set_block_based_table_factory(options, table_options);
    db = rocksdb_open(options, dbname, &err);
    checknoerror(err);
    rocksdb_put(db, woptions, "foo", 3, "foovalue", 8, &err);
    checknoerror(err);
    rocksdb_put(db, woptions, "bar", 3, "barvalue", 8, &err);
    checknoerror(err);
    rocksdb_compact_range(db, null, 0, null, 0);

    fake_filter_result = 1;
    checkget(db, roptions, "foo", "foovalue");
    checkget(db, roptions, "bar", "barvalue");
    if (phase == 0) {
      // must not find value when custom filter returns false
      fake_filter_result = 0;
      checkget(db, roptions, "foo", null);
      checkget(db, roptions, "bar", null);
      fake_filter_result = 1;

      checkget(db, roptions, "foo", "foovalue");
      checkget(db, roptions, "bar", "barvalue");
    }
    // reset the policy
    rocksdb_block_based_options_set_filter_policy(table_options, null);
    rocksdb_options_set_block_based_table_factory(options, table_options);
  }

  startphase("compaction_filter");
  {
    rocksdb_options_t* options = rocksdb_options_create();
    rocksdb_options_set_create_if_missing(options, 1);
    rocksdb_compactionfilter_t* cfilter;
    cfilter = rocksdb_compactionfilter_create(null, cfilterdestroy,
                                              cfilterfilter, cfiltername);
    // create new database
    rocksdb_close(db);
    rocksdb_destroy_db(options, dbname, &err);
    rocksdb_options_set_compaction_filter(options, cfilter);
    db = checkcompaction(db, options, roptions, woptions);

    rocksdb_options_set_compaction_filter(options, null);
    rocksdb_compactionfilter_destroy(cfilter);
    rocksdb_options_destroy(options);
  }

  startphase("compaction_filter_factory");
  {
    rocksdb_options_t* options = rocksdb_options_create();
    rocksdb_options_set_create_if_missing(options, 1);
    rocksdb_compactionfilterfactory_t* factory;
    factory = rocksdb_compactionfilterfactory_create(
        null, cfilterfactorydestroy, cfiltercreate, cfilterfactoryname);
    // create new database
    rocksdb_close(db);
    rocksdb_destroy_db(options, dbname, &err);
    rocksdb_options_set_compaction_filter_factory(options, factory);
    db = checkcompaction(db, options, roptions, woptions);

    rocksdb_options_set_compaction_filter_factory(options, null);
    rocksdb_options_destroy(options);
  }

  startphase("compaction_filter_v2");
  {
    rocksdb_compactionfilterfactoryv2_t* factory;
    rocksdb_slicetransform_t* prefix_extractor;
    prefix_extractor = rocksdb_slicetransform_create(
        null, cfv2prefixextractordestroy, cfv2prefixextractortransform,
        cfv2prefixextractorindomain, cfv2prefixextractorinrange,
        cfv2prefixextractorname);
    factory = rocksdb_compactionfilterfactoryv2_create(
        prefix_extractor, prefix_extractor, compactionfilterfactoryv2destroy,
        compactionfilterfactoryv2create, compactionfilterfactoryv2name);
    // create new database
    rocksdb_close(db);
    rocksdb_destroy_db(options, dbname, &err);
    rocksdb_options_set_compaction_filter_factory_v2(options, factory);
    db = rocksdb_open(options, dbname, &err);
    checknoerror(err);
    // only foo2 is gc'd, foo3 is changed.
    rocksdb_put(db, woptions, "foo1", 4, "no gc", 5, &err);
    checknoerror(err);
    rocksdb_put(db, woptions, "foo2", 4, "gc", 2, &err);
    checknoerror(err);
    rocksdb_put(db, woptions, "foo3", 4, "change", 6, &err);
    checknoerror(err);
    // all bars are gc'd.
    rocksdb_put(db, woptions, "bar1", 4, "no gc", 5, &err);
    checknoerror(err);
    rocksdb_put(db, woptions, "bar2", 4, "gc all", 6, &err);
    checknoerror(err);
    rocksdb_put(db, woptions, "bar3", 4, "no gc", 5, &err);
    checknoerror(err);
    // compact the db to garbage collect.
    rocksdb_compact_range(db, null, 0, null, 0);

    // verify foo entries.
    checkget(db, roptions, "foo1", "no gc");
    checkget(db, roptions, "foo2", null);
    checkget(db, roptions, "foo3", "changed");
    // verify bar entries were all deleted.
    checkget(db, roptions, "bar1", null);
    checkget(db, roptions, "bar2", null);
    checkget(db, roptions, "bar3", null);
  }

  startphase("merge_operator");
  {
    rocksdb_mergeoperator_t* merge_operator;
    merge_operator = rocksdb_mergeoperator_create(
        null, mergeoperatordestroy, mergeoperatorfullmerge,
        mergeoperatorpartialmerge, null, mergeoperatorname);
    // create new database
    rocksdb_close(db);
    rocksdb_destroy_db(options, dbname, &err);
    rocksdb_options_set_merge_operator(options, merge_operator);
    db = rocksdb_open(options, dbname, &err);
    checknoerror(err);
    rocksdb_put(db, woptions, "foo", 3, "foovalue", 8, &err);
    checknoerror(err);
    checkget(db, roptions, "foo", "foovalue");
    rocksdb_merge(db, woptions, "foo", 3, "barvalue", 8, &err);
    checknoerror(err);
    checkget(db, roptions, "foo", "fake");

    // merge of a non-existing value
    rocksdb_merge(db, woptions, "bar", 3, "barvalue", 8, &err);
    checknoerror(err);
    checkget(db, roptions, "bar", "fake");

  }

  startphase("columnfamilies");
  {
    rocksdb_close(db);
    rocksdb_destroy_db(options, dbname, &err);
    checknoerror(err)

    rocksdb_options_t* db_options = rocksdb_options_create();
    rocksdb_options_set_create_if_missing(db_options, 1);
    db = rocksdb_open(db_options, dbname, &err);
    checknoerror(err)
    rocksdb_column_family_handle_t* cfh;
    cfh = rocksdb_create_column_family(db, db_options, "cf1", &err);
    rocksdb_column_family_handle_destroy(cfh);
    checknoerror(err);
    rocksdb_close(db);

    size_t cflen;
    char** column_fams = rocksdb_list_column_families(db_options, dbname, &cflen, &err);
    checknoerror(err);
    checkequal("default", column_fams[0], 7);
    checkequal("cf1", column_fams[1], 3);
    checkcondition(cflen == 2);
    rocksdb_list_column_families_destroy(column_fams, cflen);

    rocksdb_options_t* cf_options = rocksdb_options_create();

    const char* cf_names[2] = {"default", "cf1"};
    const rocksdb_options_t* cf_opts[2] = {cf_options, cf_options};
    rocksdb_column_family_handle_t* handles[2];
    db = rocksdb_open_column_families(db_options, dbname, 2, cf_names, cf_opts, handles, &err);
    checknoerror(err);

    rocksdb_put_cf(db, woptions, handles[1], "foo", 3, "hello", 5, &err);
    checknoerror(err);

    checkgetcf(db, roptions, handles[1], "foo", "hello");

    rocksdb_delete_cf(db, woptions, handles[1], "foo", 3, &err);
    checknoerror(err);

    checkgetcf(db, roptions, handles[1], "foo", null);

    rocksdb_writebatch_t* wb = rocksdb_writebatch_create();
    rocksdb_writebatch_put_cf(wb, handles[1], "baz", 3, "a", 1);
    rocksdb_writebatch_clear(wb);
    rocksdb_writebatch_put_cf(wb, handles[1], "bar", 3, "b", 1);
    rocksdb_writebatch_put_cf(wb, handles[1], "box", 3, "c", 1);
    rocksdb_writebatch_delete_cf(wb, handles[1], "bar", 3);
    rocksdb_write(db, woptions, wb, &err);
    checknoerror(err);
    checkgetcf(db, roptions, handles[1], "baz", null);
    checkgetcf(db, roptions, handles[1], "bar", null);
    checkgetcf(db, roptions, handles[1], "box", "c");
    rocksdb_writebatch_destroy(wb);

    rocksdb_iterator_t* iter = rocksdb_create_iterator_cf(db, roptions, handles[1]);
    checkcondition(!rocksdb_iter_valid(iter));
    rocksdb_iter_seek_to_first(iter);
    checkcondition(rocksdb_iter_valid(iter));

    int i;
    for (i = 0; rocksdb_iter_valid(iter) != 0; rocksdb_iter_next(iter)) {
      i++;
    }
    checkcondition(i == 1);
    rocksdb_iter_get_error(iter, &err);
    checknoerror(err);
    rocksdb_iter_destroy(iter);

    rocksdb_drop_column_family(db, handles[1], &err);
    checknoerror(err);
    for (i = 0; i < 2; i++) {
      rocksdb_column_family_handle_destroy(handles[i]);
    }
    rocksdb_close(db);
    rocksdb_destroy_db(options, dbname, &err);
    rocksdb_options_destroy(db_options);
    rocksdb_options_destroy(cf_options);
  }

  startphase("prefix");
  {
    // create new database
    rocksdb_options_set_allow_mmap_reads(options, 1);
    rocksdb_options_set_prefix_extractor(options, rocksdb_slicetransform_create_fixed_prefix(3));
    rocksdb_options_set_hash_skip_list_rep(options, 5000, 4, 4);
    rocksdb_options_set_plain_table_factory(options, 4, 10, 0.75, 16);

    db = rocksdb_open(options, dbname, &err);
    checknoerror(err);

    rocksdb_put(db, woptions, "foo1", 4, "foo", 3, &err);
    checknoerror(err);
    rocksdb_put(db, woptions, "foo2", 4, "foo", 3, &err);
    checknoerror(err);
    rocksdb_put(db, woptions, "foo3", 4, "foo", 3, &err);
    checknoerror(err);
    rocksdb_put(db, woptions, "bar1", 4, "bar", 3, &err);
    checknoerror(err);
    rocksdb_put(db, woptions, "bar2", 4, "bar", 3, &err);
    checknoerror(err);
    rocksdb_put(db, woptions, "bar3", 4, "bar", 3, &err);
    checknoerror(err);

    rocksdb_iterator_t* iter = rocksdb_create_iterator(db, roptions);
    checkcondition(!rocksdb_iter_valid(iter));

    rocksdb_iter_seek(iter, "bar", 3);
    rocksdb_iter_get_error(iter, &err);
    checknoerror(err);
    checkcondition(rocksdb_iter_valid(iter));

    checkiter(iter, "bar1", "bar");
    rocksdb_iter_next(iter);
    checkiter(iter, "bar2", "bar");
    rocksdb_iter_next(iter);
    checkiter(iter, "bar3", "bar");
    rocksdb_iter_get_error(iter, &err);
    checknoerror(err);
    rocksdb_iter_destroy(iter);
  }


  startphase("cleanup");
  rocksdb_close(db);
  rocksdb_options_destroy(options);
  rocksdb_block_based_options_destroy(table_options);
  rocksdb_readoptions_destroy(roptions);
  rocksdb_writeoptions_destroy(woptions);
  rocksdb_cache_destroy(cache);
  rocksdb_comparator_destroy(cmp);
  rocksdb_env_destroy(env);

  fprintf(stderr, "pass\n");
  return 0;
}
