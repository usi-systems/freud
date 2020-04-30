// Copyright (c) 2019 Daniele Rogora. All rights reserved.
// Use of this source code is governed by an MIT license
// that can be found in the LICENSE file.

#include "internal.hh"
#include "inttypes.h"

using namespace std;

DWARFPP_BEGIN_NAMESPACE

// From the gcc source code: gcc/dwarf2asm.c
int
size_of_uleb128 (uint64_t value)
{
  int size = 0;

  do
    {
      value >>= 7;
      size += 1;
    }
  while (value != 0);

  return size;
}

/* Return the size of a signed LEB128 quantity.  */
int
size_of_sleb128 (int64_t value)
{
  int size = 0, byte;

  do
    {
      byte = (value & 0x7f);
      value >>= 7;
      size += 1;
    }
  while (!((value == 0 && (byte & 0x40) == 0)
           || (value == -1 && (byte & 0x40) != 0)));

  return size;
}

class cie {
public:
	cie() {
		// An empty CIE, should never be used
		address_size = 0; //  Zero size triggers an error if used for real
		segment_size = 0;
	};
	cie(cursor cur, uint64_t len, uint64_t off, format fmt, expr_context *ctx, bool from_eh): length(len), offset(off), context(ctx) {
		version = cur.fixed<ubyte>();
		cur.string(augmentation);
		augmentation_length = 0;
 
		if (!from_eh) {
			// DWARF debug_frame SECTION
			// FIXME: the two following fields are missing with gcc
			// with -g -gstrict-dwarf -gdwarf-4, although they should be
			// present according to the DWARF4 standard!
			//address_size = cur.fixed<ubyte>();
			//segment_size = cur.fixed<ubyte>();
			code_alignment_factor = cur.uleb128();
			data_alignment_factor = cur.sleb128();
			return_address_register = cur.uleb128();

			// Not present in debug_frame?
			fde_encoding = lsda_encoding = personality_encoding = personality_address = return_address_register = 0; 
		} else {
			// GCC eh_frame SECTION
			// as documented in 
			// (a) - http://refspecs.linuxbase.org/LSB_3.0.0/LSB-PDA/LSB-PDA/ehframechpt.html#EHFRAME
			// (c) - http://refspecs.linuxbase.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/dwarfext.html#DWARFEHENCODING
			// (b) - https://www.airs.com/blog/archives/460
			if (augmentation == "eh") {
				if (fmt == format::dwarf32) {
					// uint32_t undoc_value = 
					cur.fixed<uword>();
				}
				else if (fmt == format::dwarf64) {
					// uint64_t undoc_value = 
					cur.fixed<uint64_t>();
				}
			}
			code_alignment_factor = cur.uleb128();
			data_alignment_factor = cur.sleb128();
			// Return address register
			// * NOT DOCUMENTED IN SOURCE (a)
			switch (version) {
				case 1:
					return_address_register = cur.fixed<ubyte>();
					break;

				case 3:
					return_address_register = cur.uleb128();
					break;

				default:
					throw expr_error("Unsupported CIE version");
					break;			
			}
			if (augmentation.size() && augmentation[0] == 'z') {
				augmentation_length = cur.uleb128();
				uint32_t used_bytes = 0;
				for (unsigned int i = 1; i < augmentation.size(); i++) {
					switch (augmentation[i]) {
						case 'L':
						   lsda_encoding = cur.fixed<uint8_t>();
						   used_bytes++;
						   break;
							
						case 'P':
						{
						   personality_encoding = cur.fixed<uint8_t>();
						   uint8_t encoding = personality_encoding & 0x0F;
						   //uint8_t modifier = personality_encoding & 0x70;
						   used_bytes++;
						   switch ((DW_EH_PE)encoding) {
							case DW_EH_PE::absptr:
						   		personality_address = cur.address();
								used_bytes += cur.sec->addr_size;
								break;

							case DW_EH_PE::uleb128: 
						   		personality_address = cur.uleb128();
								used_bytes += size_of_uleb128(personality_address);
								break;

							case DW_EH_PE::sleb128: 
						   		personality_address = cur.sleb128();
								used_bytes += size_of_sleb128(personality_address);
								break;

							case DW_EH_PE::udata2: 
						   		personality_address = cur.fixed<uint16_t>();
								used_bytes += 2;
								break;

							case DW_EH_PE::udata4: 
						   		personality_address = cur.fixed<uint32_t>();
								used_bytes += 4;
								break;

							case DW_EH_PE::udata8: 
						   		personality_address = cur.fixed<uint64_t>();
								used_bytes += 8;
								break;

							case DW_EH_PE::sdata2: 
						   		personality_address = cur.fixed<int16_t>();
								used_bytes += 2;
								break;

							case DW_EH_PE::sdata4: 
						   		personality_address = cur.fixed<int32_t>();
								used_bytes += 4;
								break;

							case DW_EH_PE::sdata8: 
						   		personality_address = cur.fixed<int64_t>();
								used_bytes += 8;
								break;
						        
							default:
								throw expr_error("Unhandled augmentation data string P");
						   		break;
						   }
						   break;
						}
						case 'R':
						   fde_encoding = cur.fixed<uint8_t>();
						   used_bytes++;
						   break;

						case 'S':
						default:
						   throw expr_error("Unhandled augmentation data string S");
						
					}
				}
				cur.skip_bytes(augmentation_length - used_bytes);
			}
		}
		// Save a pointer to the beginning of the DW_CFA_OP section
		cfa_cur = cur;
		
		// printf("CFA Read CIE with offset %lx, version %u, augm %s, addr_size %u, seg_size %u, caf %lu, daf %ld, rar %lu, rsp_off = %d, auglen = %lu, fdeenc = %u\n", offset, version, augmentation.c_str(), address_size, segment_size, code_alignment_factor, data_alignment_factor, return_address_register, rsp_offset, augmentation_length, fde_encoding);
	};

