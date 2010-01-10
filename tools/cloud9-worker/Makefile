LEVEL = ../..

TOOLNAME = c9-worker
USEDLIBS = cloud9common.a cloud9worker.a kleeCore.a kleeModule.a kleaverSolver.a kleaverExpr.a kleeSupport.a kleeBasic.a
LINK_COMPONENTS = jit engine bitreader bitwriter linker ipo

include $(LEVEL)/Makefile.common

ifeq ($(ENABLE_STPLOG), 1)
	LIBS += -lstplog
endif

LIBS += -lstp -lboost_system-mt -lboost_thread-mt -lprotoc