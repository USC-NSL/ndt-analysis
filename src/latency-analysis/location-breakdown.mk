# Makefile section related to generating plots with location breakdowns
# (e.g. by country, ASN, ...)

ifeq ($(MAKECMDGOALS),plots)
include top-asn-plots.gen.mk
include continent-plots.gen.mk
include country-plots.gen.mk
endif

include helper-plots.gen.mk
include merged-location-plots.gen.mk
include merged-location-plots-normalized.gen.mk
include merged-location-plots-relative.gen.mk

include timeout.mk
include timeout-estimates.mk
include buffering.mk

GEOIPLOOKUP := $(MK_PATH)../geoip-api-c/apps/geoiplookup
NUM_RANDOM_SAMPLES := 10000
NUM_BREAKDOWN_SAMPLES := 100
BREAKDOWN_COLUMNS = "Base,Loss Recovery,Late Trigger,Queuing,Other"
TRIGGER_BREAKDOWN_COLUMNS = \
	"Timeout,Late ACK armed,Late ACK triggered,Late trigger for final trigger"

merged-location-plots.gen.mk: merged-location-plots.mk
	sed 's/XX//g' $< |\
		sed 's/XSCALE/0.001/g' |\
		sed 's/XRANGE/2000/g' |\
		sed 's/XLABEL/Worst ACK delay (in milliseconds)/g' > $@

merged-location-plots-normalized.gen.mk: merged-location-plots.mk
	sed 's/XX/normalized-/g' $< |\
		sed 's/XSCALE/1/g' |\
		sed 's/XRANGE/1/g' |\
		sed 's/XLABEL/Normalized delay/g' |\
		awk '/plots.*base/ {next} \
			 /plots.*overall/ {next}1' > $@

merged-location-plots-relative.gen.mk: merged-location-plots.mk
	sed 's/XX/relative-/g' $< |\
		sed 's/XSCALE/1/g' |\
		sed 's/XRANGE/20/g' |\
		sed 's/XLABEL/Relative delay (wrt. to base delay)/g' |\
		awk '/plots.*base/ {next} \
			 /plots.*overall/ {next}1' > $@

helper-plots.gen.mk:
	rm -f $@
	for i in `seq 1 5`; do \
		echo "random-flow-delays-col-$$i-%.csv: random-flow-delays-all-cols-%.csv" >> $@; \
		echo "	cut -d, -f $$i \$$< > \$$@" >> $@; \
		echo "random-flow-delays-per-year-col-$$i-%.csv: random-flow-delays-per-year-all-cols-%.csv" >> $@; \
		echo "	cut -d, -f 1,$$((i+1)) \$$< > \$$@" >> $@; \
		echo "random-flow-trigger-delays-col-$$i-%.csv: random-flow-trigger-delays-all-cols-%.csv" >> $@; \
		echo "	cut -d, -f $$i \$$< > \$$@" >> $@; \
	done
	for i in `seq 2 5`; do \
		echo "normalized-tail-breakdown-col-$$i-%.csv: normalized-tail-breakdown-%.csv" >> $@; \
		echo "	cut -d, -f $$((i-1)) \$$< > \$$@" >> $@; \
		echo "relative-tail-breakdown-col-$$i-%.csv: relative-tail-breakdown-%.csv" >> $@; \
		echo "	cut -d, -f $$((i-1)) \$$< > \$$@" >> $@; \
		echo "normalized-tail-trigger-breakdown-col-$$i-%.csv: normalized-tail-trigger-breakdown-%.csv" >> $@; \
		echo "	cut -d, -f $$((i-1)) \$$< > \$$@" >> $@; \
	done

# Global plots
plots: $(FIG_DIR)/delay-cdf-global.pdf
plots: $(FIG_DIR)/normalized-delay-cdf-global.pdf
plots: $(FIG_DIR)/relative-delay-cdf-global.pdf

# Generate plotting rules for the top-100 ASNs
top-asn-plots.gen.mk: top-asns.csv location-plot-prefixes.txt
	rm -f $@
	for i in `head -100 $<`; do \
		for prefix in `cat $(word 2, $^)`; do \
			echo "plots: $(FIG_DIR)/$${prefix}-asn-$$i.pdf" >> $@; \
		done \
	done

continent-plots.gen.mk: $(MK_PATH)country-continent.txt location-plot-prefixes.txt
	rm -f $@
	for c in `tail -n +2 $< | awk -F, '{print $$2}'`; do \
		for prefix in `cat $(word 2, $^)`; do \
			echo "plots: $(FIG_DIR)/$${prefix}-continent-$$c.pdf" >> $@; \
		done \
	done

