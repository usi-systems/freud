ROOT=./
WORKLOADS=mysql_classes
LOGDIR=$(ROOT)logs
EPSDIR=$(ROOT)eps
GPLDIR=$(ROOT)gnuplot
ANNDIR=$(ROOT)ann
CHECKDIR=$(ROOT)ass
SYMDIR=$(ROOT)symbols

ANALYZER=$(ROOT)freud-statistics
CHECKER=$(ROOT)freud-statistics

LOGFILES := $(wildcard $(LOGDIR)/*.bin)

.PHONY: all
all: analysis

.PHONY: clean
clean:
	rm -rf $(CHECKDIR)
	rm -rf $(ANNDIR)
	rm -rf $(EPSDIR)
	rm -rf $(GPLDIR)

$(SYMDIR): $(LOGFILES)
	rm -rf $(SYMDIR)
	mkdir $(SYMDIR)
	$(SYM_SPLIT) $(LOGFILES) -o $(SYMDIR)

SYMBOLS := $(patsubst $(SYMDIR)/%,%,$(wildcard $(SYMDIR)/*))
#EPSDIRS := $(addprefix $(EPSDIR)/,$(addsuffix _graphs,$(SYMBOLS)))
ANNDIRS := $(addprefix $(ANNDIR)/,$(addsuffix .txt,$(SYMBOLS)))
CHECKDIRS := $(addprefix $(CHECKDIR)/,$(addsuffix .txt,$(SYMBOLS)))

.PHONY: analysis
analysis: $(ANNDIRS)

$(EPSDIR):
	mkdir $@

$(ANNDIR):
	mkdir $@
	mkdir $(EPSDIR)
	mkdir $(GPLDIR) 

## $(info analysis_template called with parameter "$(1)")

define analysis_template
$$(ANNDIR)/$(1).txt: 
	-mkdir $$(ANNDIR) > /dev/null 2>&1
	time -o times.txt --append -f "%e" $(ANALYZER) 3 0.9 0 $(1) $$(SYMDIR)/$(1)/
endef

$(foreach sym,$(SYMBOLS),$(eval $(call analysis_template,$(sym))))

.PHONY: check
check: $(CHECKDIRS)

$(CHECKDIR):
	mkdir $@

define check_template
$$(CHECKDIR)/$(1).txt:
	-mkdir $$(CHECKDIR) > /dev/null 2>&1
	$(CHECKER) 5 0 0 $(1) $$(SYMDIR)/$(1)/ $(ANNDIR)
endef

$(foreach sym,$(SYMBOLS),$(eval $(call check_template,$(sym))))


