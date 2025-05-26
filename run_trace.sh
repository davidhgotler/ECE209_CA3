./drrip-config1 -warmup_instructions 1000000 -simulation_instructions 10000000 -traces trace/bzip2_10M.trace.gz > results/bzip2.txt
./drrip-config1 -warmup_instructions 1000000 -simulation_instructions 10000000 -traces trace/graph_analytics_10M.trace.gz > results/graph.txt
./drrip-config1 -warmup_instructions 1000000 -simulation_instructions 10000000 -traces trace/libquantum_10M.trace.gz > results/libquantum.txt
./drrip-config1 -warmup_instructions 1000000 -simulation_instructions 10000000 -traces trace/mcf_10M.trace.gz > results/mcf.txt