DEFINES+=PROJECT_CONF_H=\"project-conf.h\"
CONTIKI_PROJECT = project
APPS+=powertrace
all: $(CONTIKI_PROJECT) token_1 token_2

CONTIKI_WITH_RIME = 1

CONTIKI = ../..
include $(CONTIKI)/Makefile.include
