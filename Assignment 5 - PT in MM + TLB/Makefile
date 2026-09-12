# declare compiler
CC = gcc

# enable all warnings
CFLAGS = -Wall

# declare name of target executable
TARGET = lab5

# declare list of object files
OBJS = main.o compiler.o memory.o processor.o os.o

# build target by linking object files
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# build object files by compiling source files
main.o: main.c compiler.h memory.h processor.h os.h
	$(CC) $(CFLAGS) -c main.c

compiler.o: compiler.c compiler.h
	$(CC) $(CFLAGS) -c compiler.c

memory.o: memory.c memory.h
	$(CC) $(CFLAGS) -c memory.c

processor.o: processor.c processor.h memory.h
	$(CC) $(CFLAGS) -c processor.c

os.o: os.c os.h memory.h processor.h compiler.h
	$(CC) $(CFLAGS) -c os.c

# clean target by removing object files and executable
clean:
	rm -f *.o $(TARGET)
