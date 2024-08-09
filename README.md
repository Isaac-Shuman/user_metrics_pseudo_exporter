Command I use for starting the pseudo_exporter
./stable_top_parse_prog/write_metrics & ./node_exporter-1.8.2.linux-amd64/node_exporter --collector.disable-defaults --collector.textfile --collector.textfile.directory=/tmp/ --web.listen-address=:9201 --web.disable-exporter-metrics &

Possible optimizations:
    Making scraper pull-based by making it an actual exporter by incorporating Prometheus Cllient libraries
    Never closing top.
    Eradicating the extraneous metrics gatthered by node exporter

If the grafana_prom service keeps breaking I should add TimeOut to ensure podman-compose down has time to run.

Bash command for killing processes listenting on ports
kill $(lsof -i | cut -d ' ' -f 2 | tr '\n' ' ' | cut -d ' ' -f 9-)

SLURM and TOP gatherers operate exclusively to PS