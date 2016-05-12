# Create plots with multiple continents plotted as separate lines in the same
# figure
CONT1 := AF
CONT2 := AS
CONT3 := EU
CONT4 := NA
CONT5 := SA
CONT6 := OC

CONT1N := "Africa"
CONT2N := "Asia"
CONT3N := "Europe"
CONT4N := "North America"
CONT5N := "South America"
CONT6N := "Oceania"

COUNTRY1 := DZ
COUNTRY2 := DE
COUNTRY3 := IN
COUNTRY4 := KZ
COUNTRY5 := RU
COUNTRY6 := US

COUNTRY1N := "Armenia"
COUNTRY2N := "Germany"
COUNTRY3N := "India"
COUNTRY4N := "Kazakhstan"
COUNTRY5N := "Russia"
COUNTRY6N := "United States"

ASN1 := AS701
ASN2 := AS852
ASN3 := AS6697
ASN4 := AS16345
ASN5 := AS24608
ASN6 := AS198471

quantiles-delays-XXmerged-col-%-all-continents.csv: \
		quantiles-delays-XXcol-%-continent-$(CONT1).csv \
		quantiles-delays-XXcol-%-continent-$(CONT2).csv \
		quantiles-delays-XXcol-%-continent-$(CONT3).csv \
		quantiles-delays-XXcol-%-continent-$(CONT4).csv \
		quantiles-delays-XXcol-%-continent-$(CONT5).csv \
		quantiles-delays-XXcol-%-continent-$(CONT6).csv
	echo "$(CONT1N),,$(CONT2N),,$(CONT3N),,$(CONT4N),,$(CONT5N),,$(CONT6N)," > $@
	paste -d, $^ >> $@

# Create plots with multiple countries plotted as separate lines in the same
# figure
quantiles-delays-XXmerged-col-%-all-countries.csv: \
		quantiles-delays-XXcol-%-country-$(COUNTRY1).csv \
		quantiles-delays-XXcol-%-country-$(COUNTRY2).csv \
		quantiles-delays-XXcol-%-country-$(COUNTRY3).csv \
		quantiles-delays-XXcol-%-country-$(COUNTRY4).csv \
		quantiles-delays-XXcol-%-country-$(COUNTRY5).csv \
		quantiles-delays-XXcol-%-country-$(COUNTRY6).csv
	echo "$(COUNTRY1N),,$(COUNTRY2N),,$(COUNTRY3N),,$(COUNTRY4N),,$(COUNTRY5N),,$(COUNTRY6N)," > $@
	paste -d, $^ >> $@

# Create plots with multiple ASNs plotted as separate lines in the same figure
quantiles-delays-XXmerged-col-%-all-asns.csv: \
		quantiles-delays-XXcol-%-asn-$(ASN1).csv \
		quantiles-delays-XXcol-%-asn-$(ASN2).csv \
		quantiles-delays-XXcol-%-asn-$(ASN3).csv \
		quantiles-delays-XXcol-%-asn-$(ASN4).csv \
		quantiles-delays-XXcol-%-asn-$(ASN5).csv \
		quantiles-delays-XXcol-%-asn-$(ASN6).csv
	echo "$(ASN1),,$(ASN2),,$(ASN3),,$(ASN4),,$(ASN5),,$(ASN6)," > $@
	paste -d, $^ >> $@

$(FIG_DIR)/XXdelay-cdf-merged-col-%.pdf: quantiles-delays-XXmerged-col-%.csv $(MK_PATH)cdf_columns.gp
	gnuplot -e "set output '$@'" \
		-e "set xrange [0:XRANGE]" \
		-e "scale='XSCALE'" \
		-e "x_label='XLABEL'" \
		-e "num_lines=6" \
		-e "input='$<'" $(word 2, $^)

$(FIG_DIR)/overall-XXdelay-cdf-merged-%.pdf: $(FIG_DIR)/XXdelay-cdf-merged-col-1-%.pdf
	mv $< $@

$(FIG_DIR)/base-XXdelay-cdf-merged-%.pdf: $(FIG_DIR)/XXdelay-cdf-merged-col-2-%.pdf
	mv $< $@

$(FIG_DIR)/loss-XXdelay-cdf-merged-%.pdf: $(FIG_DIR)/XXdelay-cdf-merged-col-3-%.pdf
	mv $< $@

$(FIG_DIR)/loss-trigger-XXdelay-cdf-merged-%.pdf: $(FIG_DIR)/XXdelay-cdf-merged-col-4-%.pdf
	mv $< $@

$(FIG_DIR)/queueing-XXdelay-cdf-merged-%.pdf: $(FIG_DIR)/XXdelay-cdf-merged-col-5-%.pdf
	mv $< $@

plots: $(FIG_DIR)/overall-XXdelay-cdf-merged-all-continents.pdf
plots: $(FIG_DIR)/base-XXdelay-cdf-merged-all-continents.pdf
plots: $(FIG_DIR)/loss-XXdelay-cdf-merged-all-continents.pdf
plots: $(FIG_DIR)/loss-trigger-XXdelay-cdf-merged-all-continents.pdf
plots: $(FIG_DIR)/queueing-XXdelay-cdf-merged-all-continents.pdf

plots: $(FIG_DIR)/overall-XXdelay-cdf-merged-all-countries.pdf
plots: $(FIG_DIR)/base-XXdelay-cdf-merged-all-countries.pdf
plots: $(FIG_DIR)/loss-XXdelay-cdf-merged-all-countries.pdf
plots: $(FIG_DIR)/loss-trigger-XXdelay-cdf-merged-all-countries.pdf
plots: $(FIG_DIR)/queueing-XXdelay-cdf-merged-all-countries.pdf

plots: $(FIG_DIR)/overall-XXdelay-cdf-merged-all-asns.pdf
plots: $(FIG_DIR)/base-XXdelay-cdf-merged-all-asns.pdf
plots: $(FIG_DIR)/loss-XXdelay-cdf-merged-all-asns.pdf
plots: $(FIG_DIR)/loss-trigger-XXdelay-cdf-merged-all-asns.pdf
plots: $(FIG_DIR)/queueing-XXdelay-cdf-merged-all-asns.pdf