	void process_cfa() {
		// When evaluating a CIE, I do not expect any advance_loc instruction
		// So, passing 0 should be harmless
		frame::evaluate_cfa(cfa_cur, length, context, 0ul);
	}

	cursor cfa_cur;
	uint64_t length;
	uint8_t address_size;
	uint8_t segment_size;
	int64_t rsp_offset; 
	uint8_t fde_encoding;
	uint64_t augmentation_length;

private:
	uint64_t offset;
	uint8_t version;
	string augmentation;
	uint64_t code_alignment_factor;
	int64_t data_alignment_factor;
	uint64_t return_address_register;
	uint8_t lsda_encoding;
	uint8_t personality_encoding;
	uint64_t personality_address;
	// I want to store this value to defer the evaluation to a later time
	// since I might not be interested in this FDE
	uint64_t initial_instruction_offset;
	expr_context *context;
};

// TODO: check the real CIE parent of the FDE. It's not necessarily the last one seen
// before the FDE, according to the documentation. 
// Never found a counterexample with GCC, though...
class fde {
public:
	fde(cursor cur, uint64_t len, uint64_t cie_ptr, const cie * parent_cie, uint64_t offset, expr_context *ctx, bool from_eh): length(len), cie_pointer(cie_ptr), context(ctx) {
		if (!from_eh) {
			if (parent_cie->segment_size > 0) {
				// parse the segment selector
				// cur.skip_bytes(parent_cie->segment_size);
			}
			unsigned_initial_location = cur.address();
			unsigned_address_range = cur.address();
			signed_encoding = false;
		} else {
			uint8_t encoding = parent_cie->fde_encoding & 0x0F;
			uint8_t modifier = parent_cie->fde_encoding & 0x70;
			signed_encoding = false;
			switch ((DW_EH_PE)encoding) {
				case DW_EH_PE::absptr:
					// Architecture dependent; should be ok
					unsigned_initial_location = cur.address();
					unsigned_address_range = cur.address();
					offset += 2 * cur.sec->addr_size;
					break;

				case DW_EH_PE::uleb128:
					unsigned_initial_location = cur.uleb128();
					unsigned_address_range = cur.uleb128();
					offset += size_of_uleb128(unsigned_initial_location) + size_of_uleb128(unsigned_address_range);
					break;

				case DW_EH_PE::udata2:
					unsigned_initial_location = cur.fixed<uint16_t>();
					unsigned_address_range = cur.fixed<uint16_t>();
					offset += 4;
					break;

				case DW_EH_PE::udata4:
					unsigned_initial_location = cur.fixed<uint32_t>();
					unsigned_address_range = cur.fixed<uint32_t>();
					offset += 8;
					break;

				case DW_EH_PE::udata8:
					unsigned_initial_location = cur.fixed<uint64_t>();
					unsigned_address_range = cur.fixed<uint64_t>();
					offset += 16;
					break;

				case DW_EH_PE::sleb128:
					signed_initial_location = cur.sleb128();
					signed_address_range = cur.sleb128();
					signed_encoding = true;
					offset += size_of_sleb128(signed_initial_location) + size_of_sleb128(signed_address_range);
					break;

				case DW_EH_PE::sdata2:
					signed_initial_location = cur.fixed<int16_t>();
					signed_address_range = cur.fixed<int16_t>();
					signed_encoding = true;
					offset += 4;
					break;

				case DW_EH_PE::sdata4:
					signed_initial_location = cur.fixed<int32_t>();
					signed_address_range = cur.fixed<int32_t>();
					signed_encoding = true;
					offset += 8;
					break;

				case DW_EH_PE::sdata8:
					signed_initial_location = cur.fixed<int64_t>();
					signed_address_range = cur.fixed<int64_t>();
					signed_encoding = true;
					offset += 16;
					break;

				default:
					throw expr_error("Unhandled fde encoding");
					signed_initial_location = signed_address_range = unsigned_initial_location = unsigned_address_range = 0;
					break;
			}
			switch((DW_EH_PE)modifier) {
				case DW_EH_PE::pcrel:
					// I need to get the "current pc"
					// Which is the exact offset in the binary?
					if (signed_encoding) {
						final_initial_location = offset + signed_initial_location;
						final_ending_location = final_initial_location + signed_address_range;
					} else {
						final_initial_location = offset + unsigned_initial_location;
						final_ending_location = final_initial_location + unsigned_address_range;
					}
					break;

				case DW_EH_PE::textrel:
					printf("Textrel\n");
					throw expr_error("Unhandled fde modifier");
					break;

				case DW_EH_PE::datarel:
					printf("Datarel\n");
					throw expr_error("Unhandled fde modifier");
					break;

				case DW_EH_PE::funcrel:
					printf("Funcrel\n");
					throw expr_error("Unhandled fde modifier");
					break;
				
				case DW_EH_PE::aligned:
					printf("Aligned\n");
					throw expr_error("Unhandled fde modifier");
					break;

				default:
					throw expr_error("Unhandled fde modifier");
					break;
			}		
		}
		if (parent_cie->augmentation_length > 0) {
			uint64_t auglen = cur.uleb128();
			// Ignore this augmentation data
			cur.skip_bytes(auglen);
		}
		cfa_cursor = cur;
		//printf("FDE which points to %lx, addr %lx to %lx\n", cie_pointer, final_initial_location, final_ending_location);
	};

