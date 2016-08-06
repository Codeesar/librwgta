# null, opengl
BUILD=null

RWDIR=$(HOME)/src/librw

# e.g. null -> -DRW_NULL
BUILDDEF:=$(shell echo $(BUILD) | tr a-z A-Z | sed 's/^/-DRW_/')
BUILDDIR=build-$(BUILD)
SRCDIR=src
#SRC := $(patsubst %.cpp,$(SRCDIR)/%.cpp, $(wildcard *.cpp))
SRC := $(wildcard $(SRCDIR)/*.cpp)
OBJ := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRC))
DEP := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.d,$(SRC))
INC := -I/usr/local/include -I$(RWDIR)
CFLAGS=-Wall -Wextra -g $(BUILDDEF) -Wno-parentheses #-Wconversion
LIB=librwgta-$(BUILD).a

$(LIB): $(OBJ)
	ar scr $@ $(OBJ)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) $(INC) -c $< -o $@

$(BUILDDIR)/%.d: $(SRCDIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) -MM -MT '$(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$<)' $(CFLAGS) $(INC) $< > $@

clean:
	rm -f $(BUILDDIR)/*.[od]

-include $(DEP)
