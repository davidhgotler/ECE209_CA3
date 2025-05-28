import os
import subprocess
import sys
import numpy as np
from glob import glob
from argparse import ArgumentParser

parser = ArgumentParser("Script to (compile and) run simulations for all or some traces")
parser.add_argument("-c","--compile",action="store_true",help="compile = select if you would like to recompile at runtime")
parser.add_argument("-f","--filename",action="store",help="filename = model file path to recompile. Only applicable if compile is true.")
parser.add_argument("-t","--traces",action="store",nargs="+",default=["all"],help="traces = path(s) to trace files, or 'all' to run all in traces folder.")
parser.add_argument("-v","--verbose",action="store_true",help="verbose = print extra runtime info")
parser.add_argument("-d","--debug",action="store_true",help="debug = print extra debug info such as command outputs")

args = parser.parse_args()
compile_cmd = f"g++ -Wall --std=c++11 -o lru-config1".split() 
trace_command = "./lru-config1 -warmup_instructions 1000000 -simulation_instructions 10000000 -traces".split()

# recompile
if args.compile:
    if args.verbose:
        print("recompiling...")
    if os.path.exists(args.filename):
        success = subprocess.call(compile_cmd+[args.filename,"lib/config1.a"])
        # os.system(f"echo g++ -Wall --std=c++11 -o lru-config1 {args.filename} lib/config1.a")
    else:
        print("no file given to recompile. exiting")
        sys.exit()

traces = []
if args.traces==["all"]:
    traces = glob("trace/*")
else:
    for trace in args.traces:
        if os.path.exists(trace):
            traces.append(trace)

accesses = []
misses = []
processes = []

lognames = ["logs/run_"+trace.split("/")[-1]+".log" for trace in traces]
logs = []
for log in lognames:
    logs.append(open(log,"w"))

if os.path.exists("lru-config1"):
    # run all traces in parallel
    for trace,log in zip(traces,logs):
        if args.verbose:
            print(f'starting {trace}...')
            processes.append(subprocess.Popen(trace_command+[trace], stdout=log, text=True))
    
    # wait for all to complete
    running = True
    while running:
        completed = 0
        for proc in processes:
            if proc.poll()!=None:
                completed+=1
        if completed==len(processes):
            running=False
    
    if args.verbose:
        print("All traces complete")

    # close and reopen log files in read mode
    for log in logs:
        log.close()

    for logname in lognames:
        with open(logname,"r") as log:
            lines = log.read().splitlines()
        for line in lines:
            if args.debug:
                print(line)
            if "LLC TOTAL" in line:
                if args.verbose and not args.debug:
                    print(logname.removeprefix("logs/run_").removesuffix(".log") + ":\n\t" + line)
                line = line.split()
                accesses.append(int(line[3]))
                misses.append(int(line[7].replace(',','')))
                
total_accesses = np.sum(accesses)
total_misses = np.sum(misses)
if args.verbose:
    print(f"total accesses = {total_accesses}")
    print(f"total misses = {total_misses}")
print(f"miss rate = {total_misses/total_accesses}")