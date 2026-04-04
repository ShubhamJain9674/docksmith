BUILD_DIR = build
BUILD_DEBUG = build/debug
BUILD_RELEASE = build/release

.PHONY: all debug release clean

all: debug

debug:
	@mkdir -p $(BUILD_DEBUG)
	cd $(BUILD_DEBUG) && cmake -DCMAKE_BUILD_TYPE=Debug ../.. && make -j

release:
	@mkdir -p $(BUILD_RELEASE)
	cd $(BUILD_RELEASE) && cmake -DCMAKE_BUILD_TYPE=Release ../.. && make -j


clean:
	rm -rf $(BUILD_DIR)