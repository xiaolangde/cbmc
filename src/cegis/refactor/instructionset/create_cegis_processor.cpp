#include <ansi-c/c_types.h>
#include <util/arith_tools.h>
#include <linking/zero_initializer.h>

#include <cegis/instrument/literals.h>
#include <cegis/instrument/meta_variables.h>
#include <cegis/instrument/instrument_var_ops.h>
#include <cegis/cegis-util/program_helper.h>
#include <cegis/cegis-util/type_helper.h>
#include <cegis/refactor/instructionset/processor_types.h>
#include <cegis/refactor/instructionset/cegis_processor_body_factory.h>
#include <cegis/refactor/instructionset/create_cegis_processor.h>

// XXX: Debug
#include <iostream>

#include <cegis/refactor/instructionset/execute_cegis_program.h>
// XXX: Debug

namespace
{
class type_collectort: public const_expr_visitort
{
public:
  std::set<typet> types;

  virtual ~type_collectort()=default;

  virtual void operator()(const exprt &expr)
  {
    types.insert(expr.type());
  }
};
}

std::set<typet> collect_context_types(const goto_ranget &range)
{
  type_collectort collector;
  for (goto_programt::const_targett it(range.first); it != range.second; ++it)
    it->code.visit(collector);
  return collector.types;
}

cegis_operand_datat slots_per_type(const symbol_tablet &st,
    const std::set<irep_idt> &state_vars)
{
  const namespacet ns(st);
  cegis_operand_datat result;
  for (const irep_idt &state_var : state_vars)
    ++result[ns.follow(st.lookup(state_var).type)];
  return result;
}

#define MAX_PROCESSORS 128u
#define CEGIS_PROCESSOR_FUNCTION_PREFIX CEGIS_PREFIX "processor_"
#define INSTR_TYPE_SUFFIX "_instructiont"

