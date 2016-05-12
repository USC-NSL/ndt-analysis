MK_PATH := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	SHUF_CMD = gshuf
	ZCAT_CMD = gzcat
	EXTENDED_REGEXP_FLAG = -E
else
	SHUF_CMD = shuf
	ZCAT_CMD = zcat
	EXTENDED_REGEXP_FLAG = -r
endif

# Filter used for selecting the NDT files we want to process
# e.g. '*/06/01' selects all files from June 1st from all available years
SOURCE_DATE_FILTER := 2016/03/*

FIG_DIR := pdfs
PROCESS_NDT_FILE = "$(MK_PATH)process-ndt-file.sh"

# Specify column indexes based on the given OUTPUT_FORMAT
# Note: all these indexes are off by 1 since the output files have the archive
# name appended at the front
COL_FILENAME = 2
COL_DATA_PACKETS = 5
COL_FIRST_BREAKDOWN = 8
COL_LAST_BREAKDOWN = 13
COL_OVERALL_DELAY = 8
COL_BASE_DELAY = 9
COL_TRIGGER_DELAY = 11
COL_NO_QUEUE_TIMEOUT = 14
COL_FIRST_TRIGGER_BREAKDOWN = 15
COL_LAST_TRIGGER_BREAKDOWN = 18
COL_TIMEOUT_DELAY = 15
COL_LATE_TRIGGER_FOR_TRIGGER = 18
COL_CORRELATION = 19
COL_FIRST_LINEAR_FIT = 20
COL_LAST_LINEAR_FIT = 21
COL_BYTES_ACKED_BEFORE_WORST_PACKET = 24
COL_BYTES_NEEDED_BUFFERED = 25
COL_LAST_ANALYSIS = 67
COL_FIRST_DATAPOINT = 68

HC_THRESHOLD = 0.8

default: flows.csv

.PHONY: clean-results clean-unfiltered clean-all pack-results

.PRECIOUS: ndt-files $(CSV_FILES)
.SECONDARY:

# Check if all dependencies are installed. Force immediate evaluation through
# variable assignment
EXECUTABLES = reordercap gsutil $(ZCAT_CMD) gzip bc sed $(SHUF_CMD)
K := $(foreach exec,$(EXECUTABLES), \
	$(if $(shell which $(exec)),some string,$(error "No $(exec) in PATH. Please install $(exec)")))

plots:
	mkdir -p $(FIG_DIR)

clean-all: clean-results clean-unfiltered
	rm -f *.gen.mk
	rm -f ndt-files*

clean-results:
	rm -f $(FIG_DIR)/*
	rm -f *.csv

clean-unfiltered:
	rm -f *.csv.gz

pack-results: fig-dir-prefixes.gen.mk
	rm -rf packed
	for prefix in `cat $<`; do \
		mkdir -p packed/$$prefix; \
		cp $(FIG_DIR)/$${prefix}*.pdf packed/$$prefix/; \
	done
	for suffix in `echo "timeout-breakdown merged-all-continents merged-all-countries merged-all-asns global"`; do \
		mkdir -p packed/$$suffix; \
		cp $(FIG_DIR)/*$${suffix}.pdf packed/$${suffix}/; \
	done
#	find packed -name "*.pdf" | xargs -P12 -I'{}' convert -density 150 -quality 90 {} {}.png	
	tar -cvf pdfs.tar packed
	rm -rf packed

# Generate list of NDT files to process
# Right now we collect one sample PER YEAR
ndt-files:
	gsutil ls gs://m-lab/ndt/$(SOURCE_DATE_FILTER)/*.tgz > $@

ndt-files-no-path: ndt-files
	cat $< | sed -e 's#^.*/\([^/]*\)\.tgz$$#\1.csv.gz#' >> $@

deps.gen.mk: ndt-files-no-path
	echo "CSV_FILES = \\" > $@
	cat $< | awk '{printf "%s \\\n", $$0}' >> $@
	echo >> $@

