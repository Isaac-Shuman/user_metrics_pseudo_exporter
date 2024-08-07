EXPORTER_PORT = 9100
objects = write_metrics.o
EXPOSITION_FILENAME = /tmp/added_by_pseudo_exporter.prom
EXPOSITION_DIR = $(shell dirrname $(EXPOSITION_FILENAME))
CFLAGS = -DEXPOSITION_FILENAME=\"$(EXPOSITION_FILENAME)\"

write_metrics : $(objects)
	gcc -o write_metrics $(objects)

write_metrics.o : write_metrics.c uthash.h
	gcc $(CFLAGS) -c write_metrics.c

clean :
	rm write_metrics $(objects)

run_scraper : write_metrics
#remove added_by_prom.prom to simplify debugging
	-rm $(EXPOSITION_FILENAME)
	./write_metrics &
	sleep 1
	if [ -e $(EXPOSITION_FILENAME) ]; then \
		echo "Exposition file created"; \
		exit 0; \
	else \
		echo "ERROR: write_metrics failed to create an exposition file $(EXPOSITION_FILENAME)"; \
		exit 1; \
	fi

run_exporter : 
	./node_exporter-1.8.2.linux-amd64/node_exporter \
	--collector.disable-defaults --collector.textfile \
	--collector.textfile.directory=$(EXPOSITION_DIR) --web.listen-address=:$(EXPORTER_PORT) --web.disable-exporter-metrics &


#add a test in run_scraper to detect if an added_by_prom.prom file has been made
#add a test to run_exporter to curl the exporter address