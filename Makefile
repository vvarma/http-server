BUILD_DIR ?= local_build
INSTALL_DIR ?= local_build
BUILD_PROFILE ?= default
HOST_PROFILE ?= default
BUILD_TYPE ?= Release

CPPCHECK-exists := $(shell command -v cppcheck 2> /dev/null)
CLANG_TIDY-exists := $(shell command -v clang-tidy 2> /dev/null)

CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_TOOLCHAIN_FILE=$(BUILD_DIR)/conan_toolchain.cmake 

.PHONY: all
all: clean build

.PHONY: clean 
clean:
	rm -rf $(BUILD_DIR)

.PHONY: conan
conan:
	conan install . -if=$(BUILD_DIR) -of=$(BUILD_DIR) --build=missing --profile:build=$(BUILD_PROFILE) --profile:host=$(HOST_PROFILE)

.PHONY: build
build: conan
	cmake -S . -B $(BUILD_DIR) -G Ninja $(CMAKE_FLAGS) 
	cmake --build $(BUILD_DIR) 

.PHONY: test
test: CMAKE_FLAGS += -DENABLE_TESTS=ON
test: build
	ctest --test-dir $(BUILD_DIR) -V

.PHONY: check CPPCHECK-exists CLANG_TIDY-exists
check: CMAKE_FLAGS += -DCMAKE_CXX_CPPCHECK=cppcheck -DCMAKE_CXX_CLANG_TIDY=clang-tidy 
check: build