-include deps.gen.mk
#-include fit-plots.gen.mk
-include location-breakdown.mk

HC_CSV_FILES := $(addprefix hc-, $(CSV_FILES))

fit-plots.gen.mk:
	rm -f $@
	for i in `seq 1 100`; do \
		echo "plots: $(FIG_DIR)/fit-dp-row-$$i.pdf" >> $@; \
	done

$(CSV_FILES):
	$(PROCESS_NDT_FILE) $(@:.gz=)
	gzip $(@:.gz=)

# Combine all the input files and only select lines with at least 100 packets
flows.csv: ndt-files-no-path $(HC_CSV_FILES)
	rm -f $@
	for f in `cat $<`; do \
		echo hc-$$f; \
		$(ZCAT_CMD) hc-$$f | \
		awk -F, '{if ($$$(COL_DATA_PACKETS) >= 100) print}' >> $@; \
	done

all-flows.csv: ndt-files-no-path $(CSV_FILES)
	rm -f $@
	for f in `cat $<`; do \
		echo $$f; \
		$(ZCAT_CMD) $$f | \
		awk -F, '{if ($$$(COL_DATA_PACKETS) >= 100) print}' >> $@; \
	done

%-no-dp.csv: %.csv
	cut -d, -f -$(COL_LAST_ANALYSIS) $< > $@

flows-tail-latency.csv: flows.csv
	cut -d, -f $(COL_FIRST_BREAKDOWN)-$(COL_LAST_BREAKDOWN) $< > $@

# Only aggregate and keep high-correlation cases
hc-%.csv.gz: %.csv.gz
	$(ZCAT_CMD) $^ | awk -F, \
		'{if ($$$(COL_CORRELATION) >= $(HC_THRESHOLD) && \
			  $$$(COL_CORRELATION) != "nan" && \
			  $$$(COL_CORRELATION) != "-nan" && \
			  $$$(COL_LAST_LINEAR_FIT) > 0) print}' |\
		gzip > $@

random-flows-10000.csv: flows.csv
	$(SHUF_CMD) -n 10000 $< > $@

# Helper to generate the datapoint plot for a given sample (data is
# organized in two-column format)
$(FIG_DIR)/dp-%.pdf: dp-two-column-%.csv $(MK_PATH)datapoints.gp
	gnuplot -e "set output '$@'" \
		-e "scale_x='0.0009765'" \
		-e "scale_y='0.001'" \
		-e "x_label='Unacked KBytes'" \
		-e "y_label='RTT (ms)'" \
		-e "input='$<'" \
		$(word 2, $^)

# As above, but also plots the best linear fit for the given datapoints
$(FIG_DIR)/fit-dp-%.pdf: dp-two-column-%.csv fit-dp-%.csv $(MK_PATH)datapoints_fit.gp
	$(eval c_0 := $(shell awk -F, '{print $$1}' $(word 2, $^)))
	$(eval c_1 := $(shell awk -F, '{print $$2}' $(word 2, $^)))
	gnuplot -e "set output '$@'" \
		-e "set yrange [0:]" \
		-e "scale_x='0.0009765'" \
		-e "scale_y='0.001'" \
		-e "x_label='Unacked KBytes'" \
		-e "y_label='RTT (ms)'" \
		-e "input='$<'" \
		-e "c_0=$(c_0)" \
		-e "c_1=$(c_1)" \
		$(word 3, $^)

# Convert a one-liner with consecutive (x,y) pairs to two-column format
# Each row is an (RTT, unacked bytes) pair
dp-two-column-%.csv: dp-%.csv $(MK_PATH)convert-to-two-column.awk
	awk -F, -f $(word 2, $^) $< > $@

# Extracts the datapoints from the nth row in the overall flow file
# (strips all other fields, e.g. metadata)
dp-row-%.csv: random-flows-10000.csv
	sed '$(*F)q;d' $< | cut -d, -f $(COL_FIRST_DATAPOINT)- > $@

