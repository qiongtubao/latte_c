# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.15

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/Cellar/cmake/3.15.3/bin/cmake

# The command to remove a file.
RM = /usr/local/Cellar/cmake/3.15.3/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/zhouguodong/Documents/workspace/libs/latte_c

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/zhouguodong/Documents/workspace/libs/latte_c/build

# Include any dependencies generated for this target.
include CMakeFiles/latte_c.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/latte_c.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/latte_c.dir/flags.make

CMakeFiles/latte_c.dir/latte_c.c.o: CMakeFiles/latte_c.dir/flags.make
CMakeFiles/latte_c.dir/latte_c.c.o: ../latte_c.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/zhouguodong/Documents/workspace/libs/latte_c/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/latte_c.dir/latte_c.c.o"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/latte_c.dir/latte_c.c.o   -c /Users/zhouguodong/Documents/workspace/libs/latte_c/latte_c.c

CMakeFiles/latte_c.dir/latte_c.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/latte_c.dir/latte_c.c.i"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /Users/zhouguodong/Documents/workspace/libs/latte_c/latte_c.c > CMakeFiles/latte_c.dir/latte_c.c.i

CMakeFiles/latte_c.dir/latte_c.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/latte_c.dir/latte_c.c.s"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /Users/zhouguodong/Documents/workspace/libs/latte_c/latte_c.c -o CMakeFiles/latte_c.dir/latte_c.c.s

# Object files for target latte_c
latte_c_OBJECTS = \
"CMakeFiles/latte_c.dir/latte_c.c.o"

# External object files for target latte_c
latte_c_EXTERNAL_OBJECTS =

latte_c: CMakeFiles/latte_c.dir/latte_c.c.o
latte_c: CMakeFiles/latte_c.dir/build.make
latte_c: ../deps/jemalloc/lib/libjemalloc.a
latte_c: libs/libzmalloc.a
latte_c: CMakeFiles/latte_c.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/Users/zhouguodong/Documents/workspace/libs/latte_c/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable latte_c"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/latte_c.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/latte_c.dir/build: latte_c

.PHONY : CMakeFiles/latte_c.dir/build

CMakeFiles/latte_c.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/latte_c.dir/cmake_clean.cmake
.PHONY : CMakeFiles/latte_c.dir/clean

CMakeFiles/latte_c.dir/depend:
	cd /Users/zhouguodong/Documents/workspace/libs/latte_c/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/zhouguodong/Documents/workspace/libs/latte_c /Users/zhouguodong/Documents/workspace/libs/latte_c /Users/zhouguodong/Documents/workspace/libs/latte_c/build /Users/zhouguodong/Documents/workspace/libs/latte_c/build /Users/zhouguodong/Documents/workspace/libs/latte_c/build/CMakeFiles/latte_c.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/latte_c.dir/depend

