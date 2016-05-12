flows-with-timeouts-%.csv: flows-%.csv
	awk -F, '{if ($$$(COL_NO_QUEUE_TIMEOUT) > 0) print}' $< > $@

random-flows-with-timeouts-%.csv: flows-with-timeouts-%.csv
	$(SHUF_CMD) -n $(NUM_RANDOM_SAMPLES) $< > $@

sorted-no-queue-timeouts-%.csv: random-flows-with-timeouts-%.csv
	cut -d, -f $(COL_NO_QUEUE_TIMEOUT) $< | \
		sort -te -k2,2n -k1,1n > $@ 

sorted-inflated-timeouts-%.csv: random-flows-with-timeouts-%.csv
	cut -d, -f $(COL_NO_QUEUE_TIMEOUT),$(COL_TIMEOUT_DELAY) $< | \
		awk -F, '{print $$1 + $$2}' | \
		sort -te -k2,2n -k1,1n > $@

quantiles-no-queue-timeouts-%.csv: $(MK_PATH)quantiles.sh sorted-no-queue-timeouts-%.csv
	$^ > $@

quantiles-inflated-timeouts-%.csv: $(MK_PATH)quantiles.sh sorted-inflated-timeouts-%.csv
	$^ > $@

quantiles-timeouts-merged-%.csv: quantiles-no-queue-timeouts-%.csv quantiles-inflated-timeouts-%.csv
	echo "Deflated,,Observed," > $@
	paste -d, $^ >> $@

$(FIG_DIR)/timeouts-cdf-%.pdf: quantiles-timeouts-merged-%.csv $(MK_PATH)cdf_columns.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:2000]" \
		-e "scale='0.001'" \
		-e "x_label='Timeout value (in milliseconds)'" \
		-e "num_lines=2" \
		-e "input='$<'" $(word 2, $^)

plots: $(FIG_DIR)/timeouts-cdf-global.pdf

sorted-relative-inflated-timeouts-%.csv: random-flows-with-timeouts-%.csv
	awk -F, '{print (($$$(COL_TIMEOUT_DELAY) / $$$(COL_NO_QUEUE_TIMEOUT)) + 1)}' $< | \
		sort -te -k2,2n -k1,1n > $@

quantiles-relative-inflated-timeouts-%.csv: $(MK_PATH)quantiles.sh sorted-relative-inflated-timeouts-%.csv
	$^ > $@

$(FIG_DIR)/relative-inflated-timeouts-cdf-%.pdf: quantiles-relative-inflated-timeouts-%.csv $(MK_PATH)cdf_single.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [1:20]" \
		-e "set logscale x" \
		-e "set xtics add ('20' 20)" \
		-e "scale='1'" \
		-e "x_label='Observed timeout / queue-free timeout'" \
		-e "input='$<'" $(word 2, $^)

plots: $(FIG_DIR)/relative-inflated-timeouts-cdf-global.pdf
