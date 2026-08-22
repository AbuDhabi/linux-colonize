1. Dosbox-x debugger commands live in dosbox-x-debugger-commands.txt. Some of them are unavailable due to technical reasons.
2. User has found that F10 is unavailable (because it is caught by the terminal, not the debugger) but F11 is.
3. The debugger console view has some peculiarities:
 - Code view can and does view partial instructions; only numbers viewed are disassembled, which means the first and last instruction may be incorrect.
 - The Output pane includes a bunch of crud, if the user even shows it. It can show memory breakpoint changes, like: "DEBUG: Memory breakpoint : <segment>:<offset> - <oldvalue> -> <newvalue>

