#pragma once

#include "cli.h"

namespace poly {

const CommandSpec* command_table();

int cmd_init(Context& ctx, const Args& args);
int cmd_list(Context& ctx, const Args& args);
int cmd_info(Context& ctx, const Args& args);
int cmd_config(Context& ctx, const Args& args);
int cmd_clean(Context& ctx, const Args& args);
int cmd_doctor(Context& ctx, const Args& args);
int cmd_testlib(Context& ctx, const Args& args);

int cmd_generate(Context& ctx, const Args& args);
int cmd_validate(Context& ctx, const Args& args);
int cmd_build(Context& ctx, const Args& args);
int cmd_run(Context& ctx, const Args& args);
int cmd_test(Context& ctx, const Args& args);
int cmd_stress(Context& ctx, const Args& args);
int cmd_statement(Context& ctx, const Args& args);
int cmd_package(Context& ctx, const Args& args);
int cmd_import(Context& ctx, const Args& args);
int cmd_ui(Context& ctx, const Args& args);
int cmd_help(Context& ctx, const Args& args);

}  // namespace poly
