#!/bin/bash

set -e

num=10000000

if [ $# -eq 1 ];then
  data_dir=$1
elif [ $# -eq 2 ];then
  data_dir=$1
  stat_file=$2
fi

# on the production build servers, set data and stat
# files/directories not in /tmp or else the tempdir cleaning
# scripts will make you very unhappy.
data_dir=${data_dir:-$(mktemp -t -d rocksdb_xxxx)}
stat_file=${stat_file:-$(mktemp -t -u rocksdb_test_stats_xxxx)}

function cleanup {
  rm -rf $data_dir
  rm -f $stat_file.fillseq
  rm -f $stat_file.readrandom
  rm -f $stat_file.overwrite
  rm -f $stat_file.memtablefillreadrandom
}

trap cleanup exit

if [ -z $git_branch ]; then
  git_br=`git rev-parse --abbrev-ref head`
else
  git_br=$(basename $git_branch)
fi

if [ $git_br == "master" ]; then
  git_br=""
else
  git_br="."$git_br
fi

make release

# measure fillseq + fill up the db for overwrite benchmark
./db_bench \
    --benchmarks=fillseq \
    --db=$data_dir \
    --use_existing_db=0 \
    --bloom_bits=10 \
    --num=$num \
    --writes=$num \
    --cache_size=6442450944 \
    --cache_numshardbits=6 \
    --table_cache_numshardbits=4 \
    --open_files=55000 \
    --statistics=1 \
    --histogram=1 \
    --disable_data_sync=1 \
    --disable_wal=1 \
    --sync=0  > ${stat_file}.fillseq

# measure overwrite performance
./db_bench \
    --benchmarks=overwrite \
    --db=$data_dir \
    --use_existing_db=1 \
    --bloom_bits=10 \
    --num=$num \
    --writes=$((num / 10)) \
    --cache_size=6442450944 \
    --cache_numshardbits=6  \
    --table_cache_numshardbits=4 \
    --open_files=55000 \
    --statistics=1 \
    --histogram=1 \
    --disable_data_sync=1 \
    --disable_wal=1 \
    --sync=0 \
    --threads=8 > ${stat_file}.overwrite

# fill up the db for readrandom benchmark (1gb total size)
./db_bench \
    --benchmarks=fillseq \
    --db=$data_dir \
    --use_existing_db=0 \
    --bloom_bits=10 \
    --num=$num \
    --writes=$num \
    --cache_size=6442450944 \
    --cache_numshardbits=6 \
    --table_cache_numshardbits=4 \
    --open_files=55000 \
    --statistics=1 \
    --histogram=1 \
    --disable_data_sync=1 \
    --disable_wal=1 \
    --sync=0 \
    --threads=1 > /dev/null

# measure readrandom with 6gb block cache
./db_bench \
    --benchmarks=readrandom \
    --db=$data_dir \
    --use_existing_db=1 \
    --bloom_bits=10 \
    --num=$num \
    --reads=$((num / 5)) \
    --cache_size=6442450944 \
    --cache_numshardbits=6 \
    --table_cache_numshardbits=4 \
    --open_files=55000 \
    --statistics=1 \
    --histogram=1 \
    --disable_data_sync=1 \
    --disable_wal=1 \
    --sync=0 \
    --threads=16 > ${stat_file}.readrandom

# measure readrandom with 6gb block cache and tailing iterator
./db_bench \
    --benchmarks=readrandom \
    --db=$data_dir \
    --use_existing_db=1 \
    --bloom_bits=10 \
    --num=$num \
    --reads=$((num / 5)) \
    --cache_size=6442450944 \
    --cache_numshardbits=6 \
    --table_cache_numshardbits=4 \
    --open_files=55000 \
    --use_tailing_iterator=1 \
    --statistics=1 \
    --histogram=1 \
    --disable_data_sync=1 \
    --disable_wal=1 \
    --sync=0 \
    --threads=16 > ${stat_file}.readrandomtailing

# measure readrandom with 100mb block cache
./db_bench \
    --benchmarks=readrandom \
    --db=$data_dir \
    --use_existing_db=1 \
    --bloom_bits=10 \
    --num=$num \
    --reads=$((num / 5)) \
    --cache_size=104857600 \
    --cache_numshardbits=6 \
    --table_cache_numshardbits=4 \
    --open_files=55000 \
    --statistics=1 \
    --histogram=1 \
    --disable_data_sync=1 \
    --disable_wal=1 \
    --sync=0 \
    --threads=16 > ${stat_file}.readrandomsmallblockcache

# measure readrandom with 8k data in memtable
./db_bench \
    --benchmarks=overwrite,readrandom \
    --db=$data_dir \
    --use_existing_db=1 \
    --bloom_bits=10 \
    --num=$num \
    --reads=$((num / 5)) \
    --writes=512 \
    --cache_size=6442450944 \
    --cache_numshardbits=6 \
    --table_cache_numshardbits=4 \
    --write_buffer_size=1000000000 \
    --open_files=55000 \
    --statistics=1 \
    --histogram=1 \
    --disable_data_sync=1 \
    --disable_wal=1 \
    --sync=0 \
    --threads=16 > ${stat_file}.readrandom_mem_sst


# fill up the db for readrandom benchmark with filluniquerandom (1gb total size)
./db_bench \
    --benchmarks=filluniquerandom \
    --db=$data_dir \
    --use_existing_db=0 \
    --bloom_bits=10 \
    --num=$((num / 4)) \
    --writes=$((num / 4)) \
    --cache_size=6442450944 \
    --cache_numshardbits=6 \
    --table_cache_numshardbits=4 \
    --open_files=55000 \
    --statistics=1 \
    --histogram=1 \
    --disable_data_sync=1 \
    --disable_wal=1 \
    --sync=0 \
    --threads=1 > /dev/null

# dummy test just to compact the data
./db_bench \
    --benchmarks=readrandom \
    --db=$data_dir \
    --use_existing_db=1 \
    --bloom_bits=10 \
    --num=$((num / 1000)) \
    --reads=$((num / 1000)) \
    --cache_size=6442450944 \
    --cache_numshardbits=6 \
    --table_cache_numshardbits=4 \
    --open_files=55000 \
    --statistics=1 \
    --histogram=1 \
    --disable_data_sync=1 \
    --disable_wal=1 \
    --sync=0 \
    --threads=16 > /dev/null

# measure readrandom after load with filluniquerandom with 6gb block cache
./db_bench \
    --benchmarks=readrandom \
    --db=$data_dir \
    --use_existing_db=1 \
    --bloom_bits=10 \
    --num=$((num / 4)) \
    --reads=$((num / 4)) \
    --cache_size=6442450944 \
    --cache_numshardbits=6 \
    --table_cache_numshardbits=4 \
    --open_files=55000 \
    --disable_auto_compactions=1 \
    --statistics=1 \
    --histogram=1 \
    --disable_data_sync=1 \
    --disable_wal=1 \
    --sync=0 \
    --threads=16 > ${stat_file}.readrandom_filluniquerandom

# measure readwhilewriting after load with filluniquerandom with 6gb block cache
./db_bench \
    --benchmarks=readwhilewriting \
    --db=$data_dir \
    --use_existing_db=1 \
    --bloom_bits=10 \
    --num=$((num / 4)) \
    --reads=$((num / 4)) \
    --writes_per_second=1000 \
    --write_buffer_size=100000000 \
    --cache_size=6442450944 \
    --cache_numshardbits=6 \
    --table_cache_numshardbits=4 \
    --open_files=55000 \
    --statistics=1 \
    --histogram=1 \
    --disable_data_sync=1 \
    --disable_wal=1 \
    --sync=0 \
    --threads=16 > ${stat_file}.readwhilewriting

# measure memtable performance -- none of the data gets flushed to disk
./db_bench \
    --benchmarks=fillrandom,readrandom, \
    --db=$data_dir \
    --use_existing_db=0 \
    --num=$((num / 10)) \
    --reads=$num \
    --cache_size=6442450944 \
    --cache_numshardbits=6 \
    --table_cache_numshardbits=4 \
    --write_buffer_size=1000000000 \
    --open_files=55000 \
    --statistics=1 \
    --histogram=1 \
    --disable_data_sync=1 \
    --disable_wal=1 \
    --sync=0 \
    --value_size=10 \
    --threads=16 > ${stat_file}.memtablefillreadrandom

common_in_mem_args="--db=/dev/shm/rocksdb \
    --num_levels=6 \
    --key_size=20 \
    --prefix_size=12 \
    --keys_per_prefix=10 \
    --value_size=100 \
    --compression_type=none \
    --compression_ratio=1 \
    --hard_rate_limit=2 \
    --write_buffer_size=134217728 \
    --max_write_buffer_number=4 \
    --level0_file_num_compaction_trigger=8 \
    --level0_slowdown_writes_trigger=16 \
    --level0_stop_writes_trigger=24 \
    --target_file_size_base=134217728 \
    --max_bytes_for_level_base=1073741824 \
    --disable_wal=0 \
    --wal_dir=/dev/shm/rocksdb \
    --sync=0 \
    --disable_data_sync=1 \
    --verify_checksum=1 \
    --delete_obsolete_files_period_micros=314572800 \
    --max_grandparent_overlap_factor=10 \
    --use_plain_table=1 \
    --open_files=-1 \
    --mmap_read=1 \
    --mmap_write=0 \
    --memtablerep=prefix_hash \
    --bloom_bits=10 \
    --bloom_locality=1 \
    --perf_level=0"

# prepare a in-memory db with 50m keys, total db size is ~6g
./db_bench \
    $common_in_mem_args \
    --statistics=0 \
    --max_background_compactions=16 \
    --max_background_flushes=16 \
    --benchmarks=filluniquerandom \
    --use_existing_db=0 \
    --num=52428800 \
    --threads=1 > /dev/null

# readwhilewriting
./db_bench \
    $common_in_mem_args \
    --statistics=1 \
    --max_background_compactions=4 \
    --max_background_flushes=0 \
    --benchmarks=readwhilewriting\
    --use_existing_db=1 \
    --duration=600 \
    --threads=32 \
    --writes_per_second=81920 > ${stat_file}.readwhilewriting_in_ram

# seekrandomwhilewriting
./db_bench \
    $common_in_mem_args \
    --statistics=1 \
    --max_background_compactions=4 \
    --max_background_flushes=0 \
    --benchmarks=seekrandomwhilewriting \
    --use_existing_db=1 \
    --use_tailing_iterator=1 \
    --duration=600 \
    --threads=32 \
    --writes_per_second=81920 > ${stat_file}.seekwhilewriting_in_ram


# send data to ods
function send_to_ods {
  key="$1"
  value="$2"

  if [ -z $jenkins_home ]; then
    # running on devbox, just print out the values
    echo $1 $2
    return
  fi

  if [ -z "$value" ];then
    echo >&2 "error: key $key doesn't have a value."
    return
  fi
  curl -s "https://www.intern.facebook.com/intern/agent/ods_set.php?entity=rocksdb_build$git_br&key=$key&value=$value" \
    --connect-timeout 60
}

function send_benchmark_to_ods {
  bench="$1"
  bench_key="$2"
  file="$3"

  qps=$(grep $bench $file | awk '{print $5}')
  p50_micros=$(grep $bench $file -a 6 | grep "percentiles" | awk '{print $3}' )
  p75_micros=$(grep $bench $file -a 6 | grep "percentiles" | awk '{print $5}' )
  p99_micros=$(grep $bench $file -a 6 | grep "percentiles" | awk '{print $7}' )

  send_to_ods rocksdb.build.$bench_key.qps $qps
  send_to_ods rocksdb.build.$bench_key.p50_micros $p50_micros
  send_to_ods rocksdb.build.$bench_key.p75_micros $p75_micros
  send_to_ods rocksdb.build.$bench_key.p99_micros $p99_micros
}

send_benchmark_to_ods overwrite overwrite $stat_file.overwrite
send_benchmark_to_ods fillseq fillseq $stat_file.fillseq
send_benchmark_to_ods readrandom readrandom $stat_file.readrandom
send_benchmark_to_ods readrandom readrandom_tailing $stat_file.readrandomtailing
send_benchmark_to_ods readrandom readrandom_smallblockcache $stat_file.readrandomsmallblockcache
send_benchmark_to_ods readrandom readrandom_memtable_sst $stat_file.readrandom_mem_sst
send_benchmark_to_ods readrandom readrandom_fillunique_random $stat_file.readrandom_filluniquerandom
send_benchmark_to_ods fillrandom memtablefillrandom $stat_file.memtablefillreadrandom
send_benchmark_to_ods readrandom memtablereadrandom $stat_file.memtablefillreadrandom
send_benchmark_to_ods readwhilewriting readwhilewriting $stat_file.readwhilewriting
send_benchmark_to_ods readwhilewriting readwhilewriting_in_ram ${stat_file}.readwhilewriting_in_ram
send_benchmark_to_ods seekrandomwhilewriting seekwhilewriting_in_ram ${stat_file}.seekwhilewriting_in_ram
