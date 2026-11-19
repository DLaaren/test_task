CC        ?= gcc
AR        ?= ar
ARFLAGS   ?= rcs
CFLAGS    ?= -Wall -Wextra -O2 -std=c11
CPPFLAGS  += -Iinclude
DEPFLAGS   = -MMD -MP

BUILD := build

LIBS := $(BUILD)/libstatdata.a \
	$(BUILD)/liblogging.a
BINS := $(BUILD)/processor \
	$(BUILD)/tester
OBJS := $(BUILD)/src/statdata.o \
	$(BUILD)/src/logging.o \
	$(BUILD)/tools/processor/processor.o \
	$(BUILD)/tools/tester/tester.o \
	$(BUILD)/tools/tester/tests.o

# --- default ---------------------------------------------------------------
all: $(LIBS) $(BINS)

# --- convenience aliases ---------------------------------------------------
processor: $(BUILD)/processor
tester:    $(BUILD)/tester
test:      all
	./$(BUILD)/tester

# --- libraries -------------------------------------------------------------
$(BUILD)/libstatdata.a: $(BUILD)/src/statdata.o
	$(AR) $(ARFLAGS) $@ $^

$(BUILD)/liblogging.a: $(BUILD)/src/logging.o
	$(AR) $(ARFLAGS) $@ $^

# --- executables -----------------------------------------------------------
$(BUILD)/processor: $(BUILD)/tools/processor/processor.o $(LIBS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/tester: $(BUILD)/tools/tester/tester.o $(BUILD)/tools/tester/tests.o $(LIBS)
	$(CC) $(CFLAGS) -o $@ $^

# --- every object file, mirroring the source tree --------------------------
$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

# --- auto-generated header deps (-MMD -MP) --------------------------------
-include $(OBJS:.o=.d)

# --- housekeeping ----------------------------------------------------------
clean:
	rm -rf $(BUILD)

help:
	@echo "make            - build the libraries and the tools into $(BUILD)/"
	@echo "make processor  - build $(BUILD)/processor only (and the libraries)"
	@echo "make tester     - build $(BUILD)/tester only (and the libraries)"
	@echo "make test       - build and run $(BUILD)/tester"
	@echo "make clean      - remove $(BUILD)/"
	@echo "make help       - this message"

.PHONY: all processor tester test clean help
