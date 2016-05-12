# Timeout estimates after 1M of data
COL_EST_RTO = 63
COL_EST_TLP = 64
COL_EST_TLP_DEL = 65
COL_EST_QUEUE_FREE_RTO = 66
COL_EST_QUEUE_FREE_TLP = 67
COL_EST_QUEUE_FREE_TLP_DEL = 68

flows-with-timeout-estimates-%.csv: flows-%.csv
	awk -F, '{if ($$$(COL_EST_RTO) > 0) print}' $< > $@

random-flows-with-timeout-estimates-%.csv: flows-with-timeout-estimates-%.csv
	$(SHUF_CMD) -n $(NUM_RANDOM_SAMPLES) $< > $@

sorted-est-rto-%.csv: random-flows-with-timeout-estimates-%.csv
	cut -d, -f $(COL_EST_RTO) $< | \
		sort -te -k2,2n -k1,1n > $@

sorted-est-tlp-%.csv: random-flows-with-timeout-estimates-%.csv
	cut -d, -f $(COL_EST_TLP) $< | \
		sort -te -k2,2n -k1,1n > $@

sorted-est-queue-free-rto-%.csv: random-flows-with-timeout-estimates-%.csv
	cut -d, -f $(COL_EST_QUEUE_FREE_RTO) $< | \
		sort -te -k2,2n -k1,1n > $@

sorted-est-queue-free-tlp-%.csv: random-flows-with-timeout-estimates-%.csv
	cut -d, -f $(COL_EST_QUEUE_FREE_TLP) $< | \
		sort -te -k2,2n -k1,1n > $@

quantiles-est-%.csv: $(MK_PATH)quantiles.sh sorted-est-%.csv
	$^ > $@

quantiles-timeout-estimates-merged-%.csv: \
	quantiles-est-queue-free-tlp-%.csv \
	quantiles-est-tlp-%.csv \
	quantiles-est-queue-free-rto-%.csv \
	quantiles-est-rto-%.csv
	echo "Queue-free TLP,,TLP,,Queue-free RTO,,RTO" > $@
	paste -d, $^ >> $@

$(FIG_DIR)/timeout-estimates-cdf-%.pdf: \
	quantiles-timeout-estimates-merged-%.csv $(MK_PATH)cdf_column_pairs.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:1000]" \
		-e "scale='0.001'" \
		-e "x_label='Estimated timout value (in milliseconds)'" \
		-e "num_lines=4" \
		-e "input='$<'" $(word 2, $^)

plots: $(FIG_DIR)/timeout-estimates-cdf-global.pdf
