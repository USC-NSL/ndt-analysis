sorted-bytes-needed-buffered-%.csv: random-flows-%.csv
	cut -d, -f $(COL_BYTES_NEEDED_BUFFERED) $< | \
		sort -te -k2,2n -k1,1n > $@

quantiles-bytes-needed-buffered-%.csv: $(MK_PATH)quantiles.sh sorted-bytes-needed-buffered-%.csv
	$^ > $@

$(FIG_DIR)/bytes-needed-buffered-cdf-%.pdf: quantiles-bytes-needed-buffered-%.csv $(MK_PATH)cdf_single.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:2000]" \
		-e "scale='0.000976'" \
		-e "x_label='Buffer needed (in kilobytes)'" \
		-e "input='$<'" $(word 2, $^)

sorted-acked-fraction-needed-buffered-%.csv: random-flows-%.csv
	cut -d, -f $(COL_BYTES_ACKED_BEFORE_WORST_PACKET),$(COL_BYTES_NEEDED_BUFFERED) $< | \
		awk -F, '{if ($$1 > 0) print $$2 / $$1}' | \
		sort -te -k2,2n -k1,1n > $@

quantiles-acked-fraction-needed-buffered-%.csv: $(MK_PATH)quantiles.sh sorted-acked-fraction-needed-buffered-%.csv
	$^ > $@

$(FIG_DIR)/acked-fraction-needed-buffered-cdf-%.pdf: quantiles-acked-fraction-needed-buffered-%.csv $(MK_PATH)cdf_single.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:2]" \
		-e "scale='1'" \
		-e "x_label='Fraction of acked bytes needed buffered'" \
		-e "input='$<'" $(word 2, $^)

plots: $(FIG_DIR)/bytes-needed-buffered-cdf-global.pdf
plots: $(FIG_DIR)/acked-fraction-needed-buffered-cdf-global.pdf

# Merged per-ASN/country/continent plots
# See merged-location-plots.mk for location variable definitions
quantiles-bytes-needed-buffered-merged-all-countries.csv: \
		quantiles-bytes-needed-buffered-country-$(COUNTRY1).csv \
		quantiles-bytes-needed-buffered-country-$(COUNTRY2).csv \
		quantiles-bytes-needed-buffered-country-$(COUNTRY3).csv \
		quantiles-bytes-needed-buffered-country-$(COUNTRY4).csv \
		quantiles-bytes-needed-buffered-country-$(COUNTRY5).csv \
		quantiles-bytes-needed-buffered-country-$(COUNTRY6).csv
	echo "$(COUNTRY1N),,$(COUNTRY2N),,$(COUNTRY3N),,$(COUNTRY4N),,$(COUNTRY5N),,$(COUNTRY6N)," > $@
	paste -d, $^ >> $@

quantiles-bytes-needed-buffered-merged-all-continents.csv: \
		quantiles-bytes-needed-buffered-continent-$(CONT1).csv \
		quantiles-bytes-needed-buffered-continent-$(CONT2).csv \
		quantiles-bytes-needed-buffered-continent-$(CONT3).csv \
		quantiles-bytes-needed-buffered-continent-$(CONT4).csv \
		quantiles-bytes-needed-buffered-continent-$(CONT5).csv \
		quantiles-bytes-needed-buffered-continent-$(CONT6).csv
	echo "$(CONT1N),,$(CONT2N),,$(CONT3N),,$(CONT4N),,$(CONT5N),,$(CONT6N)," > $@
	paste -d, $^ >> $@

quantiles-bytes-needed-buffered-merged-all-asns.csv: \
		quantiles-bytes-needed-buffered-asn-$(ASN1).csv \
		quantiles-bytes-needed-buffered-asn-$(ASN2).csv \
		quantiles-bytes-needed-buffered-asn-$(ASN3).csv \
		quantiles-bytes-needed-buffered-asn-$(ASN4).csv \
		quantiles-bytes-needed-buffered-asn-$(ASN5).csv \
		quantiles-bytes-needed-buffered-asn-$(ASN6).csv
	echo "$(ASN1),,$(ASN2),,$(ASN3),,$(ASN4),,$(ASN5),,$(ASN6)," > $@
	paste -d, $^ >> $@

