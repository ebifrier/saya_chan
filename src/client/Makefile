#
# Makefile for client
#

include ../Makefile-common
OPT_CPPFLAGS = -DBOOST_LOG_DYN_LINK -DGODWHALE_CLIENT
OPT_CXXFLAGS = -fpermissive
OPT_LIBS     = -lboost_thread -lboost_system -lboost_timer -lboost_atomic \
               -lboost_chrono -lboost_log -lboost_log_setup -lboost_regex \
               -lboost_program_options
OPT = -DNDEBUG -DNANOHA -DEVAL_OLD -DCHK_PERFORM -DOLD_LOCKS  \
      -DUSE_POPCNT -DBITBOARD64 -DUSE_FAST_BIN -DNO_NARAZU

INCLUDES = -I. -I.. -I/usr/include $(OPT_INCLUDES)
CPPFLAGS = $(OPT) $(OPT_CPPFLAGS)
CXXFLAGS = -std=c++11 -Wall -Wno-unknown-pragmas -g \
           -march=native -mtune=native -Ofast $(OPT_CXXFLAGS)
LIBS     = $(OPT_LIBS) -pthread -lm

BIN  = one_godwhale
OBJS = 	client.cpp client_mainloop.cpp \
		cmovenoderow.cpp cmovenodetree.cpp \
		main_client.cpp sayachan_client.cpp

SRCS = $(FILES:.o=.cpp)

PRECOMP_H   = ../precomp.h
PRECOMP_GCH = ../precomp.h.gch

.DEFAULT_GOAL = all

all: $(BIN)

.cpp.o:
	$(CXX) $(INCLUDES) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BIN): $(OBJS) $(COMMON_OBJS) $(SAYA_OBJS)
	$(CXX) $(INCLUDES) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS)
$(PRECOMP_GCH): $(PRECOMP_H)
	$(CXX) $(INCLUDES) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
	
depend: $(SRCS)
	$(CXX) $(INCLUDES) $(CPPFLAGS) $(FLAGS) -MM -MG $(SRCS) > make.dep

clean: common-clean
	rm -f *.o *.exe $(BIN) $(PRECOMP_GCH)

clean-full: clean
	rm -f make.dep

$(OBJS): $(PRECOMP_GCH)

-include make.dep
