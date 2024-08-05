 ./stable_top_parse_prog/write_metrics & ./node_exporter-1.8.2.linux-amd64/node_exporter --collector.disable-defaults --collector.textfile --collector.textfile.directory=/tmp/added_by_exporter.prom --web.listen-address=:9201 --web.disable-exporter-metrics &

./stable_top_parse_prog/write_metrics & ./node_exporter-1.8.2.linux-amd64/node_exporter --collector.disable-defaults --collector.textfile --collector.textfile.directory=/tmp/ --web.listen-address=:9201 --web.disable-exporter-metrics &


unit file stuff:

If it keeps breaking I should add TimeOut to ensure podman-compose down has time to run.
WantedBy=multi-user.target


Location of unit files:/etc/systemd/system 

kill $(lsof -i | cut -d ' ' -f 2 | tr '\n' ' ' | cut -d ' ' -f 9-)