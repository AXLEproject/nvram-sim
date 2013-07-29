##
## To build the tool, execute the make command:
##
##      make
##
## After building your tool, you would invoke Pin like this:
##
##      $PIN_ROOT/pin -t nvramsim -- /bin/ls
##

############## CONFIG START #####################
PINVER="pin-2.12-58423-gcc.4.4.7-linux"
PIN_ROOT ?= pin
PIN_KIT=$(PIN_ROOT)

TARGET_COMPILER?=gnu
CXX := g++
PIN_LD := g++

TOOL_ROOTS = nvramsim
## Additional dependencies of this tool (c/cpp/object files)
DEP_ROOTS = cache-sim/cache cache-sim/logger
############## CONFIG END #####################

OBJDIR := obj-intel64
TOOLS = $(TOOL_ROOTS:%=$(OBJDIR)/%.so)
DEP_SRCS := $(foreach d, $(DEP_ROOTS), $(wildcard $(d).cpp)) $(foreach d, $(DEP_ROOTS), $(wildcard $(d).[ch]))
DEP_OBJS = $(DEP_ROOTS:%=$(OBJDIR)/%.o)

#DBG := -g -rdynamic -DDEBUG=1
DBG := -g -rdynamic
EXTRA_CXXFLAGS := -Wall -Werror -Wno-unknown-pragmas -fno-stack-protector
PIN_CXXFLAGS := -DBIGARRAY_MULTIPLIER=1 -DUSING_XED -DTARGET_IA32E -DHOST_IA32E -fPIC -DTARGET_LINUX
PIN_INCLUDES := -Ipin/source/include/pin -Ipin/source/include/pin/gen -Ipin/extras/components/include -Ipin/extras/xed2-intel64/include -Ipin/source/tools/InstLib

PIN_OPT := -O3 -fomit-frame-pointer -fno-strict-aliasing
PIN_LDFLAGS := -shared -Wl,--hash-style=sysv -Wl,-rpath=pin/intel64/runtime/cpplibs -Wl,-Bsymbolic -Wl,--version-script=pin/source/include/pin/pintool.ver
PIN_LPATHS := -Lpin/intel64/runtime/cpplibs -Lpin/intel64/lib -Lpin/intel64/lib-ext -Lpin/intel64/runtime/glibc -Lpin/extras/xed2-intel64/lib
PIN_LIBS := -lpin -lxed -ldwarf -lelf -ldl
EXTRA_LIBS :=

all: pin_check
tools: $(OBJDIR) $(TOOLS)
test: $(OBJDIR) $(TOOL_ROOTS:%=%.test)

pin_check:
	test -d $(PIN_ROOT) || $(MAKE) pin_download
	$(MAKE) tools

pin_download:
	@echo "-----------------------"
	@echo "To use this program, you must agree to the license of the Pin Tool, that you can read at:"
	@echo "http://software.intel.com/sites/default/files/m/a/d/2/extlicense.txt"
	@echo "If you do not agree, press Ctrl+C in the next 2 seconds, to quit"
	@echo "-----------------------"
	@sleep 2
	@echo "Downloading and unpacking PIN tool"
	@wget -nv -c "http://software.intel.com/sites/landingpage/pintool/downloads/$(PINVER).tar.gz" -O- | tar xz
	@echo "Done. PIN tool is downloaded and ready"
	@mv "$(PINVER)" pin

$(OBJDIR):
	@mkdir -p "$(OBJDIR)"

$(OBJDIR)/%.o : %.cpp Makefile $(DEP_SRCS)
	@mkdir -p "$(shell dirname '$(OBJDIR)/$<')"
	@$(CXX) $(EXTRA_CXXFLAGS) $(PIN_CXXFLAGS) $(PIN_INCLUDES) $(PIN_OPT) $(DBG) -c $< -o $@
	@echo "CXX $< -c -o $@"

$(TOOLS): %.so : %.o $(DEP_OBJS)
	@${PIN_LD} $(PIN_LDFLAGS) $(LINK_DEBUG) $(DEP_OBJS) -o ${LINK_OUT}$@ $< ${PIN_LPATHS} $(PIN_LIBS) $(EXTRA_LIBS) $(DBG);
	@echo "LD $< $(DEP_OBJS) -o $@"

clean:
	rm -rf $(OBJDIR) *.out *.tested *.failed

