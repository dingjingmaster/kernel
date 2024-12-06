# if_changed(6.12.0版本)

## if_changed 定义

```makefile
# 如果 if-changed-cond 中有值则执行 cmd_and_savecmd, 否则什么都不做
if_changed = $(if $(if-changed-cond), $(cmd_and_savecmd), @:)

if-changed-cond = $(newer-prereqs)$(cmd-check)$(check-FORCE)

# '$?'是Makefile中的自动变量, 表示比目标文件更新的所有依赖文件,
# 取这些文件并去掉 $(PHONY) 目标
newer-prereqs = $(filter-out $(PHONY), $?)

# 使用默认配置
# 检查某个变量(如: savecmd_$@)是否为空或只包含空白字符.
#  savecmd_foo.o = gcc -o foo.o foo.c
#  '$@' 表示当前目标文件的名称
cmd-check = $(if $(strip $(savedcmd_$@)),,1)

check-FORCE = $(if $(filter FORCE, $^),, $(warning FORCE prerequisite is missing))

cmd = @$(if $(cmd_$(1)), set -e; $($(quiet)log_print) $(delete-on-interrupt) $(cmd_$(1)),:)

delete-on-interrupt = \
    $(if $(filter-out $(PHONY), $@), \
        $(foreach sig, HUP INT QUIT TERM PIPE, \
            trap 'rm -f $@; trap - $(sig); kill -s $(sig) $$$$' $(sig);))

# savedcmd_$@ := $(make-cmd)
cmd_and_savecmd = $(cmd); printf '%s\n' 'savedcmd_$@ := $(make-cmd)' > $(dot-target).cmd

# 处理 $(cmd_$(1))中的变量, 将转义后的字符串输出到 xxx.cmd 中
make-cmd = $(call escsq, $(subst $(pound), $$(pound), $(subst $$,$$$$,$(cmd_$(1)))))
escsq = $(subst $(squote), '\$(squote)', $1)

if_changed_dep = $(if $(if-changed-cond),$(cmd_and_fixdep), @:)

cmd_and_fixdep = $(cmd); scripts/basic/fixdep $(depfile) $@ '$(make-cmd)' > $(dot-target).cmd; rm -f $(depfile)

if_changed_rule = $(if $(if-changed-cond), $(rule_$(1)), @:)

# 将一个文件 'build/obj/foo.o' 变为 'build/obj/.foo.o'
dot-target = $(dir $@).$(notdir $@)
```

归根结底, 这个函数的功能主要就是执行 `$(cmd_$(1))`
