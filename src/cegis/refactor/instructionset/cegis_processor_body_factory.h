/*******************************************************************

 Module: Counterexample-Guided Inductive Synthesis

 Author: Daniel Kroening, kroening@kroening.com
         Pascal Kesseli, pascal.kesseli@cs.ox.ac.uk

 \*******************************************************************/

#ifndef CEGIS_REFACTOR_INSTRUCTIONSET_CEGIS_PROCESSOR_BODY_FACTORY_H_
#define CEGIS_REFACTOR_INSTRUCTIONSET_CEGIS_PROCESSOR_BODY_FACTORY_H_

#include <cegis/refactor/instructionset/operand_data.h>

/**
 * @brief
 *
 * @details
 */
#define CEGIS_PROC_PROGRAM_PARAM_ID "program"

/**
 * @brief
 *
 * @details
 */
#define CEGIS_PROC_PROGRAM_SIZE_PARAM_ID "size"

/**
 * @brief
 *
 * @details
 */
#define CEGIS_PROC_OPCODE_MEMBER_NAME "opcode"

/**
 * @brief
 *
 * @details
 *
 * @param op
 *
 * @return
 */
std::string cegis_operand_base_name(size_t op);

/**
 * @brief
 *
 * @details
 *
 * @param st
 * @param type
 *
 * @return
 */
std::string cegis_operand_array_name(const symbol_tablet &st, const typet &type);

/**
 * @brief
 *
 * @details
 *
 * @param slots
 *
 * @return
 */
size_t cegis_max_operands(const cegis_operand_datat &slots);

/**
 * @brief
 *
 * @details
 *
 * @param st
 * @param body
 * @param name
 * @param slots
 */
void generate_processor_body(
    class symbol_tablet &st,
    class goto_programt &body,
    const std::string &name,
    const cegis_operand_datat &slots);

#endif /* CEGIS_REFACTOR_INSTRUCTIONSET_CEGIS_PROCESSOR_BODY_FACTORY_H_ */