	bool surrounds_pc(uint64_t pc) {
		//printf("%lx surrounded by (%lx, %lx)\n", pc, initial_location, initial_location + address_range);
		bool res = ((pc >= final_initial_location) && (pc < final_ending_location));
		return res;
	}

	void process_cfa() {
		frame::evaluate_cfa(cfa_cursor, length, context, final_initial_location);
	} 

	cursor cfa_cursor;
	uint64_t length;
	int64_t signed_initial_location;
	uint64_t unsigned_initial_location;
	uint64_t unsigned_address_range;
	int64_t signed_address_range;
	uint64_t final_initial_location;
	uint64_t final_ending_location;
	bool signed_encoding;
private:
	uint64_t cie_pointer;
	expr_context *context;
};

frame::frame(const unit *cu,
           section_offset offset, section_length len)
        : cu(cu), offset(offset), len(len)
{
}

struct cfa_eval_result frame::evaluate_cfa(cursor cur, uint64_t length, expr_context * ctx, const uint64_t initial_offset) {
	struct cfa_eval_result res;
	res.regnum = res.offset = 0;
	uint64_t current_loc = initial_offset;
	while (cur.get_section_offset() < length) {
		DW_CFA cfa_op = (DW_CFA)cur.fixed<ubyte>();
		printf("Read CFA op %u\n", (unsigned)cfa_op);
		if (cfa_op >= DW_CFA::advance_loc) {
			// DWARF4, section 7.23, Fig. 40
			// Let's retrieve the 6 least significant bits
			ubyte argument = (ubyte)cfa_op;
			argument = argument << 2;
			argument = argument >> 2;

			ubyte cfa_primary_op = (ubyte)cfa_op;
			cfa_primary_op = cfa_primary_op >> 6;
			cfa_primary_op = cfa_primary_op << 6;

			switch ((DW_CFA)cfa_primary_op) {
				case DW_CFA::nop:
					// this is not a primary-op, just break!
					break;
				
				case DW_CFA::offset:
					ctx->add_reg_offset((unsigned)argument, cur.uleb128());
					break;

				case DW_CFA::advance_loc:
					{
					current_loc += (unsigned)argument;
					printf("Advancing loc; incr: %x, curr: %" PRIx64 ", pc: %" PRIx64 "\n", argument, current_loc, ctx->pc());
					if (current_loc > ctx->pc()) {
						res.regnum = ctx->get_cfa_reg();
						res.offset = ctx->get_cfa_offset(); 
						return res;
					}
					break;
					}

				case DW_CFA::restore:
					printf("TODO: implement cfa_primary_op!\n");
					break;
			
				case DW_CFA::set_loc:
				case DW_CFA::advance_loc1:
				case DW_CFA::advance_loc2:
				case DW_CFA::advance_loc4:
				case DW_CFA::offset_extended:
				case DW_CFA::restore_extended:
				case DW_CFA::undefined:
				case DW_CFA::same_value:
				case DW_CFA::_register:
				case DW_CFA::remember_state:
				case DW_CFA::restore_state:
				case DW_CFA::def_cfa:
				case DW_CFA::def_cfa_register:
				case DW_CFA::def_cfa_offset:
				case DW_CFA::def_cfa_expression:
				case DW_CFA::expression:
				case DW_CFA::offset_extended_sf:
				case DW_CFA::def_cfa_sf:
				case DW_CFA::def_cfa_offset_sf:
				case DW_CFA::val_offset:
				case DW_CFA::val_offset_sf:
				case DW_CFA::val_expression:
				case DW_CFA::lo_user:
				case DW_CFA::hi_user:
					throw expr_error("Error, this point should not be reachable!");
					break; 
			}
		} else {
			switch (cfa_op) {
				case DW_CFA::nop:
					// Nothing to do
					break;

				case DW_CFA::set_loc:
					printf("TODO: implement set_loc!\n");
					break;

				case DW_CFA::advance_loc1:
					{
						uint8_t adv = cur.fixed<ubyte>();
						current_loc += (unsigned)adv;
						if (current_loc > ctx->pc()) {
							res.regnum = ctx->get_cfa_reg();
							res.offset = ctx->get_cfa_offset(); 
							return res;
						}
						break;
					}
	
				case DW_CFA::advance_loc2:
					{
						uint16_t adv = cur.fixed<uint16_t>();
						current_loc += (unsigned)adv;
						if (current_loc > ctx->pc()) {
							res.regnum = ctx->get_cfa_reg();
							res.offset = ctx->get_cfa_offset(); 
							return res;
						}
						break;
					}
	
				case DW_CFA::advance_loc4:
					{
						uint32_t adv = cur.fixed<uint32_t>();
						printf("Advancing loc; incr: %x, curr: %" PRIx64 ", pc: %" PRIx64 "\n", adv, current_loc, ctx->pc());
						current_loc += (unsigned)adv;
						if (current_loc > ctx->pc()) {
							res.regnum = ctx->get_cfa_reg();
							res.offset = ctx->get_cfa_offset(); 
							return res;
						}
						break;
					}

				case DW_CFA::offset_extended:
				case DW_CFA::restore_extended:
				case DW_CFA::undefined:
				case DW_CFA::same_value:
				case DW_CFA::_register:
				case DW_CFA::remember_state:
				case DW_CFA::restore_state:
					printf("Unhandled DW_CFA operation: %u\n", (uint32_t)cfa_op);
					//throw expr_error("Unhandled DW_CFA operation");
					break;
 
				case DW_CFA::def_cfa:
					{
						uint8_t reg = cur.uleb128();
						uint64_t off = cur.uleb128();
						ctx->set_cfa(reg, off);
						//printf("Found cfa from rsp: %u\n", res.rsp);
						break;
					}

				case DW_CFA::def_cfa_register:
					ctx->set_cfa_reg(cur.uleb128());
					break;
	
				case DW_CFA::def_cfa_offset:
					ctx->set_cfa_offset(cur.uleb128());
					break;
	
				case DW_CFA::def_cfa_expression:
				case DW_CFA::expression:
				case DW_CFA::offset_extended_sf:
				case DW_CFA::def_cfa_sf:
				case DW_CFA::def_cfa_offset_sf:
				case DW_CFA::val_offset:
				case DW_CFA::val_offset_sf:
				case DW_CFA::val_expression:
				case DW_CFA::lo_user:
				case DW_CFA::hi_user:
					printf("Unhandled DW_CFA operation: %u\n", (uint32_t)cfa_op);
					//throw expr_error("Unhandled DW_CFA operation");
					break;
 
				case DW_CFA::advance_loc:
				case DW_CFA::offset:
				case DW_CFA::restore:
					throw expr_error("Error, this point should not be reachable!");
					break; 
			
			}
		}
	}
	return res;
}

