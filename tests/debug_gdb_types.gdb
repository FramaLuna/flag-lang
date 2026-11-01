set pagination off
set print address off
set print thread-events off
break main.flg:16
run
ptype Car
ptype Point
ptype Shape
whatis shape
print sizeof(Car)
print shapes[1]
