# Compilador
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# Nombres de archivos
TARGET = cache_sim
SRC = proyecto.cpp
OBJ = $(SRC:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

# Limpiar archivos generados
clean:
	rm -f $(OBJ) $(TARGET)