country-plots.gen.mk: $(MK_PATH)country-continent.txt location-plot-prefixes.txt
	rm -f $@
	for c in `tail -n +2 $< | awk -F, '{print $$1}'`; do \
		for prefix in `cat $(word 2, $^)`; do \
			echo "plots: $(FIG_DIR)/$${prefix}-country-$$c.pdf" >> $@; \
		done \
	done

fig-dir-prefixes.gen.mk: location-plot-prefixes.txt
	echo "fit" > $@
	for suffix in `echo "asn country continent"`; do \
		for prefix in `cat $<`; do \
			echo "$${prefix}-$${suffix}" >> $@; \
		done \
	done

# Extract client IP addresses
flows-clients.csv: flows-no-dp.csv
	cut -d, -f $(COL_FILENAME) $< |\
		sed $(EXTENDED_REGEXP_FLAG) -e \
		's/^.*_([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}).*$$/\1/' \
		-e 's/^.*_.*$$/0.0.0.0/' > $@

# Extract the year of the timestamp
flows-year.csv: flows-no-dp.csv
	cut -d, -f $(COL_FILENAME) $< | cut -c -4 > $@

flows-year-%.csv: flows-%.csv
	cut -d, -f $(COL_FILENAME) $< | cut -c -4 > $@

# Map client IP addresses to AS numbers (ASNs)
flows-asn.csv: flows-clients.csv $(MK_PATH)GeoIPASNum.dat $(GEOIPLOOKUP)
	$(GEOIPLOOKUP) -f $(word 2, $^) -file $< |\
		sed -e 's/^.*: \(AS[0-9]*\).*$$/\1/' \
		    -e 's/^GeoIP ASNum Edition: IP Address not found$$/AS0/' \
			-e 's/^.*can'\''t resolve hostname.*$$/AS0/' > $@

# Sort ASNs based on the number of samples per ASN
top-asns.csv: flows-asn.csv
	sort $< | uniq -c | sort -n -r | awk '{print $$2}' > $@

# Map client IP addresses to country identifiers (TLDs)
flows-countries.csv: flows-clients.csv $(MK_PATH)GeoIP.dat $(GEOIPLOOKUP)
	$(GEOIPLOOKUP) -f $(word 2, $^) -file $< |\
		sed -e 's/^.*: \([^,]*\),.*$$/\1/' \
		    -e 's/^GeoIP Country Edition: IP Address not found$$/--/' \
			-e 's/^.*can'\''t resolve hostname.*$$/0/' > $@

$(MK_PATH)country-continent.sed: $(MK_PATH)country-continent.txt
	tail -n +2 $< | sed -e 's#^\(.*\),\(.*\)$$#s/\1/\2/#' > $@

# Map client countries to continents
flows-continents.csv: flows-countries.csv $(MK_PATH)country-continent.sed
	sed -f $(word 2, $^) $< > $@

random-flows-%.csv: flows-%.csv
	$(SHUF_CMD) -n $(NUM_RANDOM_SAMPLES) $< > $@

random-flow-delays-all-cols-%.csv: random-flows-%.csv
	cut -d, -f $(COL_FIRST_BREAKDOWN)-$(COL_LAST_BREAKDOWN) $< > $@

random-flow-delays-per-year-all-cols-%.csv: random-flows-per-year-%.csv
	cut -d, -f 1,$$(($(COL_FIRST_BREAKDOWN)+1))-$$(($(COL_LAST_BREAKDOWN)+1)) $< > $@

random-flow-trigger-delays-all-cols-%.csv: random-flows-%.csv
	cut -d, -f $(COL_TRIGGER_DELAY),$(COL_FIRST_TRIGGER_BREAKDOWN)-$(COL_LAST_TRIGGER_BREAKDOWN) $< > $@

flows-global.csv: flows-no-dp.csv
	cp $< $@

flows-asn-%.csv: flows-asn.csv flows-no-dp.csv
	paste -d, $^ |\
		awk -F, '{if ($$1 == "$(*F)") print}' |\
		cut -d, -f 2- > $@

flows-continent-%.csv: flows-continents.csv flows-no-dp.csv
	paste -d, $^ |\
		awk -F, '{if ($$1 == "$(*F)") print}' |\
		cut -d, -f 2- > $@

