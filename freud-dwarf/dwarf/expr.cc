// Copyright (c) 2013 Austin T. Clements. All rights reserved.
// Use of this source code is governed by an MIT license
// that can be found in the LICENSE file.

#include "internal.hh"
#include "inttypes.h"

using namespace std;

DWARFPP_BEGIN_NAMESPACE

expr_context no_expr_context;

expr::expr(const unit *cu,
           section_offset offset, section_length len)
        : cu(cu), offset(offset), len(len)
{
}

// Section 2.5.1.2: to resolve the fbreg operation, I need the parent
// (that is the subroutine or entry point DIE that contains the
// frame_base attribute)

expr_result
expr::evaluate(expr_context *ctx, std::string &evaluation_string, const die * parent) const
{
        return evaluate(ctx, {}, evaluation_string, parent);
}

expr_result
expr::evaluate(expr_context *ctx, taddr argument, std::string &evaluation_string, const die * parent) const
{
        return evaluate(ctx, {argument}, evaluation_string, parent);
}


expr_result
expr::evaluate(expr_context *ctx, const std::initializer_list<taddr> &arguments, std::string &evaluation_string, const die * parent) const
{
	evaluation_string = "";
        // The stack machine's stack.  The top of the stack is
        // stack.back().
        // XXX This stack must be in target machine representation,
        // since I see both (DW_OP_breg0 (eax): -28; DW_OP_stack_value)
        // and (DW_OP_lit1; DW_OP_stack_value).
        small_vector<taddr, 8> stack;

        // Create the initial stack.  arguments are in reverse order
        // (that is, element 0 is TOS), so reverse it.
        stack.reserve(arguments.size());
	// fix for g++ version >= 9, where end() and begin() can be 0
	if (arguments.end() - arguments.begin() > 0) {
		for (const taddr *elt = arguments.end() - 1;
		     elt >= arguments.begin(); elt--) {
			stack.push_back(*elt);
		}
	}

        // Create a subsection for just this expression so we can
        // easily detect the end (including premature end).
        auto cusec = cu->data();
        shared_ptr<section> subsec
                (make_shared<section>(cusec->type,
                                      cusec->begin + offset, len,
                                      cusec->ord, cusec->fmt,
                                      cusec->addr_size));
        cursor cur(subsec);

        // Prepare the expression result.  Some location descriptions
        // create the result directly, rather than using the top of
        // stack.
        expr_result result;
	result.offset_from_value = 0;
        // 2.6.1.1.4 Empty location descriptions
        if (cur.end()) {
                result.location_type = expr_result::type::empty;
                result.value = 0;
                return result;
        }

        // Assume the result is an address for now and should be
        // grabbed from the top of stack at the end.
        result.location_type = expr_result::type::address;

        // Execute!
        while (!cur.end()) {
#define CHECK() do { if (stack.empty()) goto underflow; } while (0)
#define CHECKN(n) do { if (stack.size() < n) goto underflow; } while (0)
                union
                {
                        uint64_t u;
                        int64_t s;
                } tmp1, tmp2, tmp3;
                static_assert(sizeof(tmp1) == sizeof(taddr), "taddr is not 64 bits");

                // Tell GCC to warn us about missing switch cases,
                // even though we have a default case.
#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wswitch-enum"
                DW_OP op = (DW_OP)cur.fixed<ubyte>();
                switch (op) {
                        // 2.5.1.1 Literal encodings
                case DW_OP::lit0...DW_OP::lit31:
                        stack.push_back((unsigned)op - (unsigned)DW_OP::lit0);
			evaluation_string += "+ " + std::to_string((unsigned)op - (unsigned)DW_OP::lit0);
                        break;
                case DW_OP::addr:
			{
			auto addr = cur.address();
                        stack.push_back(addr);
			evaluation_string += "+ " + std::to_string(addr);
                        break;
			}
                case DW_OP::const1u:
			{
			auto u8 = cur.fixed<uint8_t>();
                        stack.push_back(u8);
			evaluation_string += "+ " + std::to_string(u8);
                        break;
			}
                case DW_OP::const2u:
                        {
			auto u16 = cur.fixed<uint16_t>(); 
			stack.push_back(u16);
			evaluation_string += "+ " + std::to_string(u16);
                        break;
			}
                case DW_OP::const4u:
                        {
			auto u32 = cur.fixed<uint32_t>();
			stack.push_back(u32);
			evaluation_string += "+ " + std::to_string(u32);
                        break;
			}
                case DW_OP::const8u:
			{
			auto u64 = cur.fixed<uint64_t>();
                        stack.push_back(u64);
			evaluation_string += "+ " + std::to_string(u64);
                        break;
			}
                case DW_OP::const1s:
			{
			auto i8 = cur.fixed<int8_t>();
                        stack.push_back(i8);
			evaluation_string += "+ " + std::to_string(i8);
                        break;
			}
                case DW_OP::const2s:
			{
			auto i16 = cur.fixed<int16_t>(); 
                        stack.push_back(i16);
			evaluation_string += "+ " + std::to_string(i16);
                        break;
			}
                case DW_OP::const4s:
			{
			auto i32 = cur.fixed<int32_t>();
                        stack.push_back(i32);
			evaluation_string += "+ " + std::to_string(i32);
                        break;
			}
                case DW_OP::const8s:
                        {
			auto i64 = cur.fixed<int64_t>();
			stack.push_back(i64);
			evaluation_string += "+ " + std::to_string(i64);
                        break;
			}
                case DW_OP::constu:
			{
			auto u128 = cur.uleb128();
                        stack.push_back(u128);
			evaluation_string += "+ " + std::to_string(u128);
                        break;
			}
                case DW_OP::consts:
			{
			auto s128 = cur.sleb128();
                        stack.push_back(s128);
			evaluation_string += "+ " + std::to_string(s128);
                        break;
			}

		// 2.5.1.2 Register based addressing
                case DW_OP::fbreg:
                {
			printf("FBREG from expr\n");
			auto frame_base_at = (*parent)[DW_AT::frame_base];
			expr_result frame_base{};
			frame_base.offset_from_value = 0;

			if (frame_base_at.get_type() == value::type::loclist) {
				auto loclist = frame_base_at.as_loclist();
				frame_base = loclist.evaluate(ctx, parent);
				result.value = frame_base.value;
				result.offset_from_value += frame_base.offset_from_value;
			}
			else if (frame_base_at.get_type() == value::type::exprloc) {
				auto expr = frame_base_at.as_exprloc();
				std::string tmp_str;
				frame_base = expr.evaluate(ctx, tmp_str, parent);
				evaluation_string += "+" + tmp_str;
				result.value = frame_base.value;
				result.offset_from_value += frame_base.offset_from_value;
			}
			if (frame_base.offset_from_value == INT64_MAX) {
				result.offset_from_value = INT64_MAX;
				return result;
			}
			result.location_type = frame_base.location_type;

			switch (frame_base.location_type) {
				case expr_result::type::reg:
					tmp1.u = (unsigned)frame_base.value;
					tmp2.s = cur.sleb128();
					result.offset_from_value += tmp2.s;
					//printf("Now I have a reg with off % " PRId64 "!\n", tmp2.s);
					
					// FIXME: properly handle the stack and the registers!
					stack.push_back((int64_t)ctx->reg(tmp1.u).offset + tmp2.s);
					evaluation_string += "r" + std::to_string(tmp1.u) + " + " + std::to_string(tmp2.s);
					break;
				case expr_result::type::address:
					tmp1.u = frame_base.value;
					tmp2.s = cur.sleb128();
					//printf("Now I have an addr with off %" PRId64 "! Prev fb: %" PRId64 "\n", tmp2.s, result.offset_from_value);
					result.offset_from_value += tmp2.s;
					stack.push_back(tmp1.u + tmp2.s);
					evaluation_string += "+ " + std::to_string(tmp1.u + tmp2.s);
					break;
				case expr_result::type::literal:
				case expr_result::type::implicit:
				case expr_result::type::empty:
					throw expr_error("Unhandled frame base type for DW_OP_fbreg");
				break;
			}
			break;
                }
                case DW_OP::breg0...DW_OP::breg31:
                        tmp1.u = (unsigned)op - (unsigned)DW_OP::breg0;
                        tmp2.s = cur.sleb128();
			// FIXME: properly handle the stack and the registers!
                        stack.push_back((int64_t)ctx->reg(tmp1.u).offset + tmp2.s);
			evaluation_string += "r" + std::to_string(tmp1.u) + " + " + std::to_string(tmp2.s);
                        break;
                case DW_OP::bregx:
                        tmp1.u = cur.uleb128();
                        tmp2.s = cur.sleb128();
			// FIXME: properly handle the stack and the registers!
                        stack.push_back((int64_t)ctx->reg(tmp1.u).offset + tmp2.s);
			evaluation_string += "r" + std::to_string(tmp1.u) + " + " + std::to_string(tmp2.s);
                        break;

                        // 2.5.1.3 Stack operations
                case DW_OP::dup:
                        CHECK();
                        stack.push_back(stack.back());
			evaluation_string += "dup_not_implemented";
                        break;
                case DW_OP::drop:
                        CHECK();
                        stack.pop_back();
			evaluation_string += "drop_not_implemented";
                        break;
                case DW_OP::pick:
                        tmp1.u = cur.fixed<uint8_t>();
                        CHECKN(tmp1.u);
                        stack.push_back(stack.revat(tmp1.u));
			evaluation_string += "pick_not_implemented";
                        break;
                case DW_OP::over:
                        CHECKN(2);
                        stack.push_back(stack.revat(1));
			evaluation_string += "over_not_implemented";
                        break;
                case DW_OP::swap:
                        CHECKN(2);
                        tmp1.u = stack.back();
                        stack.back() = stack.revat(1);
                        stack.revat(1) = tmp1.u;
			evaluation_string += "swap_not_implemented";
                        break;
                case DW_OP::rot:
                        CHECKN(3);
                        tmp1.u = stack.back();
                        stack.back() = stack.revat(1);
                        stack.revat(1) = stack.revat(2);
                        stack.revat(2) = tmp1.u;
			evaluation_string += "rot_not_implemented";
                        break;
                case DW_OP::deref:
                        tmp1.u = subsec->addr_size;
                        goto deref_common;
                case DW_OP::deref_size:
                        tmp1.u = cur.fixed<uint8_t>();
                        if (tmp1.u > subsec->addr_size)
                                throw expr_error("DW_OP_deref_size operand exceeds address size");
                deref_common:
                        CHECK();
                        stack.back() = ctx->deref_size(stack.back(), tmp1.u);
			evaluation_string += "deref_not_implemented";
                        break;
                case DW_OP::xderef:
                        tmp1.u = subsec->addr_size;
                        goto xderef_common;
                case DW_OP::xderef_size:
                        tmp1.u = cur.fixed<uint8_t>();
                        if (tmp1.u > subsec->addr_size)
                                throw expr_error("DW_OP_xderef_size operand exceeds address size");
                xderef_common:
                        CHECKN(2);
                        tmp2.u = stack.back();
                        stack.pop_back();
                        stack.back() = ctx->xderef_size(tmp2.u, stack.back(), tmp1.u);
			evaluation_string += "xderef_not_implemented";
                        break;
                case DW_OP::push_object_address:
                        // XXX
                        throw runtime_error("DW_OP_push_object_address not implemented");
                case DW_OP::form_tls_address:
                        CHECK();
                        stack.back() = ctx->form_tls_address(stack.back());
			evaluation_string += "from_tls_not_implemented";
                        break;
                case DW_OP::call_frame_cfa:
					{
					frame dbf(cu, 0, 0);
					// Check debug_frame first, then eh_frame 
					cfa_eval_result cfa = dbf.get_cfa(ctx);
					if (cfa.regnum == -1) {
						throw runtime_error("call_frame_cfa; could not evaluate CFA");
					}
					//printf("CFA: %d from REG %d\n", cfa.offset, cfa.regnum);
					
					result.location_type = expr_result::type::reg;
					result.value = cfa.regnum;	
					result.offset_from_value += cfa.offset;
					
					// TODO: fix this stack operation
					// FIXME: properly handle the stack and the registers!
					stack.push_back((int64_t)ctx->reg(7).offset);
					evaluation_string += "rsp + " + std::to_string(cfa.offset);
					break;
					}
                        // 2.5.1.4 Arithmetic and logical operations
#define UBINOP(binop)                                                   \
                        do {                                            \
                                CHECKN(2);                              \
                                tmp1.u = stack.back();                  \
                                stack.pop_back();                       \
                                tmp2.u = stack.back();                  \
                                stack.back() = tmp2.u binop tmp1.u;     \
                        } while (0)
                case DW_OP::abs:
                        CHECK();
                        tmp1.u = stack.back();
                        if (tmp1.s < 0)
                                tmp1.s = -tmp1.s;
                        stack.back() = tmp1.u;
                        break;
                case DW_OP::and_:
                        UBINOP(&);
                        break;
                case DW_OP::div:
                        CHECKN(2);
                        tmp1.u = stack.back();
                        stack.pop_back();
                        tmp2.u = stack.back();
                        tmp3.s = tmp1.s / tmp2.s;
                        stack.back() = tmp3.u;
                        break;
                case DW_OP::minus:
                        UBINOP(-);
                        break;
                case DW_OP::mod:
                        UBINOP(%);
                        break;
                case DW_OP::mul:
                        UBINOP(*);
                        break;
                case DW_OP::neg:
                        CHECK();
                        tmp1.u = stack.back();
                        tmp1.s = -tmp1.s;
                        stack.back() = tmp1.u;
                        break;
                case DW_OP::not_:
                        CHECK();
                        stack.back() = ~stack.back();
                        break;
                case DW_OP::or_:
                        UBINOP(|);
                        break;
                case DW_OP::plus:
                        UBINOP(+);
                        break;
                case DW_OP::plus_uconst:
                        tmp1.u = cur.uleb128();
                        CHECK();
                        stack.back() += tmp1.u;
                        break;
                case DW_OP::shl:
                        CHECKN(2);
                        tmp1.u = stack.back();
                        stack.pop_back();
                        tmp2.u = stack.back();
                        // C++ does not define what happens if you
                        // shift by more bits than the width of the
                        // type, so we handle this case specially
                        if (tmp1.u < sizeof(tmp2.u)*8)
                                stack.back() = tmp2.u << tmp1.u;
                        else
                                stack.back() = 0;
                        break;
                case DW_OP::shr:
                        CHECKN(2);
                        tmp1.u = stack.back();
                        stack.pop_back();
                        tmp2.u = stack.back();
                        // Same as above
                        if (tmp1.u < sizeof(tmp2.u)*8)
                                stack.back() = tmp2.u >> tmp1.u;
                        else
                                stack.back() = 0;
                        break;
                case DW_OP::shra:
                        CHECKN(2);
                        tmp1.u = stack.back();
                        stack.pop_back();
                        tmp2.u = stack.back();
                        // Shifting a negative number is
                        // implementation-defined in C++.
                        tmp3.u = (tmp2.s < 0);
                        if (tmp3.u)
                                tmp2.s = -tmp2.s;
                        if (tmp1.u < sizeof(tmp2.u)*8)
                                tmp2.u >>= tmp1.u;
                        else
                                tmp2.u = 0;
                        // DWARF implies that over-shifting a negative
                        // number should result in 0, not ~0.
                        if (tmp3.u)
                                tmp2.s = -tmp2.s;
                        stack.back() = tmp2.u;
                        break;
                case DW_OP::xor_:
                        UBINOP(^);
                        break;
#undef UBINOP

                        // 2.5.1.5 Control flow operations
#define SRELOP(relop)                                                   \
                        do {                                            \
                                CHECKN(2);                              \
                                tmp1.u = stack.back();                  \
                                stack.pop_back();                       \
                                tmp2.u = stack.back();                  \
                                stack.back() = (tmp2.s <= tmp1.s) ? 1 : 0; \
                        } while (0)
                case DW_OP::le:
                        SRELOP(<=);
                        break;
                case DW_OP::ge:
                        SRELOP(>=);
                        break;
                case DW_OP::eq:
                        SRELOP(==);
                        break;
                case DW_OP::lt:
                        SRELOP(<);
                        break;
                case DW_OP::gt:
                        SRELOP(>);
                        break;
                case DW_OP::ne:
                        SRELOP(!=);
                        break;
                case DW_OP::skip:
                        tmp1.s = cur.fixed<int16_t>();
                        goto skip_common;
                case DW_OP::bra:
                        tmp1.s = cur.fixed<int16_t>();
                        CHECK();
                        tmp2.u = stack.back();
                        stack.pop_back();
                        if (tmp2.u == 0)
                                break;
                skip_common:
                        cur = cursor(subsec, (int64_t)cur.get_section_offset() + tmp1.s);
                        break;
                case DW_OP::call2:
                case DW_OP::call4:
                case DW_OP::call_ref:
                        // XXX
                        throw runtime_error(to_string(op) + " not implemented");
#undef SRELOP

                        // 2.5.1.6 Special operations
                case DW_OP::nop:
                        break;

                        // 2.6.1.1.2 Register location descriptions
                case DW_OP::reg0...DW_OP::reg31:
                        result.location_type = expr_result::type::reg;
                        result.value = (unsigned)op - (unsigned)DW_OP::reg0;
                        break;
                case DW_OP::regx:
                        result.location_type = expr_result::type::reg;
                        result.value = cur.uleb128();
                        break;

                        // 2.6.1.1.3 Implicit location descriptions
                case DW_OP::implicit_value:
                        result.location_type = expr_result::type::implicit;
                        result.implicit_len = cur.uleb128();
                        cur.ensure(result.implicit_len);
                        result.implicit = cur.pos;
                        break;
                case DW_OP::stack_value:
                        CHECK();
                        result.location_type = expr_result::type::literal;
                        result.value = stack.back();
			evaluation_string += ".";
                        break;

                        // 2.6.1.2 Composite location descriptions
                case DW_OP::piece:
                        // XXX
			printf("WARN: pieces not implemented\n");
                        result.location_type = expr_result::type::empty;
                        result.value = 0;//stack.back();
                        // Move the cursor correctly
                        cur.uleb128(); 
						//throw runtime_error(to_string(op) + " not implemented");
                        break;
                case DW_OP::bit_piece:
                        // XXX
			printf("WARN: pieces not implemented\n");
                        result.location_type = expr_result::type::empty;
                        result.value = 0;//stack.back();
                        // Move the cursor correctly
			cur.uleb128(); 
                        cur.uleb128(); 
                        break;
						//throw runtime_error(to_string(op) + " not implemented");

                case DW_OP::lo_user...DW_OP::hi_user:
                        // XXX We could let the context evaluate this,
                        // but it would need access to the cursor.
			printf("WARN: pieces not implemented\n");
                        result.location_type = expr_result::type::empty;
                        result.value = 0;//stack.back();
			return result;
                        //throw expr_error("unknown user op " + to_string(op));

                default:
                        throw expr_error("bad operation " + to_string(op));
                }
#pragma GCC diagnostic pop
#undef CHECK
#undef CHECKN
        }

        if (result.location_type == expr_result::type::address) {
                // The result type is still an address, so we should
                // fetch it from the top of stack.
                if (stack.empty())
                        throw expr_error("final stack is empty; no result given");
                result.value = stack.back();
        }

        return result;

underflow:
        throw expr_error("stack underflow evaluating DWARF expression");
}

DWARFPP_END_NAMESPACE