cfa_eval_result
frame::get_cfa(expr_context *ctx) const
{
	// TODO: if debug_frame does not exist, program throws an exception
    auto dwarf = cu->get_dwarf();
    std::shared_ptr<section> debug_frame;
	try {
    	debug_frame = dwarf.get_section(section_type::frame);
	} catch (const format_error&) {
		printf("No debug frame?\n");
		return get_cfa_from_eh(ctx);
	}
    // debug_frames does not have any header: Section 7.2.2
    section_offset offset = 0;
    cursor frame_cur {debug_frame, offset};

    cie last_cie;

    uint64_t my_pc = ctx->pc();

    offset = frame_cur.get_section_offset();
    // The next subsection is either a CIE or a FDE
    while (offset < debug_frame->size()) {
        std::shared_ptr<section> subsec = frame_cur.subsection();
        // printf("CFA Read section with size %lu, type %u; pc=%lx; off %lu/%lu\n", subsec->size(), (subsec->fmt == format::dwarf32) ? 0 : 1, my_pc, offset, debug_frame->size());
        cursor subs_cur(subsec);
        subs_cur.skip_initial_length();
        uint64_t fde_or_cie = subs_cur.read_4_or_8_bytes_field();
        
        // Section 7.23, DWARFv3
        if ((subsec->fmt == format::dwarf32 && fde_or_cie == 0xffffffff)
    	|| (subsec->fmt == format::dwarf64 && fde_or_cie == 0xffffffffffffffff)) {
    	    // This is a CIE
	    // printf("Reading a CIE, size %lu\n", subsec->size());
    	    cie cie_entry(subs_cur, subsec->size(), offset, subsec->fmt, ctx, false);
    	    last_cie = cie_entry;
        } else {
    	    // This is a FDE
	    // printf("Reading a FDE, size %lu\n", subsec->size());
    	    subsec->addr_size = 8; //last_cie.address_size;
    	    fde fde_entry(subs_cur, subsec->size(), fde_or_cie, &last_cie, my_pc, ctx, false);
    	    if (fde_entry.surrounds_pc(my_pc)) {
		printf("Processing CIE cfa\n");
		last_cie.process_cfa();
		printf("Processing FDE cfa\n");
		fde_entry.process_cfa();
    		return ctx->get_cfa();
	    }
	// XXX parse the actual instructions in the FDE	
        }
        offset = frame_cur.get_section_offset();
    }
    printf("Could not find FDE for pc %" PRIx64 " in debug_frame\n", my_pc);
    return get_cfa_from_eh(ctx);
}

