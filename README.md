gcc -o write_metrics write_metrics.c && ./write_metrics & ./node_exporter-1.8.1.linux-amd64/node_exporter --collector.disable-defaults --collector.textfile --collector.textfile.directory=. --web.listen-address=:9200 &

unit file stuff:

[Unit]
Description=Pseudo-Exporter
After=network.target

[Service]
User=shumani
WHAT DO I DO HERE??
ExecStart=/home/clarkm/prometheus/node_exporter-1.8.1.linux-amd64/node_exporter

[Install]
WantedBy=multi-user.target


Location of unit files:/etc/systemd/system