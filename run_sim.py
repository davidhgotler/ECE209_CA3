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
parser.add_argument("-v","--verbose",action="store_true",help="verbose = print command outputs")
parser.add_argument("-d","--debug",action="store_true",help="debug = print extra debug info")

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
if os.path.exists("lru-config1"):
    for trace in traces:
        if args.verbose:
            print(f'running on {trace}...')
        with open("tmp","w") as tmp:
            subprocess.call(trace_command+[trace], stdout=tmp)
        with open("tmp","r") as fp:
            lines = fp.read().splitlines()
            for line in lines:
                if args.debug:
                    print(line)
                if "LLC TOTAL" in line:
                    if args.verbose:
                        print(line)
                    line = line.split()
                    accesses.append(int(line[3]))
                    misses.append(int(line[7].replace(',','')))
os.remove("tmp")
total_accesses = np.sum(accesses)
total_misses = np.sum(misses)

print(f"miss rate = {total_misses/total_accesses}")