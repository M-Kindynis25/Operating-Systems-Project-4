# Makefile
# Compiler to use
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -Wextra -std=c++11 -g

# Object files
OBJS = myz.o MyzArchive.o

# Final executable
TARGET = myz

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Compile individual source files
%.o: %.cpp %.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean target to remove compiled binaries and object files
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)

# Run target
.PHONY: run
run: $(TARGET)
	./$(TARGET) -c -j aaa.myz file.txt dir2
	make clean

run2: $(TARGET)
	./$(TARGET) -c -j bbb.myz ad-sample
	make clean

# Valgrind target
.PHONY: valgrind
valgrind: $(TARGET)
	valgrind --leak-check=full --track-origins=yes ./$(TARGET) -c aaa.myz file2 file4
	make clean
