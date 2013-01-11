##
## To build the tool, execute the make command:
##
##      make
## or
##      make PIN_HOME=<top-level directory where Pin was installed>
##
## After building your tool, you would invoke Pin like this:
##
##      $PIN_HOME/pin -t nvramsim -- /bin/ls
##

PINVER="pin-2.12-55942-gcc.4.4.7-linux"
PIN_HOME ?= pin

PIN_KIT=$(PIN_HOME)

TARGET_COMPILER?=gnu

CXXFLAGS ?= -Wall -Werror -Wno-unknown-pragmas $(DBG) $(OPT)
-include $(PIN_HOME)/source/tools/makefile.gnu.config

TOOL_ROOTS = nvramsim

TOOLS = $(TOOL_ROOTS:%=$(OBJDIR)%$(PINTOOL_SUFFIX))

EXTRA_LIBS =

## Additional objects for building this tool, e.g.
# OBJS = $(OBJDIR)CallStack.o

##############################################################
#
# build rules
#
##############################################################

all: pin_check
tools: $(OBJDIR) $(TOOLS) $(OBJDIR)cp-pin
test: $(OBJDIR) $(TOOL_ROOTS:%=%.test)

pin_download:
	@echo "-----------------------"
	@echo "To use this program, you must agree to the license of the Pin Tool, that you can read at:"
	@echo "http://software.intel.com/sites/default/files/m/a/d/2/extlicense.txt"
	@echo "If you do not agree, press Ctrl+C now, to quit"
	@echo "-----------------------"
	@sleep 2
	@wget -nv -c "http://software.intel.com/sites/landingpage/pintool/downloads/$(PINVER).tar.gz"
	@echo "Unpacking PIN tool"
	@tar xf "$(PINVER).tar.gz"
	@ln -sf "$(PINVER)" pin

pin_check:
ifeq ($(KIT), 1)
	$(MAKE) tools
else
	$(MAKE) pin_download
	$(MAKE) tools
endif

nvramsim.test: $(OBJDIR)cp-pin
	$(MAKE) -k PIN_HOME=$(PIN_HOME)

$(OBJDIR)cp-pin:
	$(CXX) $(PIN_HOME)/source/tools/Tests/cp-pin.cpp $(APP_CXXFLAGS) -o $(OBJDIR)cp-pin

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)%.o : %.cpp Makefile
	$(CXX) -c $(CXXFLAGS) $(PIN_CXXFLAGS) ${OUTOPT}$@ $<

$(TOOLS): $(PIN_LIBNAMES) $(OBJS)

$(TOOLS): %$(PINTOOL_SUFFIX) : %.o
	  ${PIN_LD} $(PIN_LDFLAGS) $(LINK_DEBUG) $(OBJS) ${LINK_OUT}$@ $< ${PIN_LPATHS} $(PIN_LIBS) $(EXTRA_LIBS) $(DBG);

## cleaning
clean:
	@rm -rf $(OBJDIR) *.out *.tested *.failed makefile.copy