flows-country-%.csv: flows-countries.csv flows-no-dp.csv
	paste -d, $^ |\
		awk -F, '{if ($$1 == "$(*F)") print}' |\
		cut -d, -f 2- > $@

random-flows-per-year-%.csv: flows-year-%.csv flows-%.csv
	rm -f $@
	for y in `seq 2010 2015`; do \
		paste -d, $^ |\
		awk -F, -v y=$$y '{if ($$1 == y) print}' |\
		$(SHUF_CMD) -n $(NUM_RANDOM_SAMPLES) >> $@; \
	done

# 1. Absolute breakdown (milliseconds attributed to each delay type)
# 2. Normalized breakdown (sums up to 1)
# 3. Relative breakdown wrt. to the base delay (i.e. base
#    delay is always 1)
tail-breakdown-%.csv: random-flow-delays-all-cols-%.csv
	sort -t, -r -n -k1,1 $< | cut -d, -f 2- > $@

normalized-tail-breakdown-%.csv: random-flow-delays-all-cols-%.csv $(MK_PATH)normalize_rows.awk
	sort -t, -r -n -k1,1 $< |\
		cut -d, -f 2- |\
		awk -F, -f $(word 2, $^) > $@

relative-tail-breakdown-%.csv: random-flow-delays-all-cols-%.csv $(MK_PATH)normalize_rows_by_first_column.awk
	sort -t, -r -n -k1,1 $< |\
		cut -d, -f 2- |\
		awk -F, -f $(word 2, $^) > $@

tail-breakdown-$(NUM_BREAKDOWN_SAMPLES)-%.csv: random-flow-delays-all-cols-%.csv
	echo $(BREAKDOWN_COLUMNS) > $@
	$(SHUF_CMD) -n $(NUM_BREAKDOWN_SAMPLES) $< |\
		sort -t, -r -n -k1,1 |\
		cut -d, -f 2- >> $@

normalized-tail-breakdown-$(NUM_BREAKDOWN_SAMPLES)-%.csv: random-flow-delays-all-cols-%.csv $(MK_PATH)normalize_rows.awk
	echo $(BREAKDOWN_COLUMNS) > $@
	$(SHUF_CMD) -n $(NUM_BREAKDOWN_SAMPLES) $< |\
		sort -t, -r -n -k1,1 |\
		cut -d, -f 2- |\
		awk -F, -f $(word 2, $^) >> $@

relative-tail-breakdown-$(NUM_BREAKDOWN_SAMPLES)-%.csv: random-flow-delays-all-cols-%.csv $(MK_PATH)normalize_rows_by_first_column.awk
	echo $(BREAKDOWN_COLUMNS) > $@
	$(SHUF_CMD) -n $(NUM_BREAKDOWN_SAMPLES) $< |\
		sort -t, -r -n -k1,1 |\
		cut -d, -f 2- |\
		awk -F, -f $(word 2, $^) >> $@

# Repeat for the trigger breakdowns
tail-trigger-breakdown-%.csv: random-flow-trigger-delays-all-cols-%.csv
	sort -t, -r -n -k1,1 $< | cut -d, -f 2- > $@

normalized-tail-trigger-breakdown-%.csv: random-flow-trigger-delays-all-cols-%.csv $(MK_PATH)normalize_rows.awk
	sort -t, -r -n -k1,1 $< |\
		cut -d, -f 2- |\
		awk -F, -f $(word 2, $^) > $@

tail-trigger-breakdown-$(NUM_BREAKDOWN_SAMPLES)-%.csv: random-flow-trigger-delays-all-cols-%.csv
	echo $(TRIGGER_BREAKDOWN_COLUMNS) > $@
	$(SHUF_CMD) -n $(NUM_BREAKDOWN_SAMPLES) $< |\
		sort -t, -r -n -k1,1 |\
		cut -d, -f 2- >> $@

normalized-tail-trigger-breakdown-$(NUM_BREAKDOWN_SAMPLES)-%.csv: random-flow-trigger-delays-all-cols-%.csv $(MK_PATH)normalize_rows.awk
	echo $(TRIGGER_BREAKDOWN_COLUMNS) > $@
	$(SHUF_CMD) -n $(NUM_BREAKDOWN_SAMPLES) $< |\
		sort -t, -r -n -k1,1 |\
		cut -d, -f 2- |\
		awk -F, -f $(word 2, $^) >> $@

