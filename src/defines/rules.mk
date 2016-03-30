EXE_TARGET_OBJS = $(foreach objs, \
	$(addsuffix _OBJS, $(EXE_TARGETS)), \
	$(addprefix $(OBJDIR)/, $($(objs))))

LINK_TARGETS = $(addprefix $(OBJDIR)/, $(EXE_TARGETS))

all:: $(LINK_TARGETS)

# Pull in auto-generated dependency information
-include $(wildcard $(OBJDIR)/*.d)

$(LINK_TARGETS):: $(EXE_TARGET_OBJS)
	$(CC) -g -o $@$(EXE) $(addprefix $(OBJDIR)/, $($(notdir $(addsuffix _OBJS, ${@})))) \
		$(addprefix -l, $($(notdir $(addsuffix _LIBS, ${@})))) $(LD_FLAGS)

# Really should have variables for the dependency switches, but
# there's only so many hours in the day

$(OBJDIR)/%$(OBJ):: %.c
	@test -d $(OBJDIR) || $(MKDIR) $(OBJDIR)
	$(CC) -g $(CC_FLAGS) $($(notdir $(addsuffix _DEFS, $(basename ${@})))) $(INCDIR) -c -o $@ $<
	$(CC) $(CC_FLAGS) $($(notdir $(addsuffix _DEFS, $(basename ${@})))) \
		$(INCDIR) -MM -MF $(OBJDIR)/$*$(DEP) -MT $(OBJDIR)/$*$(OBJ) $(CC_FLAGS) $<

clean::
	-@$(RM) $(OBJDIR)/*
	-@$(RMDIR) $(OBJDIR)

test::
