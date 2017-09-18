target remote :1234
handle SIGSEGV noprint nopass nostop
symbol-file bin/kernel
