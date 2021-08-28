ld -r -b binary -o ./beepone.o ./beepone;
ld -r -b binary -o ./beeptwo.o ./beeptwo;
ld -r -b binary -o ./beepthree.o ./beepthree;

gcc ./fwifiscan.c ./beepone.o ./beeptwo.o ./beepthree.o -lasound -pthread -lgps -liw -g -o fiwifiscan;