# Extracts the linear fit parameters for datapoints from the nth row in
# the overall flow file
fit-dp-row-%.csv: random-flows-10000.csv
	sed '$(*F)q;d' $< |\
		cut -d, -f $(COL_FIRST_LINEAR_FIT)-$(COL_LAST_LINEAR_FIT) > $@

sample-correlation.csv: random-flows-10000.csv
	    cut -d, -f $(COL_CORRELATION) $< | sort -n > $@

sample-correlation-quantiles.csv: sample-correlation.csv $(MK_PATH)quantiles.sh
	    bash $(word 2, $^) $< > $@

$(FIG_DIR)/sample-correlation.pdf: sample-correlation-quantiles.csv $(MK_PATH)cdf_single.gp
	    gnuplot -e "set output '$@'" \
			-e "set xrange [-1:1]" \
			-e "scale='1'" \
			-e "x_label='Unacked Bytes/RTT Pearson correlation'" \
			-e "input='$<'" $(word 2, $^)

# Extracts the fields that store the delay breakdown for the tail (worst latency
# packet)
sample-tail-latency.csv: random-flows-10000.csv
	cut -d, -f $(COL_FIRST_BREAKDOWN)-$(COL_LAST_BREAKDOWN) $< > $@

sample-tail-latency-sorted-col-%.csv: sample-tail-latency.csv
	cut -d, -f $(*F) $< | sort -te -k2,2n -k1,1n > $@

sample-tail-latency-quantiles-col-%.csv: sample-tail-latency-sorted-col-%.csv $(MK_PATH)quantiles.sh
	bash $(word 2, $^) $< > $@

# Overall worst latency (ACK delay) distribution
$(FIG_DIR)/sample-tail-overall-delay.pdf: sample-tail-latency-quantiles-col-1.csv $(MK_PATH)cdf_single.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1000]" \
		-e "scale='0.001'" \
		-e "x_label='Worst ACK delay (in milliseconds)'" \
		-e "input='$<'" $(word 2, $^)

# Base delay distribution
$(FIG_DIR)/sample-tail-base-delay.pdf: sample-tail-latency-quantiles-col-2.csv $(MK_PATH)cdf_single.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1000]" \
		-e "scale='0.001'" \
		-e "x_label='Estimated base delay (in milliseconds)'" \
		-e "input='$<'" $(word 2, $^)

# Loss delay distribution
$(FIG_DIR)/sample-tail-loss-delay.pdf: sample-tail-latency-quantiles-col-3.csv $(MK_PATH)cdf_single.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1000]" \
		-e "scale='0.001'" \
		-e "x_label='Worst loss delay (in milliseconds)'" \
		-e "input='$<'" $(word 2, $^)

# Base delay distribution
$(FIG_DIR)/sample-tail-queueing-delay.pdf: sample-tail-latency-quantiles-col-4.csv $(MK_PATH)cdf_single.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1000]" \
		-e "scale='0.001'" \
		-e "x_label='Worst queueing delay (in milliseconds)'" \
		-e "input='$<'" $(word 2, $^)

sample-tail-breakdown.csv: sample-tail-latency.csv
	echo "Base,Loss,Loss Trigger,Queueing,Other" > $@
	head -100 $< | sort -t, -r -n -k1,1 | cut -d, -f 2- >> $@

# Per-packet delay breakdown
$(FIG_DIR)/sample-tail-breakdown.pdf: sample-tail-breakdown.csv $(MK_PATH)bar.gp
	gnuplot -e "set output '$@'" \
		-e "set yrange [0:2000]" \
		-e "num_bars='100'" \
		-e "num_cols='5'" \
		-e "scale=0.001" \
		-e "x_label='Sample'" \
		-e "y_label='Delay (in milliseconds)'" \
		-e "input='$<'" $(word 2, $^)

plots: $(FIG_DIR)/sample-tail-overall-delay.pdf
plots: $(FIG_DIR)/sample-tail-base-delay.pdf
plots: $(FIG_DIR)/sample-tail-loss-delay.pdf
plots: $(FIG_DIR)/sample-tail-queueing-delay.pdf
plots: $(FIG_DIR)/sample-tail-breakdown.pdf

