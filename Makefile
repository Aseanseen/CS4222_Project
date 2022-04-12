DEFINES+=PROJECT_CONF_H=\"project-conf.h\"
APPS+=powertrace

CONTIKI_WITH_RIME = 1

CONTIKI = ../
TARGET_LIBFILES = -lm
include $(CONTIKI)/Makefile.include
LDFLAGS += -lm
