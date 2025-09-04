CXX = g++
CXXFLAGS = -std=c++17
LDFLAGS = -lreadline

SRC = main.cpp shell.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = my_shell

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJ) $(TARGET)

