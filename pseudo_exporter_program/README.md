# Description

This program
1. Runs ps and sacct with popen()  
2. It parses them for data
   - ps is parsed for the %CPU and %MEM of every process. This data is aggregated by process group by reading line by line and using a hash table that stores every process group with their pgid as their key. The user and command stored with every process group is that of the process group leader (process where pid == pgid).
   - sacct is parsed for the NCPUS of every job. This data is aggregated by user by reading line by line and using a hash table that stores every user with their username as their key.
3. It prints the metrics in Prometheus text-based exposition format in /tmp/added_by_pseudo_exporter.prom (https://prometheus.io/docs/instrumenting/exposition_formats/)

Truncated example output in /tmp/added_by_pseudo_exporter.prom:

```
# HELP cpu_usage how much cpu a user is using
# TYPE cpu_usage gauge
cpu_usage{user="root",pgid="1",command="systemd         "} 0.00
cpu_usage{user="root",pgid="0",command="kthreadd        "} 0.00

# HELP ram_usage how much ram a user is using
# TYPE ram_usage gauge
ram_usage{user="root",pgid="1",command="systemd         "} 0.10
ram_usage{user="root",pgid="0",command="kthreadd        "} 0.00

# HELP ncpus how many cpus the user is using on the gpu nodes
# TYPE ncpus gauge
ncpus{user="denks"} 48.00
ncpus{user="shumani"} 20.00
```

This output is then able to be parsed by the "Textfile Collector" of the official Prometheus Node Exporter (https://github.com/prometheus/node_exporter)

# Usage

To build the program such that it gathers from ps and sacct and has debug mode enabled :
``` make EXTRA_FLAGS='-DGATHER_SLURM -DGATHER_PS -DDEBUG' ```

Command I use for starting the Node Exporter:
``` ./node_exporter-1.8.2.linux-amd64/node_exporter --collector.disable-defaults --collector.textfile --collector.textfile.directory=/tmp/ --web.listen-address=:9201 --web.disable-exporter-metrics & ```
The metrics gathered are visible at this endpoint. Note that there will be additional metrics from Node Exporter. Thus, it is important to configure Prometheus to only gather the metrics we care about.

# Potential Improvements
- Making scraper pull-based by making it an actual exporter by incorporating Prometheus Cllient libraries, thus making it an actual exporter that doesn't need to rely on Node Exporter
- Never closing top and letting it run continuously.
- Adding appropriate error handling that can check if the output of ps or sacct has changed.

# Notes/Q&A

Hash table documenttion: https://troydhanson.github.io/uthash/

The reading from top functionality is likely broken and I have not had time to test it after I did some refactoring.

I did not use valgrind or linters provided by VSCode

Q: Why did I write this text parser in C?
A: I originally envisioned it as a /proc pseudo-filesystem miner that could imitate commandline tools such as top and ps and that the text parsing was a stopgap measure. Since it's intended to run constantly, it's also important that it be high-performance. Golang would have been a better choice but I am unfamiliar with the language.

Output of ps -eo pid:7,pgid:7,user:16,comm:16,pcpu:20,pmem:5 --no-header
```
      1       1 root             systemd                           0.0   0.0
      2       0 root             kthreadd                          0.0   0.0
      3       0 root             rcu_gp                            0.0   0.0
      4       0 root             rcu_par_gp                        0.0   0.0
      5       0 root             slub_flushwq                      0.0   0.0
      7       0 root             kworker/0:0H-eve                  0.0   0.0
     11       0 root             mm_percpu_wq                      0.0   0.0
     12       0 root             rcu_tasks_rude_                   0.0   0.0
     13       0 root             rcu_tasks_trace                   0.0   0.0
     14       0 root             ksoftirqd/0                       0.0   0.0
```

Output of sacct -r gpus  --allusers --format=User,ncpus -X -s R
```
     User      NCPUS 
--------- ---------- 
    denks         48 
  shumani         10 

```

TODO:
explain the sscanf line and the text parsing in main better
