cd vm
make clean
make
cd build
source ../../activate
# pintos-mkdisk filesys.dsk 10
# pintos --fs-disk filesys.dsk -- -q  -threads-tests -f run alarm-single
# pintos --gdb --fs-disk filesys.dsk -- -q  -threads-tests -f run alarm-single
# pintos --fs-disk filesys.dsk -p tests/userprog/close-normal:close-normal -p ../../tests/userprog/sample.txt:sample.txt -- -q -f run close-normal
# pintos --fs-disk filesys.dsk -p tests/userprog/read-normal:read-normal -p ../../tests/userprog/sample.txt:sample.txt -- -f run read-normal
# pintos --gdb --fs-disk filesys.dsk -p tests/userprog/fork-once:fork-once -- -q  -f run fork-once 
# pintos --fs-disk filesys.dsk -p tests/userprog/fork-once:fork-once -- -q  -f run fork-once 

pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/args-none:args-multiple --swap-disk=4 -- -q   -f run 'args-multiple ab cd ef'
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/args-none:args-none --swap-disk=4 -- -q   -f run args-none
