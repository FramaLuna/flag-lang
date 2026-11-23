set pagination off
set print address off
set print thread-events off
source lib/flag-gdb.py
break stop
run
up
python text = gdb.default_visualizer(gdb.parse_and_eval("text")).to_string(); print(ascii(bytes(gdb.selected_inferior().read_memory(text.address, text.length)).decode(text.encoding)))
print part
print empty
print numbers
print names
print holders
print none
print wild
print/r numbers
