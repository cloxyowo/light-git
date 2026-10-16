#pragma once

#include "light_git/cli.hpp"

namespace light_git {
namespace commands {

void register_all_commands(CliDispatcher& dispatcher);

// Individual command handlers
int cmd_init(const CommandContext& ctx);
int cmd_status(const CommandContext& ctx);
int cmd_add(const CommandContext& ctx);
int cmd_commit(const CommandContext& ctx);
int cmd_log(const CommandContext& ctx);
int cmd_diff(const CommandContext& ctx);
int cmd_branch(const CommandContext& ctx);
int cmd_switch(const CommandContext& ctx);
int cmd_restore(const CommandContext& ctx);
int cmd_reset(const CommandContext& ctx);
int cmd_show(const CommandContext& ctx);
int cmd_rm(const CommandContext& ctx);
int cmd_mv(const CommandContext& ctx);
int cmd_check_ignore(const CommandContext& ctx);
int cmd_config(const CommandContext& ctx);
int cmd_gc(const CommandContext& ctx);
int cmd_fsck(const CommandContext& ctx);
int cmd_version(const CommandContext& ctx);
int cmd_tag(const CommandContext& ctx);
int cmd_stash(const CommandContext& ctx);
int cmd_merge(const CommandContext& ctx);
int cmd_cherry_pick(const CommandContext& ctx);
int cmd_export_git(const CommandContext& ctx);
int cmd_future_stub(const CommandContext& ctx);

} // namespace commands
} // namespace light_git