# Sort values in a column (precursor to computing quantiles)
sorted-delays-col-%.csv: random-flow-delays-col-%.csv
	sort -te -k2,2n -k1,1n $< > $@

sorted-delays-normalized-col-%.csv: normalized-tail-breakdown-col-%.csv
	sort -te -k2,2n -k1,1n $< > $@

sorted-delays-relative-col-%.csv: relative-tail-breakdown-col-%.csv
	sort -te -k2,2n -k1,1n $< > $@

sorted-delays-per-year-col-%.csv: random-flow-delays-per-year-col-%.csv
	sort -t, -k1,1n -k2,2n $< > $@

sorted-trigger-delays-col-%.csv: random-flow-trigger-delays-col-%.csv
	sort -te -k2,2n -k1,1n $< > $@

sorted-trigger-delays-normalized-col-%.csv: normalized-tail-trigger-breakdown-col-%.csv
	sort -te -k2,2n -k1,1n $< > $@

# Fraction of the overall loss delay that is attributed to the loss trigger only
sorted-relative-loss-trigger-delay-%.csv: random-flow-delays-all-cols-%.csv
	cut -d, -f 2-3 $< |\
		awk -F, '{sum=$$1+$$2; if ($$1 > 0) { print ($$2 / sum); }}' |\
		sort -n > $@

# Compute quantiles based on sorted values in a column
quantiles-delays-col-%.csv: $(MK_PATH)quantiles.sh sorted-delays-col-%.csv
	$^ > $@

quantiles-delays-normalized-col-%.csv: $(MK_PATH)quantiles.sh sorted-delays-normalized-col-%.csv
	$^ > $@

quantiles-delays-relative-col-%.csv: $(MK_PATH)quantiles.sh sorted-delays-relative-col-%.csv
	$^ > $@

quantiles-delays-per-year-col-%.csv: $(MK_PATH)quantiles_per_key.sh sorted-delays-per-year-col-%.csv
	echo "2010,,2011,,2012,,2013,,2014,,2015," > $@
	$^ >> $@

quantiles-relative-loss-trigger-delay-%.csv: $(MK_PATH)quantiles.sh sorted-relative-loss-trigger-delay-%.csv
	$^ > $@

quantiles-trigger-delays-col-%.csv: $(MK_PATH)quantiles.sh sorted-trigger-delays-col-%.csv
	$^ > $@

quantiles-trigger-delays-normalized-col-%.csv: $(MK_PATH)quantiles.sh sorted-trigger-delays-normalized-col-%.csv
	$^ > $@

# Merge all columns back with each column containing quantiles
quantiles-delays-merged-%.csv: quantiles-delays-col-2-%.csv quantiles-delays-col-3-%.csv quantiles-delays-col-4-%.csv quantiles-delays-col-5-%.csv
	echo "Base,,Loss Recovery,,Late Trigger,,Queuing," > $@
	paste -d, $^ >> $@

quantiles-delays-normalized-merged-%.csv: quantiles-delays-normalized-col-2-%.csv quantiles-delays-normalized-col-3-%.csv quantiles-delays-normalized-col-4-%.csv quantiles-delays-normalized-col-5-%.csv
	echo "Base,,Loss Recovery,,Late Trigger,,Queuing," > $@
	paste -d, $^ >> $@

quantiles-delays-relative-merged-%.csv: quantiles-delays-relative-col-2-%.csv quantiles-delays-relative-col-3-%.csv quantiles-delays-relative-col-4-%.csv quantiles-delays-relative-col-5-%.csv
	echo "Base,,Loss Recovery,,Late Trigger,,Queuing," > $@
	paste -d, $^ >> $@

quantiles-trigger-delays-merged-%.csv: quantiles-trigger-delays-col-2-%.csv quantiles-trigger-delays-col-3-%.csv quantiles-trigger-delays-col-4-%.csv quantiles-trigger-delays-col-5-%.csv
	echo "Timeout,,Late ACK armed,,Late ACK triggered,,Late trigger for final trigger," > $@
	paste -d, $^ >> $@

quantiles-trigger-delays-normalized-merged-%.csv: quantiles-trigger-delays-normalized-col-2-%.csv quantiles-trigger-delays-normalized-col-3-%.csv quantiles-trigger-delays-normalized-col-4-%.csv quantiles-trigger-delays-normalized-col-5-%.csv
	echo "Timeout,,Late ACK armed,,Late ACK triggered,,Late trigger for final trigger," > $@
	paste -d, $^ >> $@

