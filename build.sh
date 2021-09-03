ld -r -b binary -o ./beep.o ./beep;

gcc ./fwifiscan.c ./beep.o -lasound -pthread -lgps -liw -g -o fiwifiscan;