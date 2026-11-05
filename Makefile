# Збирання в середовищі MSYS2 UCRT64:  mingw32-make
# Потрібні пакети: mingw-w64-ucrt-x86_64-gcc, mingw-w64-ucrt-x86_64-glfw
# Команди mkdir -p і rm -rf виконує оболонка sh з MSYS2

CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Isrc
LDLIBS   = -lglfw3 -lopengl32 -lgdi32

# Усе, крім main.cpp, спільне для програми й самоперевірки
LIB_SRC = $(filter-out src/main.cpp,$(wildcard src/*.cpp))
LIB_OBJ = $(LIB_SRC:src/%.cpp=build/%.o)

APP  = bin/scene3d.exe
TEST = bin/selftest.exe

all: $(APP) $(TEST)

# Графічна програма: статичне компонування, без консольного вікна
$(APP): build/main.o $(LIB_OBJ) | bin
	$(CXX) -static -mwindows -o $@ $^ $(LDLIBS)

# Консольна самоперевірка математики, сцени, ресурсів і зображень
$(TEST): build/selftest.o $(LIB_OBJ) | bin
	$(CXX) -static -o $@ $^ $(LDLIBS)

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

build/selftest.o: tests/selftest.cpp | build
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Зміна прапорів у Makefile перебудовує всі об'єкти
$(LIB_OBJ) build/main.o build/selftest.o: Makefile

bin build:
	mkdir -p $@

run: $(APP)
	./$(APP)

check: $(TEST)
	./$(TEST)

clean:
	rm -rf build bin

-include $(wildcard build/*.d)

.PHONY: all run check clean
