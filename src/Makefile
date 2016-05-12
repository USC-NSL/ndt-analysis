CFLAGS=-Wall -g3 -O3 -std=c++14 -D__FAVOR_BSD
LFLAGS=-lpcap -lgsl -lgslcblas -lm -lgflags -lglog
CC=g++
AR=ar
ARFLAGS=-rv

GTEST_DIR=googletest/googletest
GTEST_LFLAGS=-lpthread

TARGETS=analyze_latency
TEST_TARGETS=test_latency
ALL_TARGETS=$(TARGETS) $(TEST_TARGETS)

# Enable the compilation of multiple executables without defining
# separate rules. This workaround will exclude all *.cc files
# corresponding to the targets above EXCEPT for the command line
# target provided
ifneq (,$(findstring $(MAKECMDGOALS),$(ALL_TARGETS)))
NON_TARGETS := $(filter-out $(MAKECMDGOALS), $(ALL_TARGETS))
NON_TARGET_SRCS := $(addsuffix .cc, $(NON_TARGETS))
endif

SRCS := $(wildcard *.cc)
SRCS := $(filter-out $(NON_TARGET_SRCS), $(SRCS))
OBJS := $(SRCS:.cc=.o)

# Need at least make version 3.82 to enable shortest stem matching
ifneq (3.82, $(firstword $(sort $(MAKE_VERSION) 3.82)))
$(error "Need 'make' version 3.82 or higher.")
endif

.PHONY: clean

-include $(OBJS:.o=.d)

clean:
	rm $(OBJS) $(TARGETS) $(OBJS:.o=.d)

$(TARGETS): $(OBJS)
	$(CC) $^ $(LFLAGS) -o $@

$(TEST_TARGETS): $(OBJS) gtest_main.a
	$(CC) $^ $(LFLAGS) $(GTEST_LFLAGS) -o $@

%.o: %.cc
	$(CC) -MM $(CFLAGS) $< > $*.d
	$(CC) -c $(CFLAGS) $< -o $@

test_%.o: test_%.cc
	$(CC) -isystem $(GTEST_DIR)/include -I$(GTEST_DIR) -MM $(CFLAGS) $< > $*.d
	$(CC) -isystem $(GTEST_DIR)/include -I$(GTEST_DIR) -c $(CFLAGS) $< -o $@

# Builds gtest.a and gtest_main.a.

# All Google Test headers.  Usually you shouldn't change this
# definition.
GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h \
                $(GTEST_DIR)/include/gtest/internal/*.h

# Usually you shouldn't tweak such internal variables, indicated by a
# trailing _.
GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)

# For simplicity and to avoid depending on Google Test's
# implementation details, the dependencies specified below are
# conservative and not optimized.  This is fine as Google Test
# compiles fast and for ordinary users its source rarely changes.
gtest-all.o : $(GTEST_SRCS_)
	$(CC) -isystem $(GTEST_DIR)/include -I$(GTEST_DIR) $(CFLAGS) -c \
            $(GTEST_DIR)/src/gtest-all.cc

gtest_main.o : $(GTEST_SRCS_)
	$(CC) -isystem $(GTEST_DIR)/include -I$(GTEST_DIR) $(CFLAGS) -c \
            $(GTEST_DIR)/src/gtest_main.cc

gtest.a : gtest-all.o
	$(AR) $(ARFLAGS) $@ $^

gtest_main.a : gtest-all.o gtest_main.o
	$(AR) $(ARFLAGS) $@ $^