cfa_eval_result
frame::get_cfa_from_eh(expr_context *ctx) const 
{
    auto dwarf = cu->get_dwarf();
    std::shared_ptr<section> eh_frame = dwarf.get_section(section_type::eh_frame);
    // eh_frames header
    // https://refspecs.linuxfoundation.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
    section_offset offset = 0;
    cursor frame_cur {eh_frame, offset};

    cie last_cie;

    uint64_t my_pc = ctx->pc();

    offset = frame_cur.get_section_offset();

    while (offset < eh_frame->size()) {
        // CFI
        std::shared_ptr<section> subsec = frame_cur.subsection();
        // printf("CFI Read section with size %lu, type %u; pc=%lx; off %lu/%lu\n", subsec->size(), (subsec->fmt == format::dwarf32) ? 0 : 1, my_pc, offset, eh_frame->size());
        cursor subs_cur(subsec);
        subs_cur.skip_initial_length();
        // CII ID or FDE ptr
        uint32_t cieid_or_fdeptr = subs_cur.fixed<uword>();
	if (cieid_or_fdeptr == 0) {
	    // CIE
	    // printf("Reading a ehCIE, size %lu\n", subsec->size());
    	    cie cie_entry(subs_cur, subsec->size(), offset, subsec->fmt, ctx, true);
    	    last_cie = cie_entry;
	} else {
            // FDE
	    // printf("Reading a ehFDE, size %lu; offset %lu\n", subsec->size(), frame_cur.get_section_offset());
    	    subsec->addr_size = 8;//last_cie.address_size;
    	    fde fde_entry(subs_cur, subsec->size(), cieid_or_fdeptr, &last_cie, eh_frame->sec_offset + offset, ctx, true);
    	    if (fde_entry.surrounds_pc(my_pc)) {
		printf("Processing CIE cfa\n");
		last_cie.process_cfa();
		printf("Processing FDE cfa\n");
		fde_entry.process_cfa();
    		return ctx->get_cfa();
	    }
	}
        offset = frame_cur.get_section_offset();
    }
    cfa_eval_result no_res;
    no_res.regnum = -1;
    return no_res;
}

DWARFPP_END_NAMESPACE