# There are few cases with timeout delays, the ones we have we plot as
# breakdowns here
flows-timeouts.csv: flows-no-dp.csv
	echo "$(TRIGGER_BREAKDOWN_COLUMNS)" > $@
	awk -F, \
		'{if ($$$(COL_TIMEOUT_DELAY) > 0 || \
		      $$$(COL_LATE_TRIGGER_FOR_TRIGGER) > 0) print }' $< |\
		cut -d, -f $(COL_TRIGGER_DELAY),$(COL_FIRST_TRIGGER_BREAKDOWN)-$(COL_LAST_TRIGGER_BREAKDOWN) |\
		sort -n -r -k1,1 | cut -d, -f 2- >> $@

flows-timeouts-100.csv: flows-timeouts.csv
	head -101 $< > $@

$(FIG_DIR)/timeout-breakdown.pdf: flows-timeouts-100.csv $(MK_PATH)bar.gp
	gnuplot -e "set output '$@'" \
		-e "set yrange [0:2000]" \
		-e "num_bars='100'" \
		-e "num_cols='4'" \
		-e "scale=0.001" \
		-e "x_label='Sample'" \
		-e "y_label='Delay (in milliseconds)'" \
		-e "input='$<'" $(word 2, $^)

plots: $(FIG_DIR)/timeout-breakdown.pdf


flows-non-zero-col-%.csv: flows-no-dp.csv
	awk -F, '{if ($$$(*F) > 0 && $$$(COL_BASE_DELAY) > 0 && $$$(*F) < $$$(COL_OVERALL_DELAY)) print}' $< > $@

absolute-value-col-%.csv: flows-non-zero-col-%.csv
	cut -d, -f $(*F) $< > $@

normalized-value-col-%.csv: flows-non-zero-col-%.csv
	cut -d, -f $(COL_OVERALL_DELAY),$(*F) $< |\
		awk -F, '{ print ($$2 / $$1)}' > $@

relative-value-col-%.csv: flows-non-zero-col-%.csv
	cut -d, -f $(COL_BASE_DELAY),$(*F) $< |\
		awk -F, '{ print ($$2 / $$1)}' > $@

median-%.csv: %.csv $(MK_PATH)median.awk
	sort -n $< | awk -f $(word 2, $^) > $@

pc95-%.csv: %.csv
	sort -n $< | awk '{all[NR] = $$0} END{print all[int(NR*0.95 - 0.5)]}' > $@

metrics-col-%.csv: \
	median-absolute-value-col-%.csv median-normalized-value-col-%.csv median-relative-value-col-%.csv \
	pc95-absolute-value-col-%.csv pc95-normalized-value-col-%.csv pc95-relative-value-col-%.csv
	paste -d, $^ > $@

# TODO needs to use COL_* variables defined at top
metrics-for-triggers.csv: metrics-col-15.csv metrics-col-16.csv metrics-col-17.csv metrics-col-18.csv
	sed 's/^/Late ACK triggered,$(shell wc -l flows-non-zero-col-17.csv | cut -d" " -f 1),/' metrics-col-17.csv > $@
	sed 's/^/Late ACK armed timer,$(shell wc -l flows-non-zero-col-16.csv | cut -d" " -f 1),/' metrics-col-16.csv >> $@
	sed 's/^/Inflated timeout,$(shell wc -l flows-non-zero-col-15.csv | cut -d" " -f 1),/' metrics-col-15.csv >> $@
	sed 's/^/Late trigger for final trigger,$(shell wc -l flows-non-zero-col-18.csv | cut -d" " -f 1),/' metrics-col-18.csv >> $@

metrics-for-triggers-table-env.csv: metrics-for-triggers.csv
	sed 's/,/ \& /g' $< | sed 's/$$/\\\\/g' > $@

plots: metrics-for-triggers-table-env.csv