$(FIG_DIR)/bytes-needed-buffered-cdf-merged-%.pdf: \
		quantiles-bytes-needed-buffered-merged-%.csv $(MK_PATH)cdf_columns.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:2000]" \
		-e "scale='0.000976'" \
		-e "x_label='Buffer needed (in kilobytes)'" \
		-e "num_lines=6" \
		-e "input='$<'" $(word 2, $^)

plots: $(FIG_DIR)/bytes-needed-buffered-cdf-merged-all-asns.pdf
plots: $(FIG_DIR)/bytes-needed-buffered-cdf-merged-all-countries.pdf
plots: $(FIG_DIR)/bytes-needed-buffered-cdf-merged-all-continents.pdf

quantiles-acked-fraction-needed-buffered-merged-all-countries.csv: \
		quantiles-acked-fraction-needed-buffered-country-$(COUNTRY1).csv \
		quantiles-acked-fraction-needed-buffered-country-$(COUNTRY2).csv \
		quantiles-acked-fraction-needed-buffered-country-$(COUNTRY3).csv \
		quantiles-acked-fraction-needed-buffered-country-$(COUNTRY4).csv \
		quantiles-acked-fraction-needed-buffered-country-$(COUNTRY5).csv \
		quantiles-acked-fraction-needed-buffered-country-$(COUNTRY6).csv
	echo "$(COUNTRY1N),,$(COUNTRY2N),,$(COUNTRY3N),,$(COUNTRY4N),,$(COUNTRY5N),,$(COUNTRY6N)," > $@
	paste -d, $^ >> $@

quantiles-acked-fraction-needed-buffered-merged-all-continents.csv: \
		quantiles-acked-fraction-needed-buffered-continent-$(CONT1).csv \
		quantiles-acked-fraction-needed-buffered-continent-$(CONT2).csv \
		quantiles-acked-fraction-needed-buffered-continent-$(CONT3).csv \
		quantiles-acked-fraction-needed-buffered-continent-$(CONT4).csv \
		quantiles-acked-fraction-needed-buffered-continent-$(CONT5).csv \
		quantiles-acked-fraction-needed-buffered-continent-$(CONT6).csv
	echo "$(CONT1N),,$(CONT2N),,$(CONT3N),,$(CONT4N),,$(CONT5N),,$(CONT6N)," > $@
	paste -d, $^ >> $@

quantiles-acked-fraction-needed-buffered-merged-all-asns.csv: \
		quantiles-acked-fraction-needed-buffered-asn-$(ASN1).csv \
		quantiles-acked-fraction-needed-buffered-asn-$(ASN2).csv \
		quantiles-acked-fraction-needed-buffered-asn-$(ASN3).csv \
		quantiles-acked-fraction-needed-buffered-asn-$(ASN4).csv \
		quantiles-acked-fraction-needed-buffered-asn-$(ASN5).csv \
		quantiles-acked-fraction-needed-buffered-asn-$(ASN6).csv
	echo "$(ASN1),,$(ASN2),,$(ASN3),,$(ASN4),,$(ASN5),,$(ASN6)," > $@
	paste -d, $^ >> $@

$(FIG_DIR)/acked-fraction-needed-buffered-cdf-merged-%.pdf: \
		quantiles-acked-fraction-needed-buffered-merged-%.csv $(MK_PATH)cdf_columns.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:2]" \
		-e "scale='1'" \
		-e "x_label='Fraction of acked bytes needed buffered'" \
		-e "num_lines=6" \
		-e "input='$<'" $(word 2, $^)

plots: $(FIG_DIR)/acked-fraction-needed-buffered-cdf-merged-all-asns.pdf
plots: $(FIG_DIR)/acked-fraction-needed-buffered-cdf-merged-all-countries.pdf
plots: $(FIG_DIR)/acked-fraction-needed-buffered-cdf-merged-all-continents.pdf
