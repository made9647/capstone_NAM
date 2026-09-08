TARGET = passthrough
NAM_DIR = libs/NeuralAmpModelerCore
NAM_SOURCES = src/models/a2_lite.cpp build/nam/dsp.cpp
CPP_SOURCES = src/main.cpp src/audio/nam_processor.cpp src/audio/nam_audio.cpp src/models/amp_models.cpp $(NAM_SOURCES)
C_INCLUDES = -Isrc -Ibuild/generated -I$(NAM_DIR) -I$(NAM_DIR)/NAM -I$(NAM_DIR)/Dependencies/eigen -I$(NAM_DIR)/Dependencies/nlohmann
C_DEFS = -DNAM_SAMPLE_FLOAT
CPP_STANDARD = -std=gnu++17
# Optimize application and NAM code for speed; libDaisy builds with -Os.
OPT = -O3
LIBDAISY_DIR = libs/libDaisy
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
APP_TYPE = BOOT_NONE
include $(SYSTEM_FILES_DIR)/Makefile

# NAM uses exceptions for configuration errors. Keep IEEE floating-point
# behavior until model accuracy and target performance have been measured.
CPPFLAGS += -fexceptions
include make/models.mk

# Archive the selected inference sources separately from application code.
NAM_OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(NAM_SOURCES:.cpp=.o)))
OBJECTS := $(filter-out $(NAM_OBJECTS),$(OBJECTS))
NAM_LIBRARY = $(BUILD_DIR)/libnam.a
LIBS := $(NAM_LIBRARY) $(LIBS)
$(NAM_LIBRARY): $(NAM_OBJECTS)
	rm -f $@
	$(CXX:g++=ar) rcs $@ $^

# Used in dry-run mode to generate clangd commands without requiring a link.
.PHONY: compile-objects
compile-objects: $(OBJECTS) $(NAM_OBJECTS)

$(OBJECTS) $(NAM_OBJECTS): make/firmware.mk
$(BUILD_DIR)/amp_models.o: $(A2_HEADER)
$(BUILD_DIR)/$(TARGET).elf: make/firmware.mk $(LIBDAISY_DIR)/build/libdaisy.a $(NAM_LIBRARY)

# The bare-metal runtime has no TLS. Model creation occurs on the main thread;
# keep upstream untouched and use a process-wide prewarm default in firmware.
build/nam/dsp.cpp: $(NAM_DIR)/NAM/dsp.cpp make/firmware.mk
	@mkdir -p $(@D)
	sed 's/^thread_local bool gPrewarmOnResetDefault/static bool gPrewarmOnResetDefault/' $< > $@

$(BUILD_DIR)/dsp.o: build/nam/dsp.cpp | $(BUILD_DIR)
	$(CXX) -c $(CPPFLAGS) $(CPP_STANDARD) $< -o $@
