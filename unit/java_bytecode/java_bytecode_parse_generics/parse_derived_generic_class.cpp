/*******************************************************************\

 Module: Unit tests for parsing generic classes

 Author: DiffBlue Limited. All rights reserved.

\*******************************************************************/

#include <testing-utils/catch.hpp>
#include <testing-utils/load_java_class.h>

#include <util/config.h>
#include <util/language.h>
#include <java_bytecode/java_bytecode_language.h>
#include <iostream>

SCENARIO(
  "parse_derived_generic_class",
  "[core][java_bytecode][java_bytecode_parse_generics]")
{
  const symbol_tablet &new_symbol_table = load_java_class(
    "DerivedGeneric", "./java_bytecode/java_bytecode_parse_generics");

  THEN("There should be a symbol for the DerivedGeneric class")
  {
    std::string class_prefix = "java::DerivedGeneric";
    REQUIRE(new_symbol_table.has_symbol(class_prefix));

    const symbolt &derived_symbol = new_symbol_table.lookup_ref(class_prefix);
    derived_symbol.show(std::cout);
    const class_typet &derived_class_type =
      require_symbol::require_complete_class(derived_symbol);

    // TODO(tkiley): Currently we do not support extracting information
    // about the base classes generic information - issue TG-1287
  }
}
