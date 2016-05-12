random-flow-delays-col-1-%.csv: random-flow-delays-%.csv
	cut -d, -f 1 $< > $@
random-flow-delays-per-year-col-1-%.csv: random-flow-delays-per-year-%.csv
	cut -d, -f 1,2 $< > $@
random-flow-delays-col-2-%.csv: random-flow-delays-%.csv
	cut -d, -f 2 $< > $@
random-flow-delays-per-year-col-2-%.csv: random-flow-delays-per-year-%.csv
	cut -d, -f 1,3 $< > $@
random-flow-delays-col-3-%.csv: random-flow-delays-%.csv
	cut -d, -f 3 $< > $@
random-flow-delays-per-year-col-3-%.csv: random-flow-delays-per-year-%.csv
	cut -d, -f 1,4 $< > $@
random-flow-delays-col-4-%.csv: random-flow-delays-%.csv
	cut -d, -f 4 $< > $@
random-flow-delays-per-year-col-4-%.csv: random-flow-delays-per-year-%.csv
	cut -d, -f 1,5 $< > $@
random-flow-delays-col-5-%.csv: random-flow-delays-%.csv
	cut -d, -f 5 $< > $@
random-flow-delays-per-year-col-5-%.csv: random-flow-delays-per-year-%.csv
	cut -d, -f 1,6 $< > $@
normalized-tail-breakdown-col-2-%.csv: normalized-tail-breakdown-%.csv
	cut -d, -f 1 $< > $@
relative-tail-breakdown-col-2-%.csv: relative-tail-breakdown-%.csv
	cut -d, -f 1 $< > $@
normalized-tail-breakdown-col-3-%.csv: normalized-tail-breakdown-%.csv
	cut -d, -f 2 $< > $@
relative-tail-breakdown-col-3-%.csv: relative-tail-breakdown-%.csv
	cut -d, -f 2 $< > $@
normalized-tail-breakdown-col-4-%.csv: normalized-tail-breakdown-%.csv
	cut -d, -f 3 $< > $@
relative-tail-breakdown-col-4-%.csv: relative-tail-breakdown-%.csv
	cut -d, -f 3 $< > $@
normalized-tail-breakdown-col-5-%.csv: normalized-tail-breakdown-%.csv
	cut -d, -f 4 $< > $@
relative-tail-breakdown-col-5-%.csv: relative-tail-breakdown-%.csv
	cut -d, -f 4 $< > $@
