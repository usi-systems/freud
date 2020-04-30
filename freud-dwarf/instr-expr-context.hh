#ifndef INSTR_EXPR_CONTEXT_HH_INCLUDED
#define INSTR_EXPR_CONTEXT_HH_INCLUDED

#include <string>
#include <iostream>

#include "dwarf++.hh"
#include "elf++.hh"
#include "structures.hh"

class instr_expr_context : public dwarf::expr_context {
public:
    instr_expr_context (dwarf::taddr ppc);
    
    dwarf::register_content reg (unsigned regnum) override;

    void set_reg(unsigned regnum, unsigned newreg, int64_t offset);

    void add_reg_offset(unsigned regnum, int64_t off);

    void set_cfa(unsigned regnum, int64_t offset);

    dwarf::cfa_eval_result get_cfa();

    unsigned get_cfa_reg();

    int64_t get_cfa_offset();

    void set_cfa_reg(unsigned regnum);

    void set_cfa_offset(int64_t off);

    dwarf::taddr pc() override;

    dwarf::taddr deref_size (dwarf::taddr address, unsigned size) override;

private:
    dwarf::taddr ipc;
    struct dwarf::register_content registers[CONTEXT_REGS];
    struct dwarf::cfa_eval_result cfa;
};


#endif
