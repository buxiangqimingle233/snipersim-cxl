cd ../../
make clean && make RT=1 -j
# TPCC n=8 t=16
/home/wangzhao/snipersim/run-sniper --cache-only -n 16 --roi -c /home/wangzhao/experiments/DBx1000/cascade_lake.cfg -d /home/wangzhao/snipersim/test/unit-test -g perf_model/cxl/cxl_cache_roundtrip=0 -g perf_model/cxl/cxl_mem_roundtrip=0 -- /home/wangzhao/experiments/DBx1000/rundb_TPCC_OCC_LOG_NO -p1 -n8 -Tp0.5 -Gx500 -t16 -Ln
