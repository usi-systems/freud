#include "instr-expr-context.hh"

extern bool ctx_used;

instr_expr_context::instr_expr_context (dwarf::taddr ppc) : ipc{ppc} {
	// Using 8 registers: RDI .. R9 + RSP + RBP
	for (int r = 0; r < CONTEXT_REGS; r++) {
		registers[r].reg = r;
		registers[r].offset = 0;
	}
}

dwarf::register_content instr_expr_context::reg (unsigned regnum) {
	ctx_used = true; 
	return registers[regnum];
}

void instr_expr_context::set_reg(unsigned regnum, unsigned newreg, int64_t offset) {
	registers[regnum].reg = newreg;
	registers[regnum].offset = offset;
}

void instr_expr_context::add_reg_offset(unsigned regnum, int64_t off) {
	registers[regnum].offset += off;
}

void instr_expr_context::set_cfa(unsigned regnum, int64_t offset) {
	std::cout << "Setting CFA_ to " << regnum << "; " << offset << std::endl;
	cfa.regnum = regnum;
	cfa.offset = offset;
}

dwarf::cfa_eval_result instr_expr_context::get_cfa() {
	return cfa;
}

unsigned instr_expr_context::get_cfa_reg() {
	return cfa.regnum;
}

int64_t instr_expr_context::get_cfa_offset() {
	return cfa.offset;
}

void instr_expr_context::set_cfa_reg(unsigned regnum) {
	std::cout << "Setting CFA_REG to " << regnum << std::endl;
	cfa.regnum = regnum;
}

void instr_expr_context::set_cfa_offset(int64_t off) {
	std::cout << "Setting CFA_OFF to " << off << std::endl;
	cfa.offset = off;
}

dwarf::taddr instr_expr_context::pc() {
	return ipc;
}

dwarf::taddr instr_expr_context::deref_size (dwarf::taddr address, unsigned size) {
	//TODO take into account size
	ctx_used = true;
	printf("ASKING FOR DEREF %u\n", size);
	return 0;
}