namespace
{
symbol_exprt get_variable_array_symbol(const symbol_tablet &st,
    const typet &type)
{
  return st.lookup(cegis_operand_array_name(st, type)).symbol_expr();
}

void create_variable_array(symbol_tablet &st, goto_functionst &gf,
    const typet &type, const size_t size)
{
  const std::string name(cegis_operand_array_name(st, type));
  if (st.has_symbol(name)) return;
  const typet size_type(signed_int_type());
  const constant_exprt sz_expr(from_integer(size, size_type));
  const array_typet array_type(pointer_typet(type), sz_expr);
  symbolt new_symbol;
  new_symbol.name=name;
  new_symbol.type=array_type;
  new_symbol.base_name=name;
  new_symbol.pretty_name=name;
  new_symbol.location=default_cegis_source_location();
  new_symbol.mode=ID_C;
  new_symbol.module=CEGIS_MODULE;
  new_symbol.is_static_lifetime=true;
  new_symbol.is_lvalue=true;
  assert(!st.add(new_symbol));
  goto_programt &body=get_body(gf, CPROVER_INIT);
  goto_programt::targett pos=body.instructions.begin();
  pos=body.insert_after(pos);
  pos->type=goto_program_instruction_typet::ASSIGN;
  pos->source_location=new_symbol.location;
  const symbol_exprt lhs(st.lookup(name).symbol_expr());
  const namespacet ns(st);
  null_message_handlert msg;
  const exprt rhs(zero_initializer(array_type, new_symbol.location, ns, msg));
  pos->code=code_assignt(lhs, rhs);
  body.update();
}

std::string get_next_processor_name(const symbol_tablet &st)
{
  std::string name(CEGIS_PROCESSOR_FUNCTION_PREFIX);
  for (size_t index=0; index < MAX_PROCESSORS; ++index)
  {
    name+=std::to_string(index);
    if (!st.has_symbol(name)) return name;
    else name= CEGIS_PROCESSOR_FUNCTION_PREFIX;
  }
  assert(!"Exceeded maximum number of CEGIS processors.");
  return "";
}

symbol_typet create_instruction_type(symbol_tablet &st,
    const cegis_operand_datat &variable_slots_per_context_type,
    const std::string &func_name)
{
  std::string instr_type_name(func_name);
  instr_type_name+= INSTR_TYPE_SUFFIX;
  if (st.has_symbol(instr_type_name)) return symbol_typet(instr_type_name);
  struct_typet type;
  std::string tag(TAG_PREFIX);
  tag+=instr_type_name;
  type.set_tag(tag);
  struct_union_typet::componentst &comps=type.components();
  const typet opcode_type(cegis_opcode_type());
  const std::string member_name(CEGIS_PROC_OPCODE_MEMBER_NAME);
  comps.push_back(struct_typet::componentt(member_name, opcode_type));
  const size_t max_operands=cegis_max_operands(variable_slots_per_context_type);
  const typet op_type(cegis_operand_type());
  for (size_t i=0; i < max_operands; ++i)
  {
    struct_union_typet::componentt comp(cegis_operand_base_name(i), op_type);
    comps.push_back(comp);
  }
  symbolt new_symbol;
  new_symbol.name=instr_type_name;
  new_symbol.type=type;
  new_symbol.base_name=instr_type_name;
  new_symbol.pretty_name=instr_type_name;
  new_symbol.location=default_cegis_source_location();
  new_symbol.mode=ID_C;
  new_symbol.module=CEGIS_MODULE;
  new_symbol.is_type=true;
  assert(!st.add(new_symbol));
  return symbol_typet(instr_type_name);
}

const mp_integer get_width(const typet &type)
{
  const irep_idt &id_width=type.get(ID_width);
  assert(!id_width.empty());
  return string2integer(id2string(id_width));
}

code_typet create_func_type(const symbol_tablet &st,
    const symbol_typet &instruction_type)
{
  code_typet code_type;
  code_type.return_type()=empty_typet();
  const typet &followed_type=namespacet(st).follow(instruction_type);
  const struct_union_typet &struct_type=to_struct_union_type(followed_type);
  const struct_union_typet::componentst &comps=struct_type.components();
  const pointer_typet instr_ref_type(instruction_type);
  code_type.parameters().push_back(code_typet::parametert(instr_ref_type));
  const typet size_type(unsigned_char_type());
  code_type.parameters().push_back(code_typet::parametert(size_type));
  return code_type;
}

void add_param(symbol_tablet &st, const std::string &func,
    const char * const name, const typet &type)
{
  symbolt prog_param_symbol;
  prog_param_symbol.name=get_local_meta_name(func, name);
  prog_param_symbol.type=type;
  prog_param_symbol.base_name=name;
  prog_param_symbol.pretty_name=name;
  prog_param_symbol.location=default_cegis_source_location();
  prog_param_symbol.mode=ID_C;
  prog_param_symbol.module=CEGIS_MODULE;
  prog_param_symbol.is_lvalue=true;
  prog_param_symbol.is_thread_local=true;
  prog_param_symbol.is_file_local=true;
  prog_param_symbol.is_parameter=true;
  prog_param_symbol.is_state_var=true;
  assert(!st.add(prog_param_symbol));
}

void add_to_symbol_table(symbol_tablet &st, const std::string &name,
    const goto_functionst::function_mapt::mapped_type &func)
{
  if (st.has_symbol(name)) return;
  symbolt new_symbol;
  new_symbol.name=name;
  new_symbol.type=func.type;
  new_symbol.base_name=name;
  new_symbol.pretty_name=name;
  new_symbol.location=default_cegis_source_location();
  new_symbol.mode=ID_C;
  new_symbol.module=CEGIS_MODULE;
  assert(!st.add(new_symbol));
  const code_typet::parameterst &params=func.type.parameters();
  assert(2 == params.size());
  add_param(st, name, CEGIS_PROC_PROGRAM_PARAM_ID, params.front().type());
  add_param(st, name, CEGIS_PROC_PROGRAM_SIZE_PARAM_ID, params.back().type());
}
}

std::string create_cegis_processor(symbol_tablet &st, goto_functionst &gf,
    const cegis_operand_datat &slots)
{
  for (const std::pair<typet, size_t> &var_slot : slots)
    create_variable_array(st, gf, var_slot.first, var_slot.second);
  const std::string func_name(get_next_processor_name(st));
  const symbol_typet instr_type(create_instruction_type(st, slots, func_name));
  goto_functionst::function_mapt::mapped_type &func=gf.function_map[func_name];
  func.parameter_identifiers.push_back(CEGIS_PROC_PROGRAM_PARAM_ID);
  func.parameter_identifiers.push_back(CEGIS_PROC_PROGRAM_SIZE_PARAM_ID);
  func.type=create_func_type(st, instr_type);
  add_to_symbol_table(st, func_name, func);
  goto_programt &body=func.body;
  generate_processor_body(st, body, func_name, slots);
  // TODO: Implement
  // XXX: Debug
  goto_programt &entry_body=get_entry_body(gf);
  std::string prog(func_name);
  prog+="_prog";
  declare_cegis_program(st, gf, func_name, prog, 3);
  call_processor(st, entry_body, std::prev(entry_body.instructions.end(), 2),
      func_name, prog);
  try
  {
    std::cout << "<create_cegis_processor>" << std::endl;
    st.show(std::cout);
    gf.output(namespacet(st), std::cout);
    std::cout << "</create_cegis_processor>" << std::endl;
  } catch (const std::string &ex)
  {
    std::cout << "<ex>" << ex << "</ex>" << std::endl;
    throw;
  }
  // XXX: Debug
  return func_name;
}