$(FIG_DIR)/overall-delay-cdf-%.pdf: quantiles-delays-col-1-%.csv $(MK_PATH)cdf_single.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1000]" \
		-e "scale='0.001'" \
		-e "x_label='Worst ACK delay (in milliseconds)'" \
		-e "input='$<'" $(word 2, $^)

$(FIG_DIR)/base-delay-cdf-%.pdf: quantiles-delays-col-2-%.csv $(MK_PATH)cdf_single.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1000]" \
		-e "scale='0.001'" \
		-e "x_label='Base delay (in milliseconds)'" \
		-e "input='$<'" $(word 2, $^)

$(FIG_DIR)/loss-delay-cdf-%.pdf: quantiles-delays-col-3-%.csv $(MK_PATH)cdf_single.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1000]" \
		-e "scale='0.001'" \
		-e "x_label='Loss recovery delay (in milliseconds)'" \
		-e "input='$<'" $(word 2, $^)

$(FIG_DIR)/loss-trigger-delay-cdf-%.pdf: quantiles-delays-col-4-%.csv $(MK_PATH)cdf_single.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1000]" \
		-e "scale='0.001'" \
		-e "x_label='Late trigger delay (in milliseconds)'" \
		-e "input='$<'" $(word 2, $^)

$(FIG_DIR)/queueing-delay-cdf-%.pdf: quantiles-delays-col-5-%.csv $(MK_PATH)cdf_single.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1000]" \
		-e "scale='0.001'" \
		-e "x_label='Queuing delay (in milliseconds)'" \
		-e "input='$<'" $(word 2, $^)

$(FIG_DIR)/relative-loss-trigger-delay-cdf-%.pdf: quantiles-relative-loss-trigger-delay-%.csv $(MK_PATH)cdf_single.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1]" \
		-e "scale='1'" \
		-e "x_label='Fraction of loss recovery delay attributed to late trigger delay'" \
		-e "input='$<'" $(word 2, $^)

# Combined graphs (includes the overall delay breakdown, one curve per type)
$(FIG_DIR)/normalized-delay-cdf-%.pdf: quantiles-delays-normalized-merged-%.csv $(MK_PATH)cdf_columns.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1]" \
		-e "scale='1'" \
		-e "x_label='Normalized fraction of overall delay'" \
		-e "num_lines=4" \
		-e "input='$<'" $(word 2, $^)

$(FIG_DIR)/relative-delay-cdf-%.pdf: quantiles-delays-relative-merged-%.csv $(MK_PATH)cdf_columns.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0.1:100]" \
		-e "set logscale x" \
		-e "scale='1'" \
		-e "x_label='Relative fraction of overall delay (wrt. to base delay)'" \
		-e "num_lines=4" \
		-e "input='$<'" $(word 2, $^)

$(FIG_DIR)/delay-cdf-%.pdf: quantiles-delays-merged-%.csv $(MK_PATH)cdf_columns.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:2000]" \
		-e "scale='0.001'" \
		-e "x_label='Delay (in milliseconds)'" \
		-e "num_lines=4" \
		-e "input='$<'" $(word 2, $^)

# Combined graphs (includes the overall trigger delay breakdown, one curve per
# type)
$(FIG_DIR)/normalized-trigger-breakdown-cdf-%.pdf: quantiles-trigger-delays-normalized-merged-%.csv $(MK_PATH)cdf_columns.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1]" \
		-e "scale='1'" \
		-e "x_label='Normalized fraction of overall trigger delay'" \
		-e "num_lines=4" \
		-e "input='$<'" $(word 2, $^)

$(FIG_DIR)/trigger-breakdown-cdf-%.pdf: quantiles-trigger-delays-merged-%.csv $(MK_PATH)cdf_columns.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:2000]" \
		-e "scale='0.001'" \
		-e "x_label='Delay (in milliseconds)'" \
		-e "num_lines=4" \
		-e "input='$<'" $(word 2, $^)

# As above, but now broken down per year
$(FIG_DIR)/overall-delay-per-year-cdf-%.pdf: quantiles-delays-per-year-col-1-%.csv $(MK_PATH)cdf_columns.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1000]" \
		-e "scale='0.001'" \
		-e "x_label='Worst ACK delay (in milliseconds)'" \
		-e "num_lines=6" \
		-e "input='$<'" $(word 2, $^)

