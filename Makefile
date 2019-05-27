
TARGET:= shist
SOURCE:= $(wildcard *.cpp)
INCLUDE_DIRS:=cxxopts
BUILD_DIR:= .build
CFLAGS:= $(addprefix -I, $(INCLUDE_DIRS)) -g -std=c++17
LDFLAGS:= -lreadline -lncurses -lpthread

.PHONY: all
all: shist

OBJECTS:= $(filter %.o,$(SOURCE:%.cpp=$(BUILD_DIR)/%.o))
DEPENDENCIES:= $(filter %.d,$(SOURCE:%.cpp=$(BUILD_DIR)/%.d))

-include $(DEPENDENCIES)

$(TARGET): $(OBJECTS)
	g++ $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	g++ $(CFLAGS) -MF $(BUILD_DIR)/$*.d -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(DEPENDENCIES)
	rmdir $(BUILD_DIR)
