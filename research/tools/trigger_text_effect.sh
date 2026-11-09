#!/bin/bash
# Shows one of the game's text effect banners (id 1..22, 13 = Sculptor's Idol Found) in a running game, without a
# restart and without touching the save. Calls the game's own request function `0x8C75F0(id, 0)` (RVA, base 0x140000000)
# on a game thread through gdb: stop on the per frame function 0xA36CC0, run the call on that thread with the return
# address set to the same breakpoint, restore the registers, detach.
# Usage: trigger_text_effect.sh <id> [ssh target]      e.g. trigger_text_effect.sh 13 deck@192.168.0.18
# Needs gdb and python3 on the machine that runs the game (Steam Deck: both there). Wine signals are ignored (without
# that SIGUSR1 stops the wrong thread). Do not add watchpoints or leave breakpoints: a killed gdb with a breakpoint
# inserted crashes the game.
ID=${1:?id 1..22}
TARGET=${2:-}
GDB_SCRIPT=$(cat <<EOF
set pagination off
set confirm off
handle SIGUSR1 SIGUSR2 SIGALRM SIGCHLD SIGPIPE nostop noprint pass
break *0x140A36CC0
continue
python
import gdb
pc=int(gdb.parse_and_eval('\$rip'))&0xFFFFFFFFFFFFFFFF
if pc!=0x140A36CC0:
    print("ABORT not at breakpoint, pc=%x"%pc)
    gdb.execute('delete'); gdb.execute('detach'); gdb.execute('quit')
R=['rax','rbx','rcx','rdx','rsi','rdi','rbp','rsp','r8','r9','r10','r11','r12','r13','r14','r15','rip','eflags']
saved={r:int(gdb.parse_and_eval('\$'+r))&0xFFFFFFFFFFFFFFFF for r in R}
T=gdb.selected_thread().num
gdb.execute('delete')
gdb.execute('break *0x140A36CC0 thread %d'%T)
sp=((saved['rsp']-0x400)&~0xF)-8
gdb.selected_inferior().write_memory(sp,(0x140A36CC0).to_bytes(8,'little'))
gdb.execute('set \$rsp=%d'%sp); gdb.execute('set \$rcx=$ID'); gdb.execute('set \$rdx=0'); gdb.execute('set \$rip=0x1408C75F0')
print("HIJACK thread",T)
end
continue
python
for r,v in saved.items():
    gdb.execute('set \$%s=%d'%(r,v))
gdb.execute('delete')
print("RESTORED")
end
detach
EOF
)
RUN='pid=$(pgrep -x sekiro.exe | head -1); [ -n "$pid" ] || { echo "no sekiro.exe"; exit 1; }; f=$(mktemp); cat > $f; timeout -s INT 30 gdb -p $pid -batch -x $f 2>&1 | grep -E "ABORT|HIJACK|RESTORED|rror"; rm -f $f'
if [ -n "$TARGET" ]; then
  printf '%s\n' "$GDB_SCRIPT" | ssh "$TARGET" bash -c "'$RUN'"
else
  printf '%s\n' "$GDB_SCRIPT" | bash -c "$RUN"
fi