$(FIG_DIR)/base-delay-per-year-cdf-%.pdf: quantiles-delays-per-year-col-2-%.csv $(MK_PATH)cdf_columns.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1000]" \
		-e "scale='0.001'" \
		-e "x_label='Base delay (in milliseconds)'" \
		-e "num_lines=6" \
		-e "input='$<'" $(word 2, $^)

$(FIG_DIR)/loss-delay-per-year-cdf-%.pdf: quantiles-delays-per-year-col-3-%.csv $(MK_PATH)cdf_columns.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1000]" \
		-e "scale='0.001'" \
		-e "x_label='Loss recovery delay (in milliseconds)'" \
		-e "num_lines=6" \
		-e "input='$<'" $(word 2, $^)

$(FIG_DIR)/loss-trigger-delay-per-year-cdf-%.pdf: quantiles-delays-per-year-col-4-%.csv $(MK_PATH)cdf_columns.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1000]" \
		-e "scale='0.001'" \
		-e "x_label='Late trigger delay (in milliseconds)'" \
		-e "num_lines=6" \
		-e "input='$<'" $(word 2, $^)

$(FIG_DIR)/queueing-delay-per-year-cdf-%.pdf: quantiles-delays-per-year-col-5-%.csv $(MK_PATH)cdf_columns.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1000]" \
		-e "scale='0.001'" \
		-e "x_label='Queuing delay (in milliseconds)'" \
		-e "num_lines=6" \
		-e "input='$<'" $(word 2, $^)

# Sample breakdown bar graphs for the overall delays
$(FIG_DIR)/tail-breakdown-%.pdf: tail-breakdown-$(NUM_BREAKDOWN_SAMPLES)-%.csv $(MK_PATH)bar.gp
	gnuplot -e "set output '$@'" \
		-e "set yrange [0:2000]" \
		-e "num_bars='$(NUM_BREAKDOWN_SAMPLES)'" \
		-e "num_cols='5'" \
		-e "scale=0.001" \
		-e "x_label='Sample'" \
		-e "y_label='Delay (in milliseconds)'" \
		-e "input='$<'" $(word 2, $^)

$(FIG_DIR)/normalized-tail-breakdown-%.pdf: normalized-tail-breakdown-$(NUM_BREAKDOWN_SAMPLES)-%.csv $(MK_PATH)bar.gp
	gnuplot -e "set output '$@'" \
		-e "set yrange [0:1]" \
		-e "num_bars='$(NUM_BREAKDOWN_SAMPLES)'" \
		-e "num_cols='5'" \
		-e "scale=1" \
		-e "x_label='Sample'" \
		-e "y_label='Fraction of overall delay'" \
		-e "input='$<'" $(word 2, $^)

$(FIG_DIR)/relative-tail-breakdown-%.pdf: relative-tail-breakdown-$(NUM_BREAKDOWN_SAMPLES)-%.csv $(MK_PATH)bar.gp
	gnuplot -e "set output '$@'" \
		-e "set yrange [0:20]" \
		-e "num_bars='$(NUM_BREAKDOWN_SAMPLES)'" \
		-e "num_cols='5'" \
		-e "scale=1" \
		-e "x_label='Sample'" \
		-e "y_label='Relative delay (wrt. to base delay)'" \
		-e "input='$<'" $(word 2, $^)

# Sample breakdown bar graphs for the trigger delays
$(FIG_DIR)/tail-trigger-breakdown-%.pdf: tail-trigger-breakdown-$(NUM_BREAKDOWN_SAMPLES)-%.csv $(MK_PATH)bar.gp
	gnuplot -e "set output '$@'" \
		-e "set yrange [0:2000]" \
		-e "num_bars='$(NUM_BREAKDOWN_SAMPLES)'" \
		-e "num_cols='5'" \
		-e "scale=0.001" \
		-e "x_label='Sample'" \
		-e "y_label='Delay (in milliseconds)'" \
		-e "input='$<'" $(word 2, $^)

$(FIG_DIR)/normalized-tail-trigger-breakdown-%.pdf: normalized-tail-trigger-breakdown-$(NUM_BREAKDOWN_SAMPLES)-%.csv $(MK_PATH)bar.gp
	gnuplot -e "set output '$@'" \
		-e "set yrange [0:1]" \
		-e "num_bars='$(NUM_BREAKDOWN_SAMPLES)'" \
		-e "num_cols='5'" \
		-e "scale=1" \
		-e "x_label='Sample'" \
		-e "y_label='Fraction of overall trigger delay'" \
		-e "input='$<'" $(word 2, $^)
