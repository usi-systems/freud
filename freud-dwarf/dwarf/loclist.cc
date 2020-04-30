// Copyright (c) 2013 Austin T. Clements. All rights reserved.
// Use of this source code is governed by an MIT license
// that can be found in the LICENSE file.

#include "internal.hh"

#include <map>
#include <iostream>

using namespace std;

DWARFPP_BEGIN_NAMESPACE
expr_result
loclist::evaluate(expr_context *ctx, const die * parent) const {
    cursor cur {cu->data(), offset};
    auto loc_offset = cur.fixed<uint32_t>();

    auto dwarf = cu->get_dwarf();
    auto debug_loc = dwarf.get_section(section_type::loc);

    cursor loc_cur {debug_loc, loc_offset};

    auto pc = ctx->pc() - at_low_pc(cu->root());
        
	// Prepare the expression result.  Some location descriptions
	// create the result directly, rather than using the top of
	// stack.
	expr_result result;
	result.offset_from_value = 0;

    while (true) {
        auto start_addr = loc_cur.fixed<uint64_t>();
        auto end_addr = loc_cur.fixed<uint64_t>();

        // End of list
        if (start_addr == 0 && end_addr == 0) {
            break;
        }
	size_t size = loc_cur.fixed<uint16_t>(); // FIXME: the size can be different; apparently this is not really DWARF4, since blocks are used instead of uleb128!
	if (pc < start_addr || pc > end_addr) {
		loc_cur.skip_bytes(size);
		continue;
	}
        small_vector<taddr, 8> stack;

        // Assume the result is an address for now and should be
        // grabbed from the top of stack at the end.
        result.location_type = expr_result::type::address;

	size_t addr_size = cu->data()->addr_size;
        // Execute!
	unsigned int read = 0;
        while (read < size) {
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
                DW_OP op = (DW_OP)loc_cur.fixed<ubyte>();
                read++;
				//printf("Readd %u\n", (unsigned)op);
				switch (op) {
                        // 2.5.1.1 Literal encodings
                case DW_OP::lit0...DW_OP::lit31:
                        stack.push_back((unsigned)op - (unsigned)DW_OP::lit0);
                        break;
                case DW_OP::addr:
                        stack.push_back(loc_cur.address());
                        break;
                case DW_OP::const1u:
                        stack.push_back(loc_cur.fixed<uint8_t>());
						read += 1;
                        break;
                case DW_OP::const2u:
                        stack.push_back(loc_cur.fixed<uint16_t>());
						read += 2;
                        break;
                case DW_OP::const4u:
                        stack.push_back(loc_cur.fixed<uint32_t>());
						read += 4;
                        break;
                case DW_OP::const8u:
                        stack.push_back(loc_cur.fixed<uint64_t>());
						read += 8;
                        break;
                case DW_OP::const1s:
                        stack.push_back(loc_cur.fixed<int8_t>());
						read += 1;
                        break;
                case DW_OP::const2s:
                        stack.push_back(loc_cur.fixed<int16_t>());
						read += 2;
                        break;
                case DW_OP::const4s:
                        stack.push_back(loc_cur.fixed<int32_t>());
						read += 4;
                        break;
                case DW_OP::const8s:
                        stack.push_back(loc_cur.fixed<int64_t>());
						read += 8;
                        break;
                case DW_OP::constu:
                        stack.push_back(loc_cur.uleb128(read));
                        break;
                case DW_OP::consts:
                        stack.push_back(loc_cur.sleb128(read));
                        break;

                        // 2.5.1.2 Register based addressing
                case DW_OP::fbreg:
                {
                        bool found = false;
						auto frame_base_at = (*parent)[DW_AT::frame_base];
						expr_result frame_base{};
						frame_base.offset_from_value = 0;
						if (frame_base_at.get_type() == value::type::loclist) {
							auto loclist = frame_base_at.as_loclist();
							frame_base = loclist.evaluate(ctx, parent);
						}
						else if (frame_base_at.get_type() == value::type::exprloc) {
							auto expr = frame_base_at.as_exprloc();
							std::string tmp_str;
							frame_base = expr.evaluate(ctx, tmp_str, parent);
						}

						switch (frame_base.location_type) {
						case expr_result::type::reg:
							tmp1.u = (unsigned)frame_base.value;
							tmp2.s = loc_cur.sleb128(read);
							// FIXME: properly handle the stack and the registers!
							stack.push_back((int64_t)ctx->reg(tmp1.u).offset + tmp2.s);
							found = true;
							break;
						case expr_result::type::address:
							tmp1.u = frame_base.value;
							tmp2.s = loc_cur.sleb128(read);
							stack.push_back(tmp1.u + tmp2.s);
							found = true;
							break;
						case expr_result::type::literal:
						case expr_result::type::implicit:
						case expr_result::type::empty:
							throw expr_error("Unhandled frame base type for DW_OP_fbreg");
						}
					if (found) break;
                    break;
                }
                case DW_OP::breg0...DW_OP::breg31:
                        tmp1.u = (unsigned)op - (unsigned)DW_OP::breg0;
                        tmp2.s = loc_cur.sleb128(read);
			// FIXME: properly handle the stack and the registers!
			stack.push_back((int64_t)ctx->reg(tmp1.u).offset + tmp2.s);
                        break;
                case DW_OP::bregx:
                        tmp1.u = loc_cur.uleb128(read);
                        tmp2.s = loc_cur.sleb128(read);
			// FIXME: properly handle the stack and the registers!
                        stack.push_back((int64_t)ctx->reg(tmp1.u).offset + tmp2.s);
                        break;

                        // 2.5.1.3 Stack operations
                case DW_OP::dup:
                        CHECK();
                        stack.push_back(stack.back());
                        break;
                case DW_OP::drop:
                        CHECK();
                        stack.pop_back();
                        break;
                case DW_OP::pick:
                        tmp1.u = loc_cur.fixed<uint8_t>();
						read += 1;
                        CHECKN(tmp1.u);
                        stack.push_back(stack.revat(tmp1.u));
                        break;
                case DW_OP::over:
                        CHECKN(2);
                        stack.push_back(stack.revat(1));
                        break;
                case DW_OP::swap:
                        CHECKN(2);
                        tmp1.u = stack.back();
                        stack.back() = stack.revat(1);
                        stack.revat(1) = tmp1.u;
                        break;
                case DW_OP::rot:
                        CHECKN(3);
                        tmp1.u = stack.back();
                        stack.back() = stack.revat(1);
                        stack.revat(1) = stack.revat(2);
                        stack.revat(2) = tmp1.u;
                        break;
                case DW_OP::deref:
                        tmp1.u = addr_size;
                        goto deref_common;
                case DW_OP::deref_size:
                        tmp1.u = loc_cur.fixed<uint8_t>();
						read += 1;
                        //printf("DS: %lu, AS: %lu\n", tmp1.u, addr_size);
			if (tmp1.u > addr_size)
				throw expr_error("DW_OP_deref_size operand exceeds address size");
                deref_common:
                        CHECK();
                        stack.back() = ctx->deref_size(stack.back(), tmp1.u);
                        break;
                case DW_OP::xderef:
                        tmp1.u = addr_size;
                        goto xderef_common;
                case DW_OP::xderef_size:
                        tmp1.u = loc_cur.fixed<uint8_t>();
						read += 1;
                        if (tmp1.u > addr_size)
                                throw expr_error("DW_OP_xderef_size operand exceeds address size");
                xderef_common:
                        CHECKN(2);
                        tmp2.u = stack.back();
                        stack.pop_back();
                        stack.back() = ctx->xderef_size(tmp2.u, stack.back(), tmp1.u);
                        break;
                case DW_OP::push_object_address:
                        // XXX
                        throw runtime_error("DW_OP_push_object_address not implemented");
                case DW_OP::form_tls_address:
                        CHECK();
                        stack.back() = ctx->form_tls_address(stack.back());
                        break;
                case DW_OP::call_frame_cfa:
                        // Horrible hack which just reads the frame pointer on x86
                        printf("FIXME: CFA from loclist!\n");
			tmp1.u = 6;
			// FIXME: properly handle the stack and the registers!
                        stack.push_back((int64_t)ctx->reg(tmp1.u).offset);
                        break;
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
                        tmp1.u = loc_cur.uleb128(read);
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
                        tmp1.s = loc_cur.fixed<int16_t>();
						read += 2;
                        goto skip_common;
                case DW_OP::bra:
                        tmp1.s = loc_cur.fixed<int16_t>();
						read += 2;
                        CHECK();
                        tmp2.u = stack.back();
                        stack.pop_back();
                        if (tmp2.u == 0)
                                break;
                skip_common:
                        //cur = cursor(subsec, (int64_t)cur.get_section_offset() + tmp1.s);
                        loc_cur.skip_bytes(tmp1.s);
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
                        result.value = loc_cur.uleb128(read);
                        break;

                        // 2.6.1.1.3 Implicit location descriptions
                case DW_OP::implicit_value:
                        result.location_type = expr_result::type::implicit;
                        result.implicit_len = loc_cur.uleb128(read);
                        loc_cur.ensure(result.implicit_len);
                        result.implicit = loc_cur.pos;
                        break;
                case DW_OP::stack_value:
                        CHECK();
                        result.location_type = expr_result::type::literal;
                        result.value = stack.back();
                        break;

                        // 2.6.1.2 Composite location descriptions
                case DW_OP::piece:
                        // XXX
			printf("WARN: pieces not implemented\n");
                        result.location_type = expr_result::type::empty;
                        result.value = 0;//stack.back();
                        // Move the cursor correctly
                        loc_cur.uleb128(read); 
						break;
						//throw runtime_error(to_string(op) + " not implemented");

                case DW_OP::bit_piece:
                        // XXX
			printf("WARN: pieces not implemented\n");
                        result.location_type = expr_result::type::empty;
                        result.value = 0;//stack.back();
                        // Move the cursor correctly
						loc_cur.uleb128(read); 
                        loc_cur.uleb128(read); 
						break;
						//throw runtime_error(to_string(op) + " not implemented");

                case DW_OP::lo_user...DW_OP::hi_user:
                        // XXX We could let the context evaluate this,
                        // but it would need access to the cursor.
			printf("WARN: user operations not implemented\n");
                        result.location_type = expr_result::type::empty;
                        result.value = 0;//stack.back();
			return result;
                        //throw expr_error("unknown user op " + to_string(op));

                //default:
                //        throw expr_error("bad operation " + to_string(op));
                }
#pragma GCC diagnostic pop
#undef CHECK
#undef CHECKN
        }

        if (result.location_type == expr_result::type::address) {
                // The result type is still and address, so we should
                // fetch it from the top of stack.
		if (stack.empty())
                        throw expr_error("final stack is empty; no result given");
                result.value = stack.back();
        }
		return result;
    }
	printf("WARN: the given PC was not found in this loclist\n");
	result.location_type = expr_result::type::empty;
	result.value = 0;
	return result;

underflow:
        throw expr_error("stack underflow evaluating DWARF expression in loclist");
}

loclist::loclist(const unit *cu,
                 section_offset offset)
        : cu(cu), offset(offset)
{
}
DWARFPP_END_NAMESPACE